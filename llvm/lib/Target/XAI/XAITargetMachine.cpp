//===-- XAITargetMachine.cpp - Define TargetMachine for XAI ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAITargetMachine.h"
#include "XAI.h"
#include "TargetInfo/XAITargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeXAITarget() {
  RegisterTargetMachine<XAITargetMachine> X(getTheXAITarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeXAIAsmPrinterPass(PR);
  initializeXAIDAGToDAGISelLegacyPass(PR);
}

static StringRef defaultCPU(StringRef CPU) {
  return CPU.empty() ? "generic" : CPU;
}

XAITargetMachine::XAITargetMachine(const Target &T, const Triple &TT,
                                   StringRef CPU, StringRef FS,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM,
                                   std::optional<CodeModel::Model> CM,
                                   CodeGenOptLevel OL, bool)
    : CodeGenTargetMachineImpl(
          T,
          "e-m:e-p:64:64-i64:64-i32:32-i16:16-i8:8-f64:64-f32:32-f16:16-n64-"
          "S8192",
          TT, defaultCPU(CPU), FS, Options, RM.value_or(Reloc::Static),
          getEffectiveCodeModel(CM, CodeModel::Small), OL),
      Subtarget(TT, defaultCPU(CPU), FS, *this),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) {
  initAsmInfo();
}

XAITargetMachine::~XAITargetMachine() = default;

namespace {

class XAIPassConfig : public TargetPassConfig {
public:
  XAIPassConfig(XAITargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {
    if (TM.getOptLevel() != CodeGenOptLevel::None)
      substitutePass(&PostRASchedulerID, &PostMachineSchedulerID);
  }

  XAITargetMachine &getXAITargetMachine() const {
    return getTM<XAITargetMachine>();
  }

  bool addInstSelector() override;
};

} // namespace

TargetPassConfig *XAITargetMachine::createPassConfig(PassManagerBase &PM) {
  return new XAIPassConfig(*this, PM);
}

bool XAIPassConfig::addInstSelector() {
  addPass(createXAIISelDag(getXAITargetMachine(), getOptLevel()));
  return false;
}
