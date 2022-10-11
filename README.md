## A working compiler for a C language subset
This project, USCC, builds out a working compiler for a C language subset called USC ([language reference](./USCLanguage.pdf)). 
USCC uses the LLVM framework as its code generation engine.

#### Project 1: Recursive Descent Parsing
Implemented a `LL(1) parser` that parses the program into AST. [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP1.pdf)

#### Project 2: Semantic Analysis
Implemented scoped symbol table that enable scoped identifier declarations, and type checking/conversion that enables conversion between `char` and `int`. [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP2.pdf).

#### Project 3: LLVM IR Generation
Implemented an `IREmitter` that lowers various types of AST node (e.g., `While` statement, `If` statement) to LLVM IR using `llvm::IRBuilder`. [[Report]](https://github.com/wuruoyu/CS502-Compiler/blob/master/uscc/uscc/report/reportP3.pdf)
