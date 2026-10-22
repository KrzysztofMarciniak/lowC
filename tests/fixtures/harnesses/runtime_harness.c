long add(long left, long right);
long nested(void);
long folded(void);
long point_sum(void);
long byte_array_sum(void);
long pointer_member_sum(void);
long struct_array_sum(void);
long pointer_arithmetic(void);

int
main(void)
{
    return add(20, 22) == 42 &&
           nested() == 142 &&
           folded() == 20 &&
           point_sum() == 42 &&
           byte_array_sum() == 42 &&
           pointer_member_sum() == 42 &&
           struct_array_sum() == 42 &&
           pointer_arithmetic() == 42
        ? 0
        : 1;
}
