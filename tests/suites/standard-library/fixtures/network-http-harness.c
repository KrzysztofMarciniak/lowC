#include <stdint.h>
#include <stdlib.h>

extern uint64_t standard_library_network_http_test(uint64_t port);

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }

    return standard_library_network_http_test(strtoull(argv[1], NULL, 10)) != 0;
}
