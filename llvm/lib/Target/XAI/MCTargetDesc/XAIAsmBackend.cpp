//===-- XAIAsmBackend.cpp - XAI assembler backend -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// Assembly emission constructs an object writer even when the output is text.
// A minimal ELF writer satisfies that. Relocations and .o emission stay
// unimplemented: there is no MCCodeEmitter, and getRelocType refuses fixups.
class XAIELFObjectWriter : public MCELFObjectTargetWriter {
public:
  XAIELFObjectWriter()
      : MCELFObjectTargetWriter(/*Is64Bit=*/true, ELF::ELFOSABI_NONE,
                                ELF::EM_NONE, /*HasRelocationAddend=*/true) {}

protected:
  unsigned getRelocType(const MCFixup &, const MCValue &,
                        bool) const override {
    report_fatal_error("XAI object emission is not implemented");
  }
};

class XAIAsmBackend : public MCAsmBackend {
public:
  XAIAsmBackend() : MCAsmBackend(endianness::little) {}

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return std::make_unique<XAIELFObjectWriter>();
  }

  void applyFixup(const MCFragment &, const MCFixup &, const MCValue &,
                  uint8_t *, uint64_t, bool) override {}

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *) const override {
    for (uint64_t I = 0; I < Count; ++I)
      OS.write('\0');
    return true;
  }
};

} // namespace

MCAsmBackend *llvm::createXAIMCAsmBackend(const Target &, const MCSubtargetInfo &,
                                         const MCRegisterInfo &,
                                         const MCTargetOptions &) {
  return new XAIAsmBackend();
}
