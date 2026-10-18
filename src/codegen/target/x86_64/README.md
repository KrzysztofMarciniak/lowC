# x86-64 backend

The backend emits GNU assembler-compatible Intel-syntax x86-64 assembly for the System V AMD64 calling convention. `--asm` prints this output; normal compilation adds a `main` wrapper for the required zero-argument `void entrypoint()` and invokes the system C compiler driver to assemble and link an executable.

The default backend favors readable output and keeps parameter/local values in stack slots. `-O1` enables lightweight integer constant folding before instruction emission, reducing intermediate instructions and temporary stack traffic in constant expressions. Without `-O1`, the backend retains its straightforward expression lowering.
