/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Small freestanding C/POSIX compatibility layer for Zend bootstrap.  |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+

   This is not a replacement for zend_alloc. It only supplies the host calls
   used by zend_alloc to acquire its aligned 2 MiB chunks, plus the C memory
   primitives emitted by C/C++ compilers. User allocations continue through
   emalloc/efree and therefore retain the original Zend allocator semantics.
*/

#include "php_nano_kernel.h"

#include <stdarg.h>
#include <locale.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

typedef struct php_nano_kernel_block {
    size_t size;
} php_nano_kernel_block;

static uintptr_t arena_cursor;
static uintptr_t arena_end;

/* The kernel profile has no stdio object. These opaque values only satisfy
 * upstream error paths; bytes are forwarded through the host write hook. */
void *stdin = (void *) 0;
void *stdout = (void *) 1;
void *stderr = (void *) 2;

size_t strlen(const char *string);
void *malloc(size_t size);
int posix_memalign(void **result, size_t alignment, size_t size);

static uintptr_t align_up(uintptr_t value, size_t alignment)
{
    return (value + alignment - 1u) & ~((uintptr_t) alignment - 1u);
}

static int is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1u)) == 0;
}

void php_nano_kernel_memory_init(void *address, size_t size)
{
    const uintptr_t begin = (uintptr_t) address;
    if (size > UINTPTR_MAX - begin) {
        arena_cursor = 0;
        arena_end = 0;
        return;
    }
    arena_cursor = begin;
    arena_end = begin + size;
}

size_t php_nano_kernel_memory_available(void)
{
    return arena_end >= arena_cursor ? (size_t) (arena_end - arena_cursor) : 0;
}

__attribute__((weak)) void php_nano_kernel_write(const char *data, size_t size)
{
    (void) data;
    (void) size;
}

