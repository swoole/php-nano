/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdalign.h>
#include <stdalign.h>

#if __alignas_is_defined != 1 || __alignof_is_defined != 1
# error Alignment feature macros must be available in C11 and C++17
#endif

#ifdef __cplusplus
# if defined(alignas) || defined(alignof)
#  error C++ alignment keywords must not be replaced by macros
# endif
# define PHP_NANO_ALIGNMENT_ASSERT static_assert
#else
# if !defined(alignas) || !defined(alignof)
#  error C11 alignment macros must be available
# endif
# define PHP_NANO_ALIGNMENT_ASSERT _Static_assert
#endif

struct php_nano_alignment_test {
    alignas(64) unsigned char bytes[64];
};

PHP_NANO_ALIGNMENT_ASSERT(
    alignof(struct php_nano_alignment_test) == 64,
    "The alignment specifier must apply to the containing struct");
PHP_NANO_ALIGNMENT_ASSERT(
    sizeof(struct php_nano_alignment_test) == 64,
    "The aligned struct must retain its expected layout");
