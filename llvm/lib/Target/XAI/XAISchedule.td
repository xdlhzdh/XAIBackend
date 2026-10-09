//===-- XAISchedule.td - XAI scheduling model ---------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// In-order model used by the pre-RA and post-RA machine schedulers.
// Latencies are relative teaching costs, not measured cycles. The pipe issues
// one instruction per cycle; a result stays unread until its latency elapses,
// so an independent GPR op can fill the gap between a matrix load and xai.mma.

def XAIModel : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0;
  let LoadLatency = 4;
  let HighLatency = 4;
  let MispredictPenalty = 0;
  let PostRAScheduler = 1;
  let CompleteModel = 1;
}

let SchedModel = XAIModel in {
  def XAIPipe : ProcResource<1>;

  def XAIWriteGPR : SchedWriteRes<[XAIPipe]> { let Latency = 1; }
  def XAIWriteMLoad : SchedWriteRes<[XAIPipe]> { let Latency = 4; }
  def XAIWriteMMA : SchedWriteRes<[XAIPipe]> { let Latency = 4; }
  def XAIWriteMStore : SchedWriteRes<[XAIPipe]> { let Latency = 2; }

  def : InstRW<[XAIWriteGPR],
               (instrs XAI_ADD, XAI_ADDI, XAI_LI, XAI_MOV, XAI_MMOV, XAI_LD,
                       XAI_ST, XAI_BR, XAI_BEQ, XAI_BNE, XAI_BLTU, XAI_BGEU,
                       XAI_RET, COPY)>;
  def : InstRW<[XAIWriteMLoad], (instrs XAI_MLOAD_F16, XAI_MLOAD_F32)>;
  def : InstRW<[XAIWriteMMA], (instrs XAI_MMA)>;
  def : InstRW<[XAIWriteMStore], (instrs XAI_MSTORE_F16, XAI_MSTORE_F32)>;
}
