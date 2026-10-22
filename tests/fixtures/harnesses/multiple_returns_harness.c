extern unsigned long multiple_return_result;

void entrypoint(void);

int
main(void)
{
    entrypoint();
    return multiple_return_result == 1 ? 0 : 1;
}
