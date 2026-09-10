/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Freestanding host contract used by the experimental kernel profile. |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#ifndef PHP_NANO_KERNEL_H
#define PHP_NANO_KERNEL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Install the contiguous physical-memory arena used by the libc/POSIX shim.
 * The caller owns page-table setup and must keep the whole region mapped. */
void php_nano_kernel_memory_init(void *address, size_t size);
size_t php_nano_kernel_memory_available(void);

/* Initialize the unmodified Zend class/object/exception memory model. */
void php_nano_kernel_host_init(void);
void php_nano_kernel_zend_classes_init(void);
int php_nano_kernel_startup_extensions(
    struct _zend_module_entry *const *extensions, size_t count);

/* C++17 global new/delete are supplied by the kernel profile and allocate
 * exclusively through Zend MM (emalloc/efree). */

/* A platform may override these weak hooks for diagnostics and shutdown. */
void php_nano_kernel_write(const char *data, size_t size);
void php_nano_kernel_panic(const char *message);

#ifdef __cplusplus
}
#endif

#endif
