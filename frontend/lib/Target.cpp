#include "XAI/XAIDialect.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Target/LLVMIR/ModuleTranslation.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

using namespace mlir;
using namespace mlir::xai;

namespace {

std::string gDeviceLlc;

void append16(std::vector<uint8_t> &out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value));
  out.push_back(static_cast<uint8_t>(value >> 8));
}

void append32(std::vector<uint8_t> &out, uint32_t value) {
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

void append64(std::vector<uint8_t> &out, uint64_t value) {
  for (int i = 0; i < 8; ++i)
    out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

void alignTo(std::vector<uint8_t> &out, size_t align) {
  while (out.size() % align)
    out.push_back(0);
}

void appendShdr(std::vector<uint8_t> &out, uint32_t name, uint32_t type,
                uint64_t flags, uint64_t offset, uint64_t size, uint32_t link,
                uint32_t info, uint64_t addralign, uint64_t entsize) {
  append32(out, name);
  append32(out, type);
  append64(out, flags);
  append64(out, 0);
  append64(out, offset);
  append64(out, size);
  append32(out, link);
  append32(out, info);
  append64(out, addralign);
  append64(out, entsize);
}

// Relocatable ELF64 container. The assembly text lives in .xai_isa. This is
// not an instruction encoding: XAI has no MCCodeEmitter.
std::vector<uint8_t> wrapAssemblyInElf(StringRef assembly,
                                       ArrayRef<StringRef> symbols) {
  std::string shstr(1, '\0');
  auto addName = [&](StringRef name) -> uint32_t {
    uint32_t offset = static_cast<uint32_t>(shstr.size());
    shstr.append(name.str());
    shstr.push_back('\0');
    return offset;
  };
  const uint32_t isaName = addName(".xai_isa");
  const uint32_t strName = addName(".strtab");
  const uint32_t symName = addName(".symtab");
  const uint32_t shstrName = addName(".shstrtab");
  std::string strtab(1, '\0');
  SmallVector<uint32_t> nameOffs;
  for (StringRef symbol : symbols) {
    nameOffs.push_back(static_cast<uint32_t>(strtab.size()));
    strtab.append(symbol.str());
    strtab.push_back('\0');
  }

  std::vector<uint8_t> elf(64, 0);
  elf[0] = 0x7f;
  elf[1] = 'E';
  elf[2] = 'L';
  elf[3] = 'F';
  elf[4] = 2;
  elf[5] = 1;
  elf[6] = 1;

  const uint64_t isaOff = 64;
  elf.insert(elf.end(), assembly.begin(), assembly.end());
  const uint64_t isaSize = assembly.size();

  alignTo(elf, 8);
  const uint64_t strOff = elf.size();
  elf.insert(elf.end(), strtab.begin(), strtab.end());

  alignTo(elf, 8);
  const uint64_t symOff = elf.size();
  const uint64_t symCount = static_cast<uint64_t>(symbols.size()) + 1;
  for (uint64_t i = 0; i < symCount * 24; ++i)
    elf.push_back(0);
  auto patch = [&](size_t at, auto write) {
    std::vector<uint8_t> tmp;
    write(tmp);
    for (size_t i = 0; i < tmp.size(); ++i)
      elf[at + i] = tmp[i];
  };
  for (size_t i = 0; i < symbols.size(); ++i) {
    size_t at = symOff + (i + 1) * 24;
    patch(at, [&](std::vector<uint8_t> &tmp) {
      append32(tmp, nameOffs[i]);
      tmp.push_back(0x12);
      tmp.push_back(0);
      append16(tmp, 1);
      append64(tmp, 0);
      append64(tmp, isaSize);
    });
  }

  alignTo(elf, 8);
  const uint64_t shstrOff = elf.size();
  elf.insert(elf.end(), shstr.begin(), shstr.end());

  alignTo(elf, 8);
  const uint64_t shOff = elf.size();
  appendShdr(elf, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  appendShdr(elf, isaName, 1, 2, isaOff, isaSize, 0, 0, 1, 0);
  appendShdr(elf, strName, 3, 0, strOff, strtab.size(), 0, 0, 1, 0);
  appendShdr(elf, symName, 2, 0, symOff, symCount * 24, 2, 1, 8, 24);
  appendShdr(elf, shstrName, 3, 0, shstrOff, shstr.size(), 0, 0, 1, 0);

  auto patchHeader = [&](size_t at, auto write) {
    std::vector<uint8_t> tmp;
    write(tmp);
    for (size_t i = 0; i < tmp.size(); ++i)
      elf[at + i] = tmp[i];
  };
  patchHeader(16, [](std::vector<uint8_t> &tmp) { append16(tmp, 1); });
  patchHeader(18, [](std::vector<uint8_t> &tmp) { append16(tmp, 0); });
  patchHeader(20, [](std::vector<uint8_t> &tmp) { append32(tmp, 1); });
  patchHeader(40, [&](std::vector<uint8_t> &tmp) { append64(tmp, shOff); });
  patchHeader(52, [](std::vector<uint8_t> &tmp) { append16(tmp, 64); });
  patchHeader(58, [](std::vector<uint8_t> &tmp) { append16(tmp, 64); });
  patchHeader(60, [](std::vector<uint8_t> &tmp) { append16(tmp, 5); });
  patchHeader(62, [](std::vector<uint8_t> &tmp) { append16(tmp, 4); });
  return elf;
}

std::string sanitizeDeviceIR(std::string ir) {
  auto eraseToken = [&](StringRef token) {
    std::string needle = token.str();
    for (size_t at = 0; (at = ir.find(needle, at)) != std::string::npos;)
      ir.erase(at, needle.size());
  };
  eraseToken(" noundef");
  eraseToken(" samesign");
  eraseToken(" disjoint");
  std::string nuw = " nuw";
  for (size_t at = 0; (at = ir.find(nuw, at)) != std::string::npos;)
    ir.erase(at, nuw.size());
  for (size_t at = 0; (at = ir.find("range(", at)) != std::string::npos;) {
    size_t end = ir.find(')', at);
    if (end == std::string::npos)
      break;
    ir.erase(at, end - at + 1);
  }
  return ir;
}

LogicalResult runXAILlc(StringRef llPath, StringRef asmPath, Operation *errorOp) {
  if (gDeviceLlc.empty()) {
    return errorOp->emitError(
        "XAI llc path is empty; pass --xai-llc or build with XAI_LLC_DEFAULT");
  }
  if (!llvm::sys::fs::exists(gDeviceLlc)) {
    return errorOp->emitError("XAI llc not found at '" + gDeviceLlc +
                              "'; run ./scripts/build_llc.sh");
  }
  SmallString<128> errFile;
  std::error_code ec =
      llvm::sys::fs::createTemporaryFile("xai-llc", "err", errFile);
  if (ec)
    return errorOp->emitError("cannot create llc stderr file");

  std::optional<StringRef> redirects[3] = {std::nullopt, std::nullopt,
                                           StringRef(errFile)};
  std::string err;
  SmallVector<StringRef> args = {gDeviceLlc, "-mtriple=xai", "-filetype=asm",
                                 llPath, "-o", asmPath};
  int rc = llvm::sys::ExecuteAndWait(gDeviceLlc, args, std::nullopt, redirects,
                                     /*SecondsToWait=*/0, /*MemoryLimit=*/0,
                                     &err);
  std::string llcErr;
  if (auto buf = llvm::MemoryBuffer::getFile(errFile))
    llcErr = buf.get()->getBuffer().str();
  llvm::sys::fs::remove(errFile);
  if (rc != 0) {
    return errorOp->emitError("llc -mtriple=xai failed: ")
           << err << "\n" << llcErr;
  }
  return success();
}

LogicalResult buildDeviceModule(gpu::GPUModuleOp gpuModule,
                                OwningOpRef<ModuleOp> &device) {
  MLIRContext *ctx = gpuModule.getContext();
  device = ModuleOp::create(gpuModule.getLoc());
  OpBuilder builder(device->getContext());
  builder.setInsertionPointToEnd(device->getBody());
  Location loc = gpuModule.getLoc();
  auto ptrTy = LLVM::LLVMPointerType::get(ctx);
  auto tileF16 = VectorType::get({256}, builder.getF16Type());
  auto tileF32 = VectorType::get({256}, builder.getF32Type());
  auto voidTy = LLVM::LLVMVoidType::get(ctx);

  auto decl = [&](StringRef name, Type result, ArrayRef<Type> args) {
    auto type = LLVM::LLVMFunctionType::get(result, args);
    builder.setInsertionPointToStart(device->getBody());
    builder.create<LLVM::LLVMFuncOp>(loc, name, type);
  };
  decl("llvm.xai.mload.f16", tileF16, {ptrTy});
  decl("llvm.xai.mload.f32", tileF32, {ptrTy});
  decl("llvm.xai.mma", tileF32, {tileF16, tileF16, tileF32});
  decl("llvm.xai.mstore.f32", voidTy, {ptrTy, tileF32});

  bool sawKernel = false;
  for (gpu::GPUFuncOp func : gpuModule.getOps<gpu::GPUFuncOp>()) {
    if (!func.isKernel())
      continue;
    sawKernel = true;
    SmallVector<Type> argTypes;
    for (Type type : func.getArgumentTypes()) {
      if (isa<MemRefType>(type))
        argTypes.push_back(ptrTy);
      else if (isa<IndexType>(type))
        argTypes.push_back(builder.getI64Type());
      else
        argTypes.push_back(type);
    }
    auto fnType = LLVM::LLVMFunctionType::get(voidTy, argTypes);
    builder.setInsertionPointToEnd(device->getBody());
    auto llvmFn =
        builder.create<LLVM::LLVMFuncOp>(func.getLoc(), func.getName(), fnType);
    Block *entry = llvmFn.addEntryBlock(builder);
    builder.setInsertionPointToStart(entry);

    IRMapping mapping;
    for (auto [oldArg, newArg] :
         llvm::zip(func.getArguments(), entry->getArguments()))
      mapping.map(oldArg, newArg);

    for (Operation &op : func.getBody().front().without_terminator()) {
      // Outlining always materializes block/thread ids. This kernel does not
      // use them; only the pointer chain and the llvm.xai calls are lowered.
      if (isa<gpu::GPUDialect>(op.getDialect()))
        continue;
      if (isa<memref::ExtractAlignedPointerAsIndexOp, arith::IndexCastOp>(op))
        continue;
      if (auto toPtr = dyn_cast<LLVM::IntToPtrOp>(op)) {
        auto castOp = toPtr.getArg().getDefiningOp<arith::IndexCastOp>();
        auto ext = castOp
                       ? castOp.getIn().getDefiningOp<
                             memref::ExtractAlignedPointerAsIndexOp>()
                       : nullptr;
        if (!ext || !mapping.contains(ext.getSource()))
          return func.emitError("cannot map llvm.inttoptr back to a kernel argument");
        mapping.map(toPtr.getResult(), mapping.lookup(ext.getSource()));
        continue;
      }
      auto call = dyn_cast<LLVM::CallOp>(op);
      if (!call || !call.getCallee())
        return func.emitError("expected llvm.xai calls in the kernel, found ")
               << op.getName();
      SmallVector<Type> argTypesForCall;
      SmallVector<Value> args;
      for (Value arg : call.getOperands()) {
        if (!mapping.contains(arg))
          return call.emitError("call operand was not lowered");
        Value mapped = mapping.lookup(arg);
        args.push_back(mapped);
        argTypesForCall.push_back(mapped.getType());
      }
      Type result = call.getNumResults() == 0 ? voidTy : call->getResult(0).getType();
      auto calleeType = LLVM::LLVMFunctionType::get(result, argTypesForCall);
      auto newCall = builder.create<LLVM::CallOp>(call.getLoc(), calleeType,
                                                  call.getCalleeAttr(), args);
      if (call.getNumResults() == 1)
        mapping.map(call->getResult(0), newCall->getResult(0));
    }
    builder.create<LLVM::ReturnOp>(func.getLoc(), ValueRange{});
  }
  if (!sawKernel)
    return gpuModule.emitError("gpu.module has no kernel function");
  return success();
}

struct XAITargetModel
    : public gpu::TargetAttrInterface::ExternalModel<XAITargetModel, TargetAttr> {
  std::optional<gpu::SerializedObject>
  serializeToObject(Attribute attribute, Operation *module,
                    const gpu::TargetOptions &options) const {
    auto gpuModule = dyn_cast<gpu::GPUModuleOp>(module);
    if (!gpuModule) {
      module->emitError("serializeToObject expected a gpu.module");
      return std::nullopt;
    }

    OwningOpRef<ModuleOp> device;
    if (failed(buildDeviceModule(gpuModule, device)))
      return std::nullopt;
    if (failed(device->verify()))
      return std::nullopt;

    llvm::LLVMContext llvmCtx;
    auto llvmModule = translateModuleToLLVMIR(*device, llvmCtx, "xai-device");
    if (!llvmModule) {
      gpuModule.emitError("failed to translate the XAI kernel to LLVM IR");
      return std::nullopt;
    }

    std::string ir;
    llvm::raw_string_ostream irStream(ir);
    llvmModule->print(irStream, nullptr);
    irStream.flush();
    ir = sanitizeDeviceIR(std::move(ir));

    SmallString<128> llPath, asmPath;
    if (llvm::sys::fs::createTemporaryFile("xai-device", "ll", llPath) ||
        llvm::sys::fs::createTemporaryFile("xai-device", "s", asmPath)) {
      gpuModule.emitError("cannot create temporary files for XAI llc");
      return std::nullopt;
    }
    {
      std::error_code ec;
      llvm::raw_fd_ostream os(llPath, ec);
      if (ec) {
        gpuModule.emitError("cannot write device LLVM IR");
        return std::nullopt;
      }
      os << ir;
    }
    if (failed(runXAILlc(llPath, asmPath, gpuModule))) {
      llvm::sys::fs::remove(llPath);
      llvm::sys::fs::remove(asmPath);
      return std::nullopt;
    }
    auto asmBuf = llvm::MemoryBuffer::getFile(asmPath);
    llvm::sys::fs::remove(llPath);
    llvm::sys::fs::remove(asmPath);
    if (!asmBuf) {
      gpuModule.emitError("cannot read XAI assembly");
      return std::nullopt;
    }
    StringRef assembly = asmBuf.get()->getBuffer();
    if (!assembly.contains("xai.mma")) {
      gpuModule.emitError("XAI assembly does not contain xai.mma");
      return std::nullopt;
    }

    SmallVector<char> bytes;
    gpu::CompilationTarget format = options.getCompilationTarget();
    if (format == gpu::CompilationTarget::Assembly) {
      bytes.append(assembly.begin(), assembly.end());
    } else {
      SmallVector<StringRef> symbols;
      for (gpu::GPUFuncOp func : gpuModule.getOps<gpu::GPUFuncOp>()) {
        if (func.isKernel())
          symbols.push_back(func.getName());
      }
      std::vector<uint8_t> elf = wrapAssemblyInElf(assembly, symbols);
      bytes.append(elf.begin(), elf.end());
    }
    return gpu::SerializedObject(std::move(bytes));
  }

  Attribute createObject(Attribute attribute, Operation *module,
                         const gpu::SerializedObject &object,
                         const gpu::TargetOptions &options) const {
    auto target = cast<TargetAttr>(attribute);
    std::string bytes(object.getObject().begin(), object.getObject().end());
    gpu::CompilationTarget format = options.getCompilationTarget();
    if (format == gpu::CompilationTarget::Fatbin ||
        format == gpu::CompilationTarget::Offload)
      format = gpu::CompilationTarget::Binary;
    return gpu::ObjectAttr::get(target, format,
                                StringAttr::get(target.getContext(), bytes));
  }
};

llvm::Value *asI64(llvm::IRBuilderBase &builder, llvm::Value *value) {
  llvm::Type *i64 = llvm::Type::getInt64Ty(builder.getContext());
  if (!value)
    return nullptr;
  if (value->getType() == i64)
    return value;
  if (value->getType()->isIntegerTy())
    return builder.CreateIntCast(value, i64, /*isSigned=*/false);
  return nullptr;
}

struct XAIOffloadModel
    : public gpu::OffloadingLLVMTranslationAttrInterface::ExternalModel<
          XAIOffloadModel, TargetAttr> {
  llvm::LogicalResult
  embedBinary(Attribute, Operation *binaryOp, llvm::IRBuilderBase &hostBuilder,
              LLVM::ModuleTranslation &hostModuleTranslation) const {
    auto binary = dyn_cast<gpu::BinaryOp>(binaryOp);
    if (!binary || binary.getObjects().empty())
      return llvm::failure();
    auto object = dyn_cast<gpu::ObjectAttr>(binary.getObjects()[0]);
    if (!object)
      return llvm::failure();
    StringRef data = object.getObject().getValue();
    llvm::Module *module = hostModuleTranslation.getLLVMModule();
    llvm::LLVMContext &ctx = module->getContext();
    llvm::ArrayType *arrayType =
        llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx), data.size());
    llvm::Constant *init = llvm::ConstantDataArray::get(
        ctx, llvm::ArrayRef<uint8_t>(
                 reinterpret_cast<const uint8_t *>(data.data()), data.size()));
    auto *global = new llvm::GlobalVariable(
        *module, arrayType, /*isConstant=*/true,
        llvm::GlobalValue::InternalLinkage, init,
        (Twine("__xai_offload_") + binary.getSymName()).str());
    global->setSection(".xai_offload");
    global->setAlignment(llvm::Align(8));
    (void)hostBuilder;
    return llvm::success();
  }

  llvm::LogicalResult
  launchKernel(Attribute, Operation *launchOp, Operation *binaryOp,
               llvm::IRBuilderBase &hostBuilder,
               LLVM::ModuleTranslation &hostModuleTranslation) const {
    auto launch = dyn_cast<gpu::LaunchFuncOp>(launchOp);
    auto binary = dyn_cast<gpu::BinaryOp>(binaryOp);
    if (!launch || !binary)
      return llvm::failure();
    llvm::Module *module = hostModuleTranslation.getLLVMModule();
    llvm::GlobalVariable *global = module->getGlobalVariable(
        (Twine("__xai_offload_") + binary.getSymName()).str());
    if (!global)
      return llvm::failure();

    llvm::LLVMContext &ctx = module->getContext();
    llvm::Type *ptrTy = llvm::PointerType::getUnqual(ctx);
    llvm::Type *i64 = llvm::Type::getInt64Ty(ctx);
    StringRef kernelName = launch.getKernel().getLeafReference().getValue();
    auto *nameInit =
        llvm::ConstantDataArray::getString(ctx, kernelName, /*AddNull=*/true);
    auto *nameGlobal = new llvm::GlobalVariable(
        *module, nameInit->getType(), /*isConstant=*/true,
        llvm::GlobalValue::InternalLinkage, nameInit,
        (Twine("__xai_kernel_") + kernelName).str());

    llvm::Value *gridX = asI64(hostBuilder, hostModuleTranslation.lookupValue(launch.getGridSizeX()));
    llvm::Value *gridY = asI64(hostBuilder, hostModuleTranslation.lookupValue(launch.getGridSizeY()));
    llvm::Value *gridZ = asI64(hostBuilder, hostModuleTranslation.lookupValue(launch.getGridSizeZ()));
    llvm::Value *blockX = asI64(hostBuilder, hostModuleTranslation.lookupValue(launch.getBlockSizeX()));
    llvm::Value *blockY = asI64(hostBuilder, hostModuleTranslation.lookupValue(launch.getBlockSizeY()));
    llvm::Value *blockZ = asI64(hostBuilder, hostModuleTranslation.lookupValue(launch.getBlockSizeZ()));
    if (!gridX || !gridY || !gridZ || !blockX || !blockY || !blockZ)
      return llvm::failure();

    llvm::FunctionType *fnType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(ctx),
        {ptrTy, i64, ptrTy, i64, i64, i64, i64, i64, i64}, /*isVarArg=*/false);
    llvm::Function *fn = module->getFunction("xai_launch_kernel");
    if (!fn) {
      fn = llvm::Function::Create(fnType, llvm::GlobalValue::ExternalLinkage,
                                  "xai_launch_kernel", module);
    }
    llvm::Value *binPtr = hostBuilder.CreateBitCast(global, ptrTy);
    llvm::Value *binSize =
        llvm::ConstantInt::get(i64, global->getValueType()->getArrayNumElements());
    llvm::Value *namePtr = hostBuilder.CreateBitCast(nameGlobal, ptrTy);
    hostBuilder.CreateCall(fnType, fn,
                           {binPtr, binSize, namePtr, gridX, gridY, gridZ,
                            blockX, blockY, blockZ});
    return llvm::success();
  }
};

} // namespace

void mlir::xai::setXAIDeviceLlcPath(StringRef path) { gDeviceLlc = path.str(); }

void mlir::xai::registerXAITargetInterfaceExternalModels(DialectRegistry &registry) {
  registry.addExtension(+[](MLIRContext *context, XAIDialect *) {
    TargetAttr::attachInterface<XAITargetModel>(*context);
    TargetAttr::attachInterface<XAIOffloadModel>(*context);
  });
}

void mlir::xai::registerXAITargetInterfaceExternalModels(MLIRContext &context) {
  DialectRegistry registry;
  registerXAITargetInterfaceExternalModels(registry);
  context.appendDialectRegistry(registry);
}
