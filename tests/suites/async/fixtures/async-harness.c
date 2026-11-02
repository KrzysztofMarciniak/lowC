#include <stdio.h>

int async_module_test(void);

int
main(void)
{
    int result = async_module_test();

    if (result != 0) {
        fprintf(stderr, "async module test failed: %d\n", result);
    }

    return result;
}
