#include "type.h"

#include <stdlib.h>
#include <string.h>


/*
 * Parse the byte width from an integer type name.
 *
 * Examples:
 *
 *     u1 -> 1
 *     u2 -> 2
 *     u4 -> 4
 *     u8 -> 8
 *
 * Returns zero when the type is not a primitive integer
 * type.
 */
static unsigned long
integer_width(
    const char *type
)
{
    if (!type){
        return 0;
    }


    if (
        type[0] != 'u' &&
        type[0] != 's'
    ){
        return 0;
    }


    return strtoul(
        type + 1,
        NULL,
        10
    );
}


/*
 * Return the size of a lowC type in bytes.
 */
unsigned long
codegen_x86_64_type_size(
    const char *type,
    unsigned long pointer_depth
)
{
    unsigned long width;


    /*
     * Every pointer is 64 bits on x86-64.
     */
    if (pointer_depth != 0){
        return 8;
    }


    if (!type){
        return 0;
    }


    /*
     * void has no storage size.
     */
    if (strcmp(type, "void") == 0){
        return 0;
    }


    /*
     * Primitive integer types.
     */
    width =
        integer_width(type);


    if (width != 0){

        switch (width){

            case 1:
                return 1;

            case 2:
                return 2;

            case 4:
                return 4;

            case 8:
                return 8;

            default:
                return 0;
        }
    }


    /*
     * Structs and arrays cannot be reliably handled from
     * the type name alone.
     *
     * Their actual layout belongs to semantic/type
     * analysis.
     *
     * Until that information is passed into codegen,
     * leave the size unknown rather than pretending every
     * struct is eight bytes.
     */
    return 0;
}


/*
 * Return the alignment of a lowC type.
 */
unsigned long
codegen_x86_64_type_alignment(
    const char *type,
    unsigned long pointer_depth
)
{
    unsigned long size;


    /*
     * Pointers are naturally 8-byte aligned.
     */
    if (pointer_depth != 0){
        return 8;
    }


    size =
        codegen_x86_64_type_size(
            type,
            pointer_depth
        );


    if (size == 0){
        return 1;
    }


    /*
     * x86-64 primitive integer alignment follows the
     * natural size, up to 8 bytes.
     */
    if (size > 8){
        return 8;
    }


    return size;
}


/*
 * Return non-zero when a primitive integer type is signed.
 */
int
codegen_x86_64_type_signed(
    const char *type,
    unsigned long pointer_depth
)
{
    /*
     * Pointers are not signed integers.
     */
    if (pointer_depth != 0){
        return 0;
    }


    if (!type){
        return 0;
    }


    return type[0] == 's';
}


/*
 * Return the x86 memory operand size for a type.
 */
const char *
codegen_x86_64_type_operand_size(
    const char *type,
    unsigned long pointer_depth
)
{
    unsigned long size;


    size =
        codegen_x86_64_type_size(
            type,
            pointer_depth
        );


    switch (size){

        case 1:
            return "BYTE PTR";

        case 2:
            return "WORD PTR";

        case 4:
            return "DWORD PTR";

        case 8:
            return "QWORD PTR";

        default:
            return NULL;
    }
}


/*
 * Return the x86-64 general-purpose register for a value.
 */
const char *
codegen_x86_64_type_register(
    const char *type,
    unsigned long pointer_depth
)
{
    unsigned long size;


    size =
        codegen_x86_64_type_size(
            type,
            pointer_depth
        );


    switch (size){

        case 1:
            return "al";

        case 2:
            return "ax";

        case 4:
            return "eax";

        case 8:
            return "rax";

        default:
            return "rax";
    }
}
