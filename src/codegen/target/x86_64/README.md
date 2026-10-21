# x86-64 backend

The backend emits GNU assembler-compatible Intel-syntax x86-64 assembly for the System V AMD64 calling convention. `--asm` prints this output; normal compilation adds a `main` wrapper for the required zero-argument `void entrypoint()` and invokes the system C compiler driver to assemble and link an executable.

The compiler's IR selects declarations, represents typed expression values,
instructions, and control flow as basic blocks, and performs constant folding.
Function bodies are emitted from IR instructions and terminators, and expression
operations are emitted from typed IR value IDs. The backend still reads AST
metadata for function ABI and stack-frame setup, global initializers, source
rendering, and some lvalue address calculations.

Normal builds optimize by default; `-optimize` and `-release` explicitly enable
the IR constant folding, constant-branch simplification, straight-line scalar
constant propagation, and precomputation of simple pure multi-return calls with
constant arguments. Each function is emitted into its own linker section.
Optimized executables are linked with `-Os`, LTO, dead-section removal, and
symbol stripping; `-debug` emits source locations and debug symbols and
disables optimization/stripping unless either optimization flag is also
present. Link-time optimization applies to C sources compiled alongside lowC;
lowC instructions are optimized in the IR/backend.
