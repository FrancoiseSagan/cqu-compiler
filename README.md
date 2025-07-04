# cqu-compiler

重庆大学 编译原理 2025 春季实验

## homework 1 && homework 2

详见分支：homework1&2

## lab 1

语法解析和词法解析，总代码量约1000行，详见分支 lab1

## lab 2

AST -> IR，总代码量约2000行，详见分支 lab2

## lab 3

IR -> riscv32汇编代码，总代码量约1000行，详见分支 lab3

### lab 3 注意事项

助教的文档对于如何开始写得不是很清楚，在这里解释几个小问题：

**Q1：我生成的就是汇编为什么还要装riscv32的交叉编译器？**

A1：因为这里生成的汇编并不是完整代码，我们生成的程序甚至没有 _start 而众所周知程序从 _start 启动

**Q2：应该下载什么，在哪里下载？**

A2：首先需要下载gcc版本的riscv32编译工具，**千万不要**下载源码从源码编译，编译时占用存储空间很大并且容易出现问题，请直接下载编译好的预编译版本：

[Releases · riscv-collab/riscv-gnu-toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain/releases)（根据自己的ubuntu版本选择，注意**不要**下成elf版本）

另外还需要riscv32的qemu，**建议修改score.py，使用静态编译，运行时也是用qemu-riscv32-static**，qemu可以直接从包管理工具下载

**Q3：假如我想认真做实验3该从哪里开始？**

A3：实验3的代码比实验2少但是涉及riscv体系的内容很多，本人作为看过riscv手册的开始做时也麻，建议先**直接看学长的代码，搞清楚流程，再根据看不懂的地方看手册和查资料**

**Q4：寄存器分配怎么做？**

A4：由于难度和时间原因，应该不会有人真正地实现寄存器分配（即使是线性分配），都是采用一些取巧的方法，由于我的实验3是以助教的思路为基础的，所以下面讲一下此思路：

### 最丁真的实现思路

寄存器如何分配？答案是根本不分配，对于所有变量全部保存在分配的栈帧上（注意是全部），维护一个变量到内存位置的映射，例如对于：

```
add a, b, c
```

直接从内存读出b和c到两个寄存器，加到另一个寄存器再写回a在栈上的位置。

这样对于除了浮点数以外的计算，只需要使用三个固定的寄存器，进入函数也就只需要保存ra,s0和这三个寄存器。

这样搞，栈帧格局是这样的：

```
高地址
+----------------------------------+ <- 旧sp位置（s0指向此处）
| 返回地址(ra)      | fp-4  (sp+1996) |
| 帧指针(s0)        | fp-8  (sp+1992) |
| 保存的s1寄存器    | fp-12 (sp+1988) |
| 保存的s2寄存器    | fp-16 (sp+1984) |
| 保存的s3寄存器    | fp-20 (sp+1980) |
+-------------------------------------+
| 参数区域开始                        |
| (传递给被调用函数的参数)             |
+-------------------------------------+
| 局部变量区域开始  | fp-56           | <- 注意这里的size设置，只要能在上面放得下函数参数就行
| (向下增长)                          |
|                                     |
+-------------------------------------+ <- 当前sp位置（新sp=旧sp-frame_size），需要设置合适的frame_size
低地址
```

那**栈帧大小需要多大**？这取决于你的ir实现，所有局部变量都要放到栈上，由于我的ir全部使用静态单赋值并且很低级，连立即数运算都没有需要大约2000 byte（绷不住了）

另外，为了过实验2你可能为数组加入了全部初始化为0的IR，在做实验3时请删除，不需要考虑此问题，编译器会帮我们做。
