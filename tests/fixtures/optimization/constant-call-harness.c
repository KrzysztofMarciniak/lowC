extern unsigned char result_x;
extern unsigned char result_y;
void entrypoint(void);

int
main(void)
{
    entrypoint();
    return result_x == 0 && result_y == 100 ? 0 : 1;
}
