#ifndef XAI_OPS_TD
#define XAI_OPS_TD

include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/OpBase.td"
include "mlir/Dialect/GPU/IR/CompilationAttrInterfaces.td"

def XAI_Dialect : Dialect {
  let name = "xai";
  let cppNamespace = "::mlir::xai";
  let summary = "XAI GPU offload target";
  let description = [{
    `#xai.target` compiles a `gpu.module` with the XAI LLVM backend.
    The attribute implements `gpu::TargetAttrInterface` for
    `gpu-module-to-binary`, and can also act as an offloading handler.
  }];
  let useDefaultAttributePrinterParser = 1;
}

class XAI_Attr<string attrName, string attrMnemonic, list<Trait> traits = []>
    : AttrDef<XAI_Dialect, attrName, traits> {
  let mnemonic = attrMnemonic;
}

// TargetAttrInterface is attached as an external model. The offloading trait
// lets this attribute be stored in `gpu.binary`'s offloading handler.
def XAI_TargetAttr : XAI_Attr<"Target", "target", [
      OffloadingTranslationAttrTrait
    ]> {
  let summary = "XAI device target";
  let description = [{
    Example: `gpu.module @kernels <#gpu.select_object> [#xai.target<"generic">]`.
  }];
  let parameters = (ins
    StringRefParameter<"Target chip. The XAI backend only has `generic`.">:$chip
  );
  let assemblyFormat = "`<` $chip `>`";
}

#endif // XAI_OPS_TD
