#ifndef XAI_PIPELINE_H
#define XAI_PIPELINE_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/StringRef.h"

#include <string>

namespace mlir::xai {

enum class StopAfter {
  Rewrite,
  Outline,
  Binary,
  Llvm,
  Elf,
};

struct PipelineOptions {
  StopAfter stopAfter = StopAfter::Elf;
  bool dumpIr = false;
  std::string xaiLlc;
  std::string hostLlc;
  std::string outputPath;
  std::string deviceElfPath;
};

StopAfter parseStopAfter(StringRef name, bool &ok);

/// Run the offload pipeline. Stages after `stopAfter` are skipped.
/// On `Elf`, writes a host object file to `options.outputPath`.
/// On earlier stages, writes MLIR (or LLVM IR for `Llvm`) there when the path
/// is non-empty, and always prints that IR to stdout.
LogicalResult runXAIPipeline(ModuleOp module, const PipelineOptions &options);

} // namespace mlir::xai

#endif // XAI_PIPELINE_H
