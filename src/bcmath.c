/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Lifecycle adapter for PHP's bundled libbcmath backend.              |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#include "php_nano_extension.h"
#include "ext/bcmath/php_bcmath.h"

#include <string.h>

ZEND_DECLARE_MODULE_GLOBALS(bcmath)

static bool php_nano_bcmath_started = false;

PHP_NANO_API zend_result php_nano_startup_bcmath(void)
{
	if (php_nano_bcmath_started) {
		return SUCCESS;
	}

#ifdef ZTS
	/* Nano currently has one statically composed runtime per process. */
	return FAILURE;
#else
	memset(&bcmath_globals, 0, sizeof(bcmath_globals));
	bc_init_numbers();
	php_nano_bcmath_started = true;
	return SUCCESS;
#endif
}

PHP_NANO_API void php_nano_shutdown_bcmath(void)
{
	if (!php_nano_bcmath_started) {
		return;
	}

	bc_force_free_number(&BCG(_two_));
	bc_force_free_number(&BCG(_one_));
	bc_force_free_number(&BCG(_zero_));
	memset(&bcmath_globals, 0, sizeof(bcmath_globals));
	php_nano_bcmath_started = false;
}
