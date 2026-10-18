#include <stdio.h>

void entrypoint(void);
long read_score(void);

int
main(void)
{
    entrypoint();
    long score = read_score();
    printf("Trail challenge complete! Score: %ld (expected 42)\n", score);
    return score == 42 ? 0 : 1;
}
