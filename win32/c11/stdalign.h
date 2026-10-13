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
 * supply the standard C11 7.15 macros here. The xxhash.h branch that selects
 * this include is only reachable in C, and this directory is on the include
 * path for Windows only; compilers that provide the real header are unaffected.
 */

#ifndef PHP_NANO_WIN32_C11_STDALIGN_H
#define PHP_NANO_WIN32_C11_STDALIGN_H

/* alignas and alignof are keywords in C++; only C needs the macros. */
#if !defined(__cplusplus) && !defined(__alignas_is_defined)
# define __alignas_is_defined 1
# define alignas _Alignas
#endif

#if !defined(__cplusplus) && !defined(__alignof_is_defined)
# define __alignof_is_defined 1
# define alignof _Alignof
#endif

#endif /* PHP_NANO_WIN32_C11_STDALIGN_H */
