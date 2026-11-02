#include <stdint.h>

extern uint64_t time_module_test(void);

int main(void)
{
    return time_module_test() != 0;
}
