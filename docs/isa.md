# XAI ISA

XAI 是一个教学用的自定义 AI 指令集，用来演示 LLVM 后端如何把 MMA intrinsic 编成汇编。它不是 IBM AIX，也没有对应的硬件或模拟器。

控制流和地址使用 64 位通用寄存器。矩阵乘加的操作数使用矩阵寄存器，模型接近 Intel AMX 的 tile 寄存器，而不是把整块 tile 塞进 GPR。

## 寄存器

| 名字 | 数量 | 用途 |
|---|---|---|
| `r0`–`r30` | 31 | 64 位 GPR，参数从 `r0` 起 |
| `r31` | 1 | 栈指针 |
| `m0`–`m7` | 8 | 矩阵寄存器，每个按 1024 字节分配 |

1024 字节刚好放下 16×16 FP32。16×16 FP16 只有 512 字节，仍占用同一个矩阵寄存器，由操作码决定怎么解释。

## 指令

非破坏式矩阵乘加，对应 PTX `mma` 的 `D = A×B+C`，不采用 AMX 那种把结果写回累加器的形式：

```
xai.mma md, ma, mb, mc
```

`md = ma × mb + mc`。`ma`、`mb` 按 16×16 FP16 解释，`mc`、`md` 按 16×16 FP32 解释。

Tile 和内存之间：

```
xai.mload.f16 md, imm(rs)
xai.mload.f32 md, imm(rs)
xai.mstore.f16 imm(rs), ma
xai.mstore.f32 imm(rs), ma
```

GPR 访存、移动、加法和返回，供调用约定、寄存器分配、栈槽和循环使用：

```
xai.ld rd, imm(rs)
xai.st imm(rs), rs
xai.mov rd, rs
xai.mmov md, ms
xai.add rd, rs1, rs2
xai.addi rd, rs, imm
xai.li rd, imm
xai.ret
```

`xai.add` 是寄存器加。`xai.addi` 的立即数是有符号 32 位，prologue 用它调整 `r31`，循环用它做指针步进。`xai.li` 把有符号 32 位常量放进 GPR，例如循环初值 0。

比较并跳转只支持四种条件。`icmp ult` 在块布局取反之后会变成 `uge`：

```
xai.br label
xai.beq rs1, rs2, label
xai.bne rs1, rs2, label
xai.bltu rs1, rs2, label
xai.bgeu rs1, rs2, label
```

有符号比较和函数调用没有对应指令。

## 调用约定

指针和 `i64` 从 `r0` 到 `r7` 依次传递。没有被调用者保存寄存器。返回值只支持 `void`。矩阵值不通过参数传递，而是从指针 load 进 `m0`–`m7`。

## LLVM intrinsic

```
declare <256 x half> @llvm.xai.mload.f16(ptr)
declare <256 x float> @llvm.xai.mload.f32(ptr)
declare void @llvm.xai.mstore.f16(ptr, <256 x half>)
declare void @llvm.xai.mstore.f32(ptr, <256 x float>)
declare <256 x float> @llvm.xai.mma(<256 x half>, <256 x half>, <256 x float>)
```

`<256 x half>` 是 16×16 FP16，`<256 x float>` 是 16×16 FP32。
