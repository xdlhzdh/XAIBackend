//===-- XAIInstrInfo.cpp - XAI Instruction Information --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIInstrInfo.h"
#include "XAI.h"
#include "XAISubtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "XAIGenInstrInfo.inc"

XAIInstrInfo::XAIInstrInfo(const XAISubtarget &STI)
    : XAIGenInstrInfo(STI, RegInfo), RegInfo() {}

static unsigned stackOpcode(const TargetRegisterClass *RC, bool IsStore) {
  if (RC == &XAI::GPRRegClass)
    return IsStore ? XAI::XAI_ST : XAI::XAI_LD;
  if (RC == &XAI::MRRegClass)
    return IsStore ? XAI::XAI_MSTORE_F32 : XAI::XAI_MLOAD_F32;
  llvm_unreachable("unexpected XAI register class");
}

void XAIInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI, const DebugLoc &DL,
                              Register DestReg, Register SrcReg, bool KillSrc,
                              bool RenamableDest, bool RenamableSrc) const {
  unsigned Opcode;
  if (XAI::GPRRegClass.contains(DestReg) && XAI::GPRRegClass.contains(SrcReg))
    Opcode = XAI::XAI_MOV;
  else if (XAI::MRRegClass.contains(DestReg) &&
           XAI::MRRegClass.contains(SrcReg))
    Opcode = XAI::XAI_MMOV;
  else
    report_fatal_error("XAI copyPhysReg: unsupported registers");

  MachineInstrBuilder MIB = BuildMI(MBB, MI, DL, get(Opcode), DestReg)
                                .addReg(SrcReg, getKillRegState(KillSrc));
  if (RenamableDest)
    MIB->getOperand(0).setIsRenamable(true);
  if (RenamableSrc)
    MIB->getOperand(1).setIsRenamable(true);
}

void XAIInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC, Register,
    MachineInstr::MIFlag Flags) const {
  BuildMI(MBB, MI, DebugLoc(), get(stackOpcode(RC, /*IsStore=*/true)))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlags(Flags);
}

void XAIInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register, unsigned,
    MachineInstr::MIFlag Flags) const {
  BuildMI(MBB, MI, DebugLoc(), get(stackOpcode(RC, /*IsStore=*/false)), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlags(Flags);
}

std::optional<DestSourcePair>
XAIInstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.getOpcode() != XAI::XAI_MOV && MI.getOpcode() != XAI::XAI_MMOV)
    return std::nullopt;
  return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
}

static bool isCondBranch(unsigned Opcode) {
  switch (Opcode) {
  case XAI::XAI_BEQ:
  case XAI::XAI_BNE:
  case XAI::XAI_BLTU:
  case XAI::XAI_BGEU:
    return true;
  default:
    return false;
  }
}

bool XAIInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                 MachineBasicBlock *&TBB,
                                 MachineBasicBlock *&FBB,
                                 SmallVectorImpl<MachineOperand> &Cond,
                                 bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  if (MBB.empty())
    return false;

  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUnpredicatedTerminator(*I))
      break;
    if (!I->isBranch())
      return true;

    if (I->getOpcode() == XAI::XAI_BR) {
      if (!AllowModify) {
        TBB = I->getOperand(0).getMBB();
        continue;
      }

      MBB.erase(std::next(I), MBB.end());
      Cond.clear();
      FBB = nullptr;

      if (MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
        TBB = nullptr;
        I->eraseFromParent();
        I = MBB.end();
        continue;
      }

      TBB = I->getOperand(0).getMBB();
      continue;
    }

    if (!isCondBranch(I->getOpcode()))
      return true;
    if (!Cond.empty())
      return true;

    FBB = TBB;
    TBB = I->getOperand(2).getMBB();
    Cond.push_back(MachineOperand::CreateImm(I->getOpcode()));
    Cond.push_back(I->getOperand(0));
    Cond.push_back(I->getOperand(1));
  }
  return false;
}

unsigned XAIInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != XAI::XAI_BR && !isCondBranch(I->getOpcode()))
      break;
    I->eraseFromParent();
    I = MBB.end();
    ++Count;
  }
  if (BytesRemoved)
    *BytesRemoved = static_cast<int>(Count) * 4;
  return Count;
}

unsigned XAIInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL,
                                    int *BytesAdded) const {
  assert(TBB && "insertBranch requires a destination");
  unsigned Count = 0;

  if (Cond.empty()) {
    assert(!FBB && "unconditional branch has one destination");
    BuildMI(&MBB, DL, get(XAI::XAI_BR)).addMBB(TBB);
    Count = 1;
  } else {
    assert(Cond.size() == 3 && "XAI condition is opcode plus two registers");
    unsigned Opcode = Cond[0].getImm();
    BuildMI(&MBB, DL, get(Opcode)).add(Cond[1]).add(Cond[2]).addMBB(TBB);
    Count = 1;
    if (FBB) {
      BuildMI(&MBB, DL, get(XAI::XAI_BR)).addMBB(FBB);
      Count = 2;
    }
  }

  if (BytesAdded)
    *BytesAdded = static_cast<int>(Count) * 4;
  return Count;
}

bool XAIInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 3 && "XAI condition is opcode plus two registers");
  unsigned NewOpcode;
  switch (Cond[0].getImm()) {
  case XAI::XAI_BEQ:
    NewOpcode = XAI::XAI_BNE;
    break;
  case XAI::XAI_BNE:
    NewOpcode = XAI::XAI_BEQ;
    break;
  case XAI::XAI_BLTU:
    NewOpcode = XAI::XAI_BGEU;
    break;
  case XAI::XAI_BGEU:
    NewOpcode = XAI::XAI_BLTU;
    break;
  default:
    return true;
  }
  Cond[0].setImm(NewOpcode);
  return false;
}
