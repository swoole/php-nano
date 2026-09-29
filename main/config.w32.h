/* Build-time Windows configuration for PHP Nano. */
#ifndef PHP_NANO_CONFIG_W32_H
#define PHP_NANO_CONFIG_W32_H

/* Define the minimum supported version */
#undef _WIN32_WINNT
#undef NTDDI_VERSION
#define _WIN32_WINNT 0x0602
#define NTDDI_VERSION 0x06020000

/* Default PHP / PEAR directories */
#define PHP_CONFIG_FILE_PATH ""
#define PHP_CONFIG_FILE_SCAN_DIR ""
#define PEAR_INSTALLDIR ""
#define PHP_BINDIR ""
#define PHP_SBINDIR ""
#define PHP_DATADIR ""
#define PHP_EXTENSION_DIR ""
#define PHP_INCLUDE_PATH "."
#define PHP_LIBDIR ""
#define PHP_LOCALSTATEDIR ""
#define PHP_PREFIX ""
#define PHP_SYSCONFDIR ""

/* PHP Runtime Configuration */
#define DEFAULT_SHORT_OPEN_TAG "1"

/* MSVC 19.44 / linker 14.44. */
#define PHP_LINKER_MAJOR 14
#define PHP_LINKER_MINOR 44

/* Platform-Specific Configuration. Should not be changed. */
/* Alignment for Zend memory allocator */
#define ZEND_MM_ALIGNMENT (size_t)8
#define ZEND_MM_ALIGNMENT_LOG2 (size_t)3
#define ZEND_MM_NEED_EIGHT_BYTE_REALIGNMENT 0
#define PHP_SIGCHILD 0
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#undef HAVE_SYMLINK

/* its in win32/time.c */
#define HAVE_USLEEP 1
#define HAVE_NANOSLEEP 1

#define HAVE_GETCWD 1
#undef HAVE_SETITIMER
#undef HAVE_IODBC
#define HAVE_GETTIMEOFDAY 1
#define HAVE_TZSET 1
#undef HAVE_FLOCK
#define HAVE_ALLOCA 1
#undef HAVE_SYS_TIME_H
#undef HAVE_STRUCT_STAT_ST_BLKSIZE
#undef HAVE_STRUCT_STAT_ST_BLOCKS
#define HAVE_STRUCT_STAT_ST_RDEV 1
#define HAVE_STRCASECMP 1
#define HAVE_UTIME 1
#undef HAVE_DIRENT_H
#define HAVE_FCNTL_H 1
#undef HAVE_GRP_H
#undef HAVE_PWD_H
#undef HAVE_SYS_FILE_H
#undef HAVE_SYS_SOCKET_H
#undef HAVE_SYS_WAIT_H
#undef HAVE_UNISTD_H
#define HAVE_SYS_TYPES_H 1
#undef HAVE_ALLOCA_H
#undef HAVE_KILL
#define HAVE_GETPID 1
/* int and long are still 32bit in 64bit compiles */
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
/* MSVC.6/NET don't allow 'long long' */
#define SIZEOF_LONG_LONG 8 /* defined as __int64 */
#define ssize_t SSIZE_T
#ifdef _WIN64
# define SIZEOF_SIZE_T 8
#else
# define SIZEOF_SIZE_T 4
#endif
#define SIZEOF_OFF_T 4
#define HAVE_FNMATCH
#define PHP_SHLIB_SUFFIX "dll"
#define PHP_SHLIB_EXT_PREFIX "php_"

#define HAVE_SOCKLEN_T 1

/* vs.net 2005 has a 64-bit time_t.  This will likely break
 * 3rdParty libs that were built with older compilers; switch
 * back to 32-bit */
#ifndef _WIN64
# define _USE_32BIT_TIME_T 1
#endif

#define _REENTRANT 1

#define HAVE_GETRUSAGE

#define HAVE_FTOK 1

#define HAVE_NICE

#ifdef __clang__
#define HAVE_FUNC_ATTRIBUTE_TARGET 1
#endif

#endif /* PHP_NANO_CONFIG_W32_H */
