#ifndef LOWC_CODEGEN_TARGET_X86_64_TYPE_H
#define LOWC_CODEGEN_TARGET_X86_64_TYPE_H

/*
 * x86-64 type representation helpers.
 *
 * These functions do not perform semantic type checking.
 *
 * Semantic analysis decides what a type means.
 *
 * This file only answers questions such as:
 *
 *     How many bytes does this value occupy?
 *     What alignment does it need?
 *     Is it signed?
 *     Which x86 memory operand size should be used?
 *     Which register should contain the value?
 */

#include "../../../parser/public.h"


/*
 * Return the size of a lowC type in bytes.
 *
 * pointer_depth != 0 means the value is a pointer,
 * regardless of the pointed-to type.
 */
unsigned long
codegen_x86_64_type_size(
    const char *type,
    unsigned long pointer_depth
);


/*
 * Return the required alignment of a lowC type.
 */
unsigned long
codegen_x86_64_type_alignment(
    const char *type,
    unsigned long pointer_depth
);


/*
 * Return non-zero when the type is signed.
 */
int
codegen_x86_64_type_signed(
    const char *type,
    unsigned long pointer_depth
);


/*
 * Return the x86 memory operand size for a type.
 *
 * Examples:
 *
 *     BYTE PTR
 *     WORD PTR
 *     DWORD PTR
 *     QWORD PTR
 */
const char *
codegen_x86_64_type_operand_size(
    const char *type,
    unsigned long pointer_depth
);


/*
 * Return the general-purpose x86-64 register used for
 * a value of this type.
 *
 * Examples:
 *
 *     al
 *     ax
 *     eax
 *     rax
 */
const char *
codegen_x86_64_type_register(
    const char *type,
    unsigned long pointer_depth
);

#endif
