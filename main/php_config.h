/* Build-time configuration for PHP Nano. No configure script is used. */
#ifndef PHP_NANO_PHP_CONFIG_H
#define PHP_NANO_PHP_CONFIG_H

#include <Zend/zend_config.h>

#define PHP_NANO 1
#define HAVE_TIMELIB_CONFIG_H 1
#define ZEND_DEBUG 0
#define PCRE2_CODE_UNIT_WIDTH 8
#define HAVE_BUNDLED_PCRE 1
/* Use PHP's portable C SHA-3 implementation on every Nano target. */
#define HAVE_SLOW_HASH3 1

/* Values normally emitted by php-src's configure-generated build-defs.h. */
#ifndef PHP_UNAME
#define PHP_UNAME "PHP Nano"
#endif
#ifndef PHP_CONFIG_FILE_PATH
#define PHP_CONFIG_FILE_PATH ""
#endif

#if defined(_WIN32)
#error "php-nano does not target Windows; use TypePHP --nano with the full PHP/PHPX DLL runtime"
#endif

#define HAVE_DIRENT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_LOCALTIME_R 1
#define HAVE_GMTIME_R 1
#define HAVE_CTIME_R 1
#define HAVE_ASCTIME_R 1
#define HAVE_FCNTL_H 1
#define HAVE_UTIME_H 1
#define HAVE_UTIME 1
#define HAVE_LSTAT 1
#define HAVE_SYMLINK 1
#define HAVE_SCANDIR 1
#define HAVE_ALPHASORT 1
#define HAVE_MKSTEMP 1
#ifndef __wasi__
#define HAVE_GRP_H 1
#define HAVE_PWD_H 1
#endif
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define _exit _Exit

#endif
