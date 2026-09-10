#include <php.h>
#include <zend_API.h>
#include <zend_exceptions.h>
#include <zend_globals.h>
#include <zend_hrtime.h>
#include <zend_interfaces.h>
#include <zend_list.h>
#include <zend_modules.h>
#include <zend_object_handlers.h>
#include <zend_objects_API.h>
#include "php_nano_kernel.h"

#include <stdarg.h>
#include <setjmp.h>

static HashTable kernel_function_table;
static HashTable kernel_class_table;
static HashTable kernel_constants_table;

zend_result zend_startup_builtin_functions(void);


#if ZEND_HRTIME_PLATFORM_POSIX
ZEND_API clockid_t zend_hrtime_posix_clock_id = CLOCK_MONOTONIC;

int clock_gettime(clockid_t clock_id, struct timespec *value)
{
    (void) clock_id;
    value->tv_sec = 0;
    value->tv_nsec = 0;
    return 0;
}
#endif

static void kernel_random_bytes(
    zend_random_bytes_insecure_state *state, void *bytes, size_t size)
{
    unsigned char *output = (unsigned char *) bytes;
    uint64_t value = state->opaque[0] != 0
        ? state->opaque[0]
        : UINT64_C(0x9e3779b97f4a7c15);
    while (size-- != 0) {
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
        *output++ = (unsigned char) value;
    }
    state->opaque[0] = value;
}

static void kernel_error_callback(
    int type, zend_string *file, uint32_t line, zend_string *message)
{
    (void) type;
    (void) file;
    (void) line;
    php_nano_kernel_panic(message ? ZSTR_VAL(message) : "Zend error");
}

/* Only the allocator's fatal recovery boundary references setjmp. A kernel
 * panic never returns, so no jump environment has to be materialized. */
int _setjmp(jmp_buf environment)
{
    (void) environment;
    return 0;
}

void php_nano_kernel_host_init(void)
{
    zend_random_bytes_insecure = kernel_random_bytes;
}

void php_nano_kernel_zend_classes_init(void)
{
    zend_interned_strings_init();

    zend_hash_init(&kernel_function_table, 64, NULL, ZEND_FUNCTION_DTOR, true);
    zend_hash_init(&kernel_class_table, 64, NULL, ZEND_CLASS_DTOR, true);
    CG(function_table) = &kernel_function_table;
    CG(class_table) = &kernel_class_table;
    EG(function_table) = &kernel_function_table;
    EG(class_table) = &kernel_class_table;
    zend_hash_init(&kernel_constants_table, 64, NULL, ZEND_CONSTANT_DTOR, true);
    EG(zend_constants) = &kernel_constants_table;

    zend_hash_init(&module_registry, 8, NULL, NULL, true);
    zend_init_rsrc_list_dtors();
    zend_init_rsrc_list();

    zend_object_handlers_startup();
    zend_objects_store_init(&EG(objects_store), 64);
    zend_printf_to_smart_string = php_printf_to_smart_string;
    zend_printf_to_smart_str = php_printf_to_smart_str;
    zend_error_cb = kernel_error_callback;
}

int php_nano_kernel_startup_extensions(
    zend_module_entry *const *extensions, size_t count)
{
    if (zend_startup_builtin_functions() != SUCCESS) {
        return FAILURE;
    }
    zend_module_entry *core = zend_hash_str_find_ptr(
        &module_registry, ZEND_STRL("Core"));
    if (core == NULL || zend_startup_module_ex(core) != SUCCESS) {
        return FAILURE;
    }

    for (size_t index = 0; index < count; ++index) {
        zend_module_entry *module = zend_register_module_ex(
            extensions[index], MODULE_PERSISTENT);
        if (module == NULL || zend_startup_module_ex(module) != SUCCESS) {
            return FAILURE;
        }
    }
    for (size_t index = 0; index < count; ++index) {
        zend_module_entry *module = extensions[index];
        if (module->request_startup_func != NULL
            && module->request_startup_func(
                module->type, module->module_number) != SUCCESS) {
            return FAILURE;
        }
    }
    return SUCCESS;
}
