## A working compiler for a C language subset
This project, USCC, builds out a working compiler for a C language subset called USC ([language reference](./USCLanguage.pdf)). 
USCC uses the LLVM framework as its code generation engine.

#### Project 1: Recursive Descent Parsing
Implemented a `LL(1) parser` that parses the program into AST. [[Handout]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/P1.pdf) [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP1.pdf)

#### Project 2: Semantic Analysis
Implemented scoped symbol table that enable scoped identifier declarations, and type checking/conversion that enables conversion between `char` and `int`. [[Handout]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/p2.pdf) [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP2.pdf).

#### Project 3: LLVM IR Generation
Implemented an `IREmitter` that lowers various types of AST node (e.g., `While` statement, `If` statement) to LLVM IR using `llvm::IRBuilder`. [[Handout]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/P3.pdf) [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP3.pdf)

#### Project 4: Static Single Assignment (SSA) generation
Implemented a fairly new approach to generate SSA form as outlined in [this paper](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/Simple%20and%20Efficient%20Construction%20of%20Static%20Single%20Assignment%20Form.pdf), which directly generate SSA form from the high-level AST, without generating non-SSA form first as in Clang/LLVM. [[Handout]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/P4.pdf) [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP4.pdf)

#### Project 5: LLVM Optimization Passes
Implemented three LLVM optimization passes, including constant branch folding, dead block removal and loop invariant code motion (LICM). 
