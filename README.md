# XAI LLVM 后端

XAI 是一个教学用 LLVM 目标，用来演示 CodeGen 如何把矩阵乘加 intrinsic 编成文本汇编。架构名是 `xai`。LLVM 的操作系统类型里已经有 IBM AIX 的 `aix`，所以这里使用独立的 `Triple::xai`。

没有硬件、模拟器、汇编器或目标文件编码器。`llc -mtriple=xai` 只输出文本汇编。

指令语义见 [`docs/isa.md`](docs/isa.md)。CodeGen 各阶段和源码的对应见 [`docs/实现与CodeGen对照.md`](docs/实现与CodeGen对照.md)。

## 仓库内容

| 路径 | 内容 |
|---|---|
| `llvm/` | 新增的目标、intrinsic 和测试，路径与 LLVM 树内布局一致 |
| `patches/llvm-project.rev` | 上游 llvm-project 的提交 |
| `patches/series` | 按顺序打到该提交上的补丁 |
| `patches/0001-register-xai-architecture.patch` | 修改上游已有文件，注册架构 `xai` |
| `patches/0002-add-xai-backend.patch` | 加入 `llvm/` 里的新文件 |
| `patches/upstream-files.txt` | 允许改动的路径。以 `+` 开头的是新增文件或目录 |
| `third_party/` | 本地缓存和构建树，不纳入本仓库 |

矩阵寄存器是 `m0`–`m7`，每个按 1024 字节分配。`xai.mma md, ma, mb, mc` 表示 `md = ma×mb+mc`：`ma`、`mb` 是 16×16 FP16，`mc`、`md` 是 16×16 FP32。指针和 `i64` 使用 `r0`–`r7`，`r31` 是栈指针。

## 上游 LLVM

本仓库不保存 llvm-project 的提交历史，也不把它做成子模块。

`scripts/setup_llvm.sh` 把 `patches/llvm-project.rev` 中的提交放进 `third_party/cache/llvm-project.git`。这里只存放对象，钩子会拒绝提交。取树范围是 `llvm`、`cmake` 和 `third-party`：Support 需要 siphash，测试需要 unittest。

构建用的源码在 `third_party/llvm-project`，这个目录没有 `.git`。

- 目录不存在时，用 `git archive` 导出干净源码，再按 `patches/series` 打补丁。
- 目录已是未打补丁的上游树时，直接在该树上打补丁。
- `.xai-stamp` 记录修订号和每个补丁的校验和。印章与当前补丁一致，且 `llvm/` 与 `0002` 一致时，不再打补丁，也不修改源码时间戳。

`0002` 由本仓库的 `llvm/` 生成。改完目标源码后运行 `scripts/update_llvm_patch.sh`，它重写 `0001` 和 `0002`，并拒绝 `patches/upstream-files.txt` 以外的差异。只改 `llvm/`、不更新补丁时，下一次 setup 会失败。

更换上游提交时，修改 `patches/llvm-project.rev` 并确认补丁仍能应用。`XAI_LLVM_RESET=1 ./scripts/setup_llvm.sh` 按新修订号重新导出源码，已有的 `build/` 会保留。

## 构建和测试

`scripts/build_llc.sh` 会先运行 setup。CMake 参数与 `build/.xai-cmake-id` 一致时不再重新配置。Ninja 只编译过期目标。二进制留在构建目录中，脚本不会安装到 `/usr/local`。

```bash
./scripts/build_llc.sh
third_party/llvm-project/build/bin/llc -mtriple=xai -o - llvm/test/CodeGen/XAI/mma.ll
third_party/llvm-project/build/bin/llvm-lit -v third_party/llvm-project/llvm/test/CodeGen/XAI
```

该脚本编译 `llc`、`llvm-config`、`FileCheck`、`not` 和 `count`。这个 LLVM 修订里的 `llvm-lit` 由 CMake 生成，不是 Ninja 目标，启动时要调用 `llvm-config`。

## MLIR offload 前端

`frontend/` 把一条 16×16 的 `linalg.matmul` 降成 `llvm.xai.*`。`gpu-module-to-binary` 里 `#xai.target` 实现 `gpu::TargetAttrInterface`：`serializeToObject` 调用上面的 `llc -mtriple=xai` 得到文本汇编，再装进 ELF64 的 `.xai_isa` 节。这一节里是汇编文本，不是机器码；XAI 没有指令编码器。offloading translate 只把这份设备 ELF 嵌进宿主 LLVM IR，最后由系统 `llc`（需要 X86 后端）写出宿主 ELF，`.xai_isa` 作为常量包在里面。

```bash
cmake -S frontend -B frontend/build -G Ninja -DMLIR_DIR=/usr/local/lib/cmake/mlir
ninja -C frontend/build xai-compile
./scripts/build_llc.sh
frontend/build/bin/xai-compile \
  --input frontend/test/matmul_xai.mlir \
  --output /tmp/xai_matmul.o \
  --dump-device-elf /tmp/xai_device.elf
```

`--pipeline-stop-after` 可以停在 `rewrite`、`outline`、`binary`、`llvm` 或 `elf`。

后端使用 SelectionDAG。`llvm.xai.mma` 由 TableGen `Pat` 选成 `xai.mma`；带 chain 的 tile load/store 在 `XAIISelDAGToDAG` 中选择。当前能通过指令选择的是 `void` 函数、最多 8 个 `ptr`/`i64` 参数、五条 `llvm.xai.*` intrinsic、`i64` 加法，以及 `eq` / `ne` / `ult` 分支。完整范围和失败点见 CodeGen 文档第 6 节。
