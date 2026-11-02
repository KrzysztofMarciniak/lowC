extern unsigned char dead_code_result;
void entrypoint(void);

int
main(void)
{
    entrypoint();
    return dead_code_result == 2 ? 0 : 1;
}