__attribute__((weak, noreturn)) void php_nano_kernel_panic(const char *message)
{
    php_nano_kernel_write(message, strlen(message));
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void *memset(void *destination, int value, size_t size)
{
    unsigned char *output = (unsigned char *) destination;
    while (size-- != 0) {
        *output++ = (unsigned char) value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *output = (unsigned char *) destination;
    const unsigned char *input = (const unsigned char *) source;
    while (size-- != 0) {
        *output++ = *input++;
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t size)
{
    unsigned char *output = (unsigned char *) destination;
    const unsigned char *input = (const unsigned char *) source;
    if (output <= input || output >= input + size) {
        return memcpy(destination, source, size);
    }
    output += size;
    input += size;
    while (size-- != 0) {
        *--output = *--input;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t size)
{
    const unsigned char *a = (const unsigned char *) left;
    const unsigned char *b = (const unsigned char *) right;
    while (size-- != 0) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
        ++a;
        ++b;
    }
    return 0;
}

size_t strlen(const char *string)
{
    const char *end = string;
    while (*end != '\0') {
        ++end;
    }
    return (size_t) (end - string);
}

char *strdup(const char *string)
{
    const size_t size = strlen(string) + 1;
    char *copy = (char *) malloc(size);
    return copy ? (char *) memcpy(copy, string, size) : 0;
}

int strcmp(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return (unsigned char) *left - (unsigned char) *right;
}

int strncmp(const char *left, const char *right, size_t size)
{
    while (size-- != 0) {
        const unsigned char a = (unsigned char) *left++;
        const unsigned char b = (unsigned char) *right++;
        if (a != b) {
            return (int) a - (int) b;
        }
        if (a == 0) {
            return 0;
        }
    }
    return 0;
}

void *memchr(const void *memory, int character, size_t size)
{
    const unsigned char *cursor = (const unsigned char *) memory;
    const unsigned char value = (unsigned char) character;
    while (size-- != 0) {
        if (*cursor == value) {
            return (void *) cursor;
        }
        ++cursor;
    }
    return 0;
}

char *strchr(const char *string, int character)
{
    do {
        if (*string == (char) character) {
            return (char *) string;
        }
    } while (*string++ != '\0');
    return 0;
}

char *strrchr(const char *string, int character)
{
    const char *match = 0;
    do {
        if (*string == (char) character) {
            match = string;
        }
    } while (*string++ != '\0');
    return (char *) match;
}

char *strstr(const char *haystack, const char *needle)
{
    const size_t needle_size = strlen(needle);
    if (needle_size == 0) {
        return (char *) haystack;
    }
    while (*haystack != '\0') {
        if (*haystack == *needle && strncmp(haystack, needle, needle_size) == 0) {
            return (char *) haystack;
        }
        ++haystack;
    }
    return 0;
}

static unsigned char ascii_lower(unsigned char character)
{
    return character >= 'A' && character <= 'Z'
        ? (unsigned char) (character + ('a' - 'A'))
        : character;
}

int strcasecmp(const char *left, const char *right)
{
    while (*left != '\0' && ascii_lower((unsigned char) *left) == ascii_lower((unsigned char) *right)) {
        ++left;
        ++right;
    }
    return (int) ascii_lower((unsigned char) *left) - (int) ascii_lower((unsigned char) *right);
}

int strncasecmp(const char *left, const char *right, size_t size)
{
    while (size-- != 0) {
        const unsigned char a = ascii_lower((unsigned char) *left++);
        const unsigned char b = ascii_lower((unsigned char) *right++);
        if (a != b) {
            return (int) a - (int) b;
        }
        if (a == 0) {
            return 0;
        }
    }
    return 0;
}

int isascii(int character)
{
    return (character & ~0x7f) == 0;
}

int isdigit(int character)
{
    return character >= '0' && character <= '9';
}

int islower(int character)
{
    return character >= 'a' && character <= 'z';
}

int isupper(int character)
{
    return character >= 'A' && character <= 'Z';
}

int isalpha(int character)
{
    return islower(character) || isupper(character);
}

int isalnum(int character)
{
    return isalpha(character) || isdigit(character);
}

int isspace(int character)
{
    return character == ' ' || (character >= '\t' && character <= '\r');
}

int isxdigit(int character)
{
    return isdigit(character)
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}

int tolower(int character)
{
    return isupper(character) ? character + ('a' - 'A') : character;
}

int toupper(int character)
{
    return islower(character) ? character - ('a' - 'A') : character;
}

struct lconv *localeconv(void)
{
    static struct lconv value = {
        .decimal_point = ".",
        .thousands_sep = "",
        .grouping = "",
    };
    return &value;
}

double pow(double base, double exponent)
{
    long power = (long) exponent;
    if ((double) power != exponent) {
        return 0.0;
    }
    double result = 1.0;
    unsigned long magnitude = power < 0 ? (unsigned long) (-power) : (unsigned long) power;
    while (magnitude != 0) {
        if ((magnitude & 1u) != 0) {
            result *= base;
        }
        base *= base;
        magnitude >>= 1u;
    }
    return power < 0 ? 1.0 / result : result;
}

double fmod(double value, double divisor)
{
    if (divisor == 0.0) {
        return 0.0 / 0.0;
    }
    double quotient = value / divisor;
    if (quotient >= 0.0) {
        quotient = (double) (unsigned long) quotient;
    } else {
        quotient = (double) (long) quotient;
    }
    return value - quotient * divisor;
}

double ceil(double value)
{
    long integral = (long) value;
    return value > (double) integral ? (double) integral + 1.0 : (double) integral;
}

double floor(double value)
{
    long integral = (long) value;
    return value < (double) integral ? (double) integral - 1.0 : (double) integral;
}

double trunc(double value)
{
    return (double) (long) value;
}

double round(double value)
{
    return value < 0.0 ? ceil(value - 0.5) : floor(value + 0.5);
}

double fabs(double value)
{
    return value < 0.0 ? -value : value;
}

int abs(int value)
{
    return value < 0 ? -value : value;
}

long long llabs(long long value)
{
    return value < 0 ? -value : value;
}

int ap_php_vsnprintf(char *buffer, size_t size, const char *format, va_list args);

int vsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
    return ap_php_vsnprintf(buffer, size, format, args);
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = ap_php_vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int __snprintf_chk(char *buffer, size_t size, int flag, size_t buffer_size, const char *format, ...)
{
    (void) flag;
    (void) buffer_size;
    va_list args;
    va_start(args, format);
    const int result = ap_php_vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

__attribute__((noreturn)) void __longjmp_chk(void *environment, int value)
{
    (void) environment;
    (void) value;
    php_nano_kernel_panic("zend_bailout");
}

__attribute__((noreturn)) void longjmp(jmp_buf environment, int value)
{
    (void) environment;
    (void) value;
    php_nano_kernel_panic("zend_bailout");
}

int fputs(const char *string, void *stream)
{
    (void) stream;
    php_nano_kernel_write(string, strlen(string));
    return 0;
}

int fputc(int character, void *stream)
{
    (void) stream;
    const char byte = (char) character;
    php_nano_kernel_write(&byte, 1);
    return (unsigned char) byte;
}

size_t fwrite(const void *data, size_t size, size_t count, void *stream)
{
    (void) stream;
    if (size != 0 && count > SIZE_MAX / size) {
        return 0;
    }
    php_nano_kernel_write((const char *) data, size * count);
    return count;
}

int fprintf(void *stream, const char *format, ...)
{
    (void) stream;
    /* Zend's allocator only uses this on fatal paths. Keep the implementation
     * allocation-free; the complete formatter belongs to the standard layer. */
    php_nano_kernel_write(format, strlen(format));
    return (int) strlen(format);
}

int __fprintf_chk(void *stream, int flag, const char *format, ...)
{
    (void) flag;
    return fprintf(stream, format);
}

void *malloc(size_t size)
{
    void *result = 0;
    if (posix_memalign(&result, 16, size) != 0) {
        return 0;
    }
    return result;
}

void free(void *pointer)
{
    /* The bootstrap arena is monotonic. Zend's own allocations are reclaimed
     * by zend_mm; backing chunks remain reserved until the kernel exits. */
    (void) pointer;
}

void *calloc(size_t count, size_t size)
{
    if (count != 0 && size > SIZE_MAX / count) {
        return 0;
    }
    const size_t bytes = count * size;
    void *result = malloc(bytes);
    if (result != 0) {
        memset(result, 0, bytes);
    }
    return result;
}

void *realloc(void *pointer, size_t size)
{
    if (pointer == 0) {
        return malloc(size);
    }
    if (size == 0) {
        return 0;
    }
    php_nano_kernel_block *old = (php_nano_kernel_block *) pointer - 1;
    void *replacement = malloc(size);
    if (replacement != 0) {
        memcpy(replacement, pointer, old->size < size ? old->size : size);
    }
    return replacement;
}

int posix_memalign(void **result, size_t alignment, size_t size)
{
    if (result == 0 || !is_power_of_two(alignment) || alignment < sizeof(void *)) {
        return 22; /* EINVAL */
    }
    if (arena_cursor == 0 || size > SIZE_MAX - sizeof(php_nano_kernel_block)) {
        *result = 0;
        return 12; /* ENOMEM */
    }

    const uintptr_t payload = align_up(
        arena_cursor + sizeof(php_nano_kernel_block), alignment);
    if (payload > arena_end || size > arena_end - payload) {
        *result = 0;
        return 12;
    }
    php_nano_kernel_block *block = (php_nano_kernel_block *) payload - 1;
    block->size = size;
    arena_cursor = payload + size;
    *result = (void *) payload;
    return 0;
}

__attribute__((noreturn)) void abort(void)
{
    php_nano_kernel_panic("abort");
}

__attribute__((noreturn)) void exit(int status)
{
    (void) status;
    php_nano_kernel_panic("exit");
}

__attribute__((noreturn)) void _Exit(int status)
{
    exit(status);
}
