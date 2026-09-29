/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Runtime stubs for the intentionally omitted php.ini parser.         |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#include "zend.h"
#include "zend_ini.h"

ZEND_API zend_result zend_parse_ini_file(
	zend_file_handle *fh,
	bool unbuffered_errors,
	int scanner_mode,
	zend_ini_parser_cb_t ini_parser_cb,
	void *arg)
{
	(void) fh;
	(void) unbuffered_errors;
	(void) scanner_mode;
	(void) ini_parser_cb;
	(void) arg;
	return FAILURE;
}

ZEND_API zend_result zend_parse_ini_string(
	const char *str,
	bool unbuffered_errors,
	int scanner_mode,
	zend_ini_parser_cb_t ini_parser_cb,
	void *arg)
{
	(void) str;
	(void) unbuffered_errors;
	(void) scanner_mode;
	(void) ini_parser_cb;
	(void) arg;
	return FAILURE;
}
