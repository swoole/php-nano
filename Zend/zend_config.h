/* Build-time configuration for the PHP Nano source selection.
 * This replaces configure output; Zend implementation files remain upstream. */
#ifndef PHP_NANO_ZEND_CONFIG_H
#define PHP_NANO_ZEND_CONFIG_H

#define ZEND_MM_ALIGNMENT 8
#define ZEND_MM_ALIGNMENT_LOG2 3
#define ZEND_ENABLE_ZVAL_LONG64 1

#if defined(_WIN32)
# define ZEND_API
# define ZEND_DLEXPORT
#elif defined(__GNUC__) || defined(__clang__)
# define ZEND_API __attribute__((visibility("default")))
# define ZEND_DLEXPORT __attribute__((visibility("default")))
#else
# define ZEND_API
# define ZEND_DLEXPORT
#endif

#define SIZEOF_INT __SIZEOF_INT__
#define SIZEOF_LONG __SIZEOF_LONG__
#define SIZEOF_LONG_LONG __SIZEOF_LONG_LONG__
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#define SIZEOF_ZEND_LONG 8

#if defined(__GNUC__) || defined(__clang__)
# define PHP_HAVE_BUILTIN_CLZ 1
# define PHP_HAVE_BUILTIN_CLZL 1
# define PHP_HAVE_BUILTIN_CLZLL 1
# define PHP_HAVE_BUILTIN_CTZL 1
# define PHP_HAVE_BUILTIN_CTZLL 1
# define PHP_HAVE_BUILTIN_EXPECT 1
# define PHP_HAVE_BUILTIN_UNREACHABLE 1
# define HAVE_ATTRIBUTE_ALIGNED 1
#endif

#if defined(__has_builtin)
# if __has_builtin(__builtin_saddl_overflow)
#  define PHP_HAVE_BUILTIN_SADDL_OVERFLOW 1
# endif
# if __has_builtin(__builtin_saddll_overflow)
#  define PHP_HAVE_BUILTIN_SADDLL_OVERFLOW 1
# endif
# if __has_builtin(__builtin_ssubl_overflow)
#  define PHP_HAVE_BUILTIN_SSUBL_OVERFLOW 1
# endif
# if __has_builtin(__builtin_ssubll_overflow)
#  define PHP_HAVE_BUILTIN_SSUBLL_OVERFLOW 1
# endif
# if __has_builtin(__builtin_smull_overflow)
#  define PHP_HAVE_BUILTIN_SMULL_OVERFLOW 1
# endif
# if __has_builtin(__builtin_smulll_overflow)
#  define PHP_HAVE_BUILTIN_SMULLL_OVERFLOW 1
# endif
#endif

#endif
