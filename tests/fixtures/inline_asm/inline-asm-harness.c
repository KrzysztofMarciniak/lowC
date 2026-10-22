unsigned long inline_assembly_move(void);
unsigned long inline_assembly_syscall_pid(void);
unsigned long inline_assembly_callee_saved(void);
unsigned long inline_assembly_call(void);

int
main(void)
{
    return inline_assembly_move() == 42 &&
           inline_assembly_syscall_pid() > 0 &&
           inline_assembly_callee_saved() == 17 &&
           inline_assembly_call() == 23
        ? 0
        : 1;
}
