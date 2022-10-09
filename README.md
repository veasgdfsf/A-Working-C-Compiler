### CS502 Course Project
This project, USCC, builds out a working compiler for a C language subset called USC. 
USCC uses the LLVM framework as its code generation engine.

#### Project 1: Recursive Descent Parsing
Implemented a `LL(1) parser` that parses the program into AST. 

#### Project 2: Semantic Analysis
Implemented scoped symbol table that enable scoped identifier declarations, and type checking/conversion that enables conversion between `char` and `int`. 

#### Project 3: LLVM IR Generation
Implemented an `IREmitter` that lowers various types of AST node (e.g., `While` statement, `If` statement) to LLVM IR. 
