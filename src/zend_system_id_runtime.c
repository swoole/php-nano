/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Minimal extension-handle entropy ABI without compiler/VM hooks.     |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#include "zend.h"
#include "zend_system_id.h"

ZEND_API char zend_system_id[32] = {0};

ZEND_API zend_result zend_add_system_entropy(
	const char *module_name,
	const char *hook_name,
	const void *data,
	size_t size)
{
	(void) module_name;
	(void) hook_name;
	(void) data;
	(void) size;
	return SUCCESS;
}
