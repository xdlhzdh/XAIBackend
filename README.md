# XAI LLVM 后端

XAI 是一个教学用的自定义 AI 指令集，用来演示 LLVM 怎样把 MMA intrinsic 编成汇编。它没有硬件，也没有模拟器，`llc` 只生成文本汇编。

名字用 **XAI** 而不是 AIX。AIX 是 IBM 的操作系统，LLVM 的 triple 里已经有 `aix` 环境，再做一个叫 AIX 的架构会和它撞车。

## 指令集在后端里怎么落

矩阵寄存器 `m0`–`m7` 模仿 Intel AMX 的 tile，不把整块矩阵塞进 GPR。每个矩阵寄存器按 1024 字节分配，正好放下 16×16 FP32；16×16 FP16 只有 512 字节，仍占用同一个寄存器，由操作码决定怎么解释。

控制流和地址使用 64 位 GPR。指针和 `i64` 从 `r0` 到 `r7` 传递，`r31` 是栈指针。矩阵值不走参数寄存器，而是从指针用 tile load/store 进出 `m0`–`m7`：

```
xai.mload.f16 md, imm(rs)
xai.mload.f32 md, imm(rs)
xai.mstore.f32 imm(rs), ma
```

矩阵乘加是非破坏式的四操作数形式，对应 `D = A×B+C`，结果写到独立的目的寄存器，而不是写回累加器：

```
xai.mma md, ma, mb, mc
```

`ma`、`mb` 按 16×16 FP16 解释，`mc`、`md` 按 16×16 FP32 解释。在 LLVM IR 里这是 `<256 x half>` 和 `<256 x float>`。

## 编译路径

后端走 SelectionDAG。`llvm.xai.mma` 用 TableGen `Pat` 选择 `xai.mma`。`mload` / `mstore` 带 chain，在 `XAIISelDAGToDAG` 里自定义选择 `ISD::INTRINSIC_W_CHAIN` 和 `ISD::INTRINSIC_VOID`。整数 `add` 选 `xai.add` 或 `xai.addi`，条件分支收成 `xai.beq` / `xai.bne` / `xai.bltu` / `xai.bgeu`。矩阵寄存器不够时，通用寄存器分配会把 tile spill 到 `r31` 指向的栈上。

汇编输出需要 `MCAsmBackend`。目标文件输出需要 `MCCodeEmitter`，这里故意没做，所以 `-filetype=obj` 会失败。

## 上游 LLVM

本仓库不把 llvm-project 做成子模块，也不在它的历史里提交。上游只以一个修订号引入，本地修改只以补丁序列存在。

| 记录 | 作用 |
|---|---|
| `patches/llvm-project.rev` | 上游提交，当前是 `068c6c5c0c8a0555036a2ff09a99f486548e6e8d` |
| `patches/series` | 按顺序应用到该提交上的补丁 |
| `patches/0001-register-xai-architecture.patch` | 修改上游已有文件，让它认识 `xai` |
| `patches/0002-add-xai-backend.patch` | 增加上游没有的目标、intrinsic 和测试 |
| `patches/upstream-files.txt` | 允许改动的路径。`+` 开头的是新增文件或目录 |

`scripts/setup_llvm.sh` 做四件事：

1. 把上游对象放在 `third_party/cache/llvm-project.git`。这里只用于按修订号取树，仓库钩子拒绝在这里提交。sparse checkout 取 `llvm`、`cmake` 和 `third-party`（Support 需要 siphash，测试需要 unittest）。
2. 用 `git archive` 导出一份没有 `.git` 的干净树到 `third_party/llvm-project`。
3. 按 `patches/series` 打补丁。补丁必须能干净地应用到该修订号；打不上就停止。
4. 把修订号和补丁校验和写入 `.xai-stamp`。印章一致时不再打补丁，也不改源码时间戳。`./scripts/build_llc.sh` 会先做这件事；CMake 缓存没变时不再重新配置，Ninja 只编译过期目标。

改上游已有文件时，编辑导出树里对应的文件。改新增的目标源码时，编辑本仓库的 `llvm/`。然后运行 `scripts/update_llvm_patch.sh`。它重写 `0001` 和 `0002`，并拒绝 `patches/upstream-files.txt` 以外的源码差异。换上游提交时，修改 `patches/llvm-project.rev`，确认 `patches/series` 仍能应用；需要丢弃已导出的树时用 `XAI_LLVM_RESET=1 ./scripts/setup_llvm.sh`。`build/` 会保留。

安装前缀留在 `third_party/llvm-project/build`，不会写到 `/usr/local`。`llvm-lit` 启动时会调用 `llvm-config`，所以 `scripts/build_llc.sh` 会一并编它。

## 构建和测试

```bash
./scripts/setup_llvm.sh
./scripts/build_llc.sh
third_party/llvm-project/build/bin/llc -mtriple=xai -o - llvm/test/CodeGen/XAI/mma.ll
third_party/llvm-project/build/bin/llvm-lit -v third_party/llvm-project/llvm/test/CodeGen/XAI
```

指令说明见 `docs/isa.md`。CodeGen 流水线、各阶段实现和支持范围见 `docs/实现与CodeGen对照.md`。
