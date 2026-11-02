#include <stdio.h>

void entrypoint(void);

static int test_result = 1;
static char *test_base_path;

void
lowc_test_debug(long result)
{
    fprintf(stderr, "OS syscall result: %ld\n", result);
}

char *
lowc_test_base_path(void)
{
    return test_base_path;
}

void
lowc_test_result(int result)
{
    test_result = result;
}

int
main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }
    test_base_path = argv[1];
    entrypoint();
    if (test_result != 0) {
        fprintf(stderr, "OS fixture failed at check %d\n", test_result);
    }
    return test_result;
}
