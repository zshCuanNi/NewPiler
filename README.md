# newpiler

Compiler for translating SysY ( a simple C-like language) to RISC-Ⅴ, using Flex and Bison

Course project of PKU Compile Course.

Basically follow the instructions of [PKU compiler course online documentation](https://pku-minic.github.io/online-doc/#/). First, translate SysY to a high-level intermediate representation, Eeyore. After that, use Linear Scan Algorithm to allocate registers and generate Tigger codes. Finally, directly generate RISC-Ⅴ assembly from Tigger.

SysY $\rightarrow$ Eeyore part: This part refers to [SysYCompiler](https://github.com/zshCuanNi/SysYCompiler). In general, the process of parsing, building AST, and generating Eeyore code in the meantime is converted to creating only AST nodes in the parsing phase and traversing the entire AST to generate Eeyore code later.

Eeyore $\rightarrow$ Tigger part: This part uses Linear Scan Register Allocation. The general process is: building Eeyore AST -- basic block partition -- creating control flow graph -- liveness analysis -- liveness interval calculation -- linear scan register allocation.

Tigger $\rightarrow$ RISC-Ⅴ part: This part refers to the online documentation mentioned above, basically doing table lookup conversion. Do strength reduction for MUL, DIV and REM.

<hr>

北京大学编译原理课程实践项目，将一种精简版的 C 语言—— SysY 语言转换为 RISC-Ⅴ 汇编。

大体流程依照[北大编译实践在线文档](https://pku-minic.github.io/online-doc/#/)的指示，首先将 SysY 转换成高级中间表示 Eeyore，再通过 Linear Scan 算法进行寄存器分配生成 Tigger 中间表示，最后直接翻译为 RISC-Ⅴ 汇编。

SysY $\rightarrow$ Eeyore 部分大致参照 [SysYCompiler](https://github.com/zshCuanNi/SysYCompiler)，主要将一边 parse 一边建立 AST 同时生成 Eeyore 代码的过程转换为在 parse 阶段只新建 AST 节点，随后遍历整棵 AST 生成 Eeyore 代码。

Eeyore $\rightarrow$ Tigger 部分使用 Linear Scan 算法分配寄存器，大致流程为：建立 Eeyore 语法分析树 -- 基本块划分 -- 建立控制流图 -- 活跃分析 -- 计算活跃区间 -- 线性扫描分配寄存器。

Tigger $\rightarrow$ RISC-Ⅴ 部分基本按前述在线文档进行查表转换。针对 mul, div, rem 进行强度削减。
