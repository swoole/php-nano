/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | C11 <stdalign.h> for toolchains that report C11 but ship no header.  |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

/*
 * MSVC compiles Nano's C11 sources with /std:c11, so __STDC_VERSION__ is
 * 201112L, but its toolset still ships no <stdalign.h>. Vendored php-src code
 * includes that header to reach `alignas` (see ext/hash/xxhash/xxhash.h), so
 * supply the C11 alignment macros here. The Windows composition shares its
 * include paths across C and C++, including Clang, so this header also preserves
 * the alignment feature macros in C++ without redefining its keywords.
 */

#ifndef PHP_NANO_WIN32_C11_STDALIGN_H
#define PHP_NANO_WIN32_C11_STDALIGN_H

/* alignas and alignof are keywords in C++; only C needs the macros. */
#ifndef __cplusplus
# ifndef alignas
#  define alignas _Alignas
# endif
# ifndef alignof
#  define alignof _Alignof
# endif
#endif

#ifndef __alignas_is_defined
# define __alignas_is_defined 1
#endif
#ifndef __alignof_is_defined
# define __alignof_is_defined 1
#endif

#endif /* PHP_NANO_WIN32_C11_STDALIGN_H */
