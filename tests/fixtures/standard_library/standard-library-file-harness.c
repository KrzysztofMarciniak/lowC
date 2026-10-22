#include <stdint.h>

extern uint64_t standard_library_file_test(unsigned char *path);

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }

    return standard_library_file_test((unsigned char *)argv[1]) != 0;
}
