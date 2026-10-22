long module_math(void);
long module_sum(void);

int
main(void)
{
    return module_math() == 50 && module_sum() == 83
        ? 0
        : 1;
}
