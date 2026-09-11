/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Process bootstrap and host error boundary for selected Zend sources.|
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

extern "C" {
#include "php.h"
#include "SAPI.h"
#include "php_nano_extension.h"
#include "php_output.h"
#include "php_streams.h"
#include "spprintf.h"

#include <zend_exceptions.h>
#include <zend_ast.h>
#include <zend_constants.h>
#include <zend_enum.h>
#include <zend_execute.h>
#include <zend_frameless_function.h>
#include <zend_gc.h>
#include <zend_globals.h>
#include <zend_ini.h>
#include <zend_list.h>
#include <zend_map_ptr.h>
#include <zend_modules.h>
#include <zend_object_handlers.h>
#include <zend_strtod.h>
#include <zend_virtual_cwd.h>
#include <zend_attributes.h>
#include <zend_extensions.h>
}

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

extern "C" {
zend_result zend_startup_builtin_functions(void);

/* main/main.c owns this storage in a full PHP build. Nano keeps the same
 * public ABI while replacing main.c's host-facing startup layer. */
php_core_globals core_globals;
sapi_globals_struct sapi_globals;
SAPI_API sapi_module_struct sapi_module;

const char php_build_date[] = __DATE__ " " __TIME__;

ZEND_ATTRIBUTE_CONST PHPAPI const char *php_build_provider(void)
{
    return nullptr;
}

ZEND_API void zend_html_putc(char value)
{
    switch (value) {
    case '<':
        ZEND_PUTS("&lt;");
        break;
    case '>':
        ZEND_PUTS("&gt;");
        break;
    case '&':
        ZEND_PUTS("&amp;");
        break;
    case '\t':
        ZEND_PUTS("    ");
        break;
    default:
        ZEND_PUTC(value);
        break;
    }
}

ZEND_API void zend_html_puts(const char *value, size_t length)
{
    const char *end = value + length;
    while (value < end) {
        zend_html_putc(*value++);
    }
}

/* Kept byte-for-byte equivalent to main/main.c's forwarding layer. Nano does
 * not compile the hosted PHP process bootstrap, but php_ini.c uses this ABI
 * when rendering phpinfo() output. */
PHPAPI void php_html_puts(const char *value, size_t length)
{
    zend_html_puts(value, length);
}

PHPAPI ZEND_COLD void php_verror(
    const char *docref, int type, const char *format, va_list args)
{
    (void) docref;
    zend_string *message = zend_vstrpprintf(0, format, args);
    zend_error_zstr(type, message);
    zend_string_release(message);
}

PHPAPI ZEND_COLD void php_error_docref(
    const char *docref, int type, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    php_verror(docref, type, format, args);
    va_end(args);
}

PHPAPI size_t php_write(void *buf, size_t size)
{
    return PHPWRITE(static_cast<const char *>(buf), size);
}

PHPAPI size_t php_printf(const char *format, ...)
{
    va_list args;
    size_t ret;
    char *buffer;
    size_t size;

    va_start(args, format);
    size = vspprintf(&buffer, 0, format, args);
    ret = PHPWRITE(buffer, size);
    efree(buffer);
    va_end(args);

    return ret;
}

PHPAPI size_t php_printf_unchecked(const char *format, ...)
{
    va_list args;
    size_t ret;
    char *buffer;
    size_t size;

    va_start(args, format);
    size = vspprintf(&buffer, 0, format, args);
    ret = PHPWRITE(buffer, size);
    efree(buffer);
    va_end(args);

    return ret;
}
}

namespace {

bool core_started = false;
bool core_active = false;
bool ini_started = false;
int cli_argc = 0;
char **cli_argv = nullptr;

void register_cli_globals() {
    zval argument_count;
    ZVAL_LONG(&argument_count, cli_argc);
    zend_hash_str_update(&EG(symbol_table), ZEND_STRL("argc"), &argument_count);

    zval arguments;
    array_init_size(&arguments, cli_argc);
    for (int index = 0; index < cli_argc; ++index) {
        add_next_index_string(&arguments, cli_argv[index]);
    }
    zend_hash_str_update(&EG(symbol_table), ZEND_STRL("argv"), &arguments);
}

void insecure_random_bytes(
    zend_random_bytes_insecure_state *state, void *bytes, size_t size) {
    auto *output = static_cast<unsigned char *>(bytes);
    auto *words = reinterpret_cast<uint64_t *>(state);
    uint64_t value = words[0] != 0 ? words[0] : UINT64_C(0x9e3779b97f4a7c15);
    for (size_t index = 0; index < size; ++index) {
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
        output[index] = static_cast<unsigned char>(value);
    }
    words[0] = value;
}

void nano_error_callback(
    int,
    zend_string *,
    uint32_t,
    zend_string *message) {
    if (message != nullptr) {
        std::fwrite(ZSTR_VAL(message), 1, ZSTR_LEN(message), stderr);
    }
    std::fputc('\n', stderr);
}

size_t nano_write(const char *data, size_t length) {
    return std::fwrite(data, 1, length, stdout);
}

size_t nano_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = std::vprintf(format, args);
    va_end(args);
    return written < 0 ? 0 : static_cast<size_t>(written);
}

} // namespace

extern "C" {

PHP_NANO_API zend_result php_nano_startup_core(void) {
    if (core_started) {
        return SUCCESS;
    }
    zend_random_bytes_insecure = insecure_random_bytes;
    std::memset(&core_globals, 0, sizeof(core_globals));
    std::memset(&sapi_module, 0, sizeof(sapi_module));
    std::memset(&compiler_globals, 0, sizeof(compiler_globals));
    std::memset(&executor_globals, 0, sizeof(executor_globals));
    core_globals.serialize_precision = -1;
    sapi_module.name = "nano";
    sapi_module.pretty_name = "PHP Nano";
    sapi_module.phpinfo_as_text = true;
    start_memory_manager();
    gc_globals_ctor();
    zend_interned_strings_init();
    zend_startup_extensions_mechanism();
    if (php_nano_startup_bcmath() != SUCCESS) {
        return FAILURE;
    }

    CG(function_table) = static_cast<HashTable *>(std::malloc(sizeof(HashTable)));
    CG(class_table) = static_cast<HashTable *>(std::malloc(sizeof(HashTable)));
    CG(auto_globals) = static_cast<HashTable *>(std::malloc(sizeof(HashTable)));
    EG(zend_constants) = static_cast<HashTable *>(std::malloc(sizeof(HashTable)));
    if (CG(function_table) == nullptr || CG(class_table) == nullptr ||
        CG(auto_globals) == nullptr || EG(zend_constants) == nullptr) {
        return FAILURE;
    }
    zend_hash_init(CG(function_table), 128, nullptr, ZEND_FUNCTION_DTOR, true);
    zend_hash_init(CG(class_table), 64, nullptr, ZEND_CLASS_DTOR, true);
    zend_hash_init(CG(auto_globals), 8, nullptr, nullptr, true);
    zend_hash_init(EG(zend_constants), 64, nullptr, ZEND_CONSTANT_DTOR, true);
    zend_hash_init(&module_registry, 16, nullptr, nullptr, true);

    zend_ini_startup();
    ini_started = true;
    zend_register_standard_ini_entries();

    zend_object_handlers_startup();
    zend_init_rsrc_list_dtors();
    zend_init_rsrc_plist();
    virtual_cwd_startup();
    php_init_stream_wrappers(0);
    zend_printf = nano_printf;
    zend_write = nano_write;
    zend_printf_to_smart_string = php_printf_to_smart_string;
    zend_printf_to_smart_str = php_printf_to_smart_str;
    zend_error_cb = nano_error_callback;

    php_output_startup();

    if (zend_startup_builtin_functions() != SUCCESS) {
        return FAILURE;
    }
    zend_register_standard_constants();
    auto *core = static_cast<zend_module_entry *>(
        zend_hash_str_find_ptr(&module_registry, ZEND_STRL("core")));
    if (core == nullptr || zend_startup_module_ex(core) != SUCCESS) {
        return FAILURE;
    }
    zend_enum_startup();
    core_started = true;
    return SUCCESS;
}

PHP_NANO_API zend_result php_nano_activate_core(void) {
    if (!core_started) {
        return FAILURE;
    }
    if (!core_active) {
        /* All module startup hooks have completed by activation time. Runtime
         * values must use request storage so arrays and objects can release
         * their keys through the normal non-persistent destructor path. */
        zend_interned_strings_switch_storage(true);
        zend_interned_strings_activate();
        zend_init_rsrc_list();
        virtual_cwd_activate();
        init_executor();
        register_cli_globals();
        /* Normally populated by main/main.c after parsing SAPI configuration.
         * Nano retains the Zend INI registry but has no SAPI bootstrap, so
         * install PHP's documented executor default explicitly. */
        EG(precision) = 14;
        if (php_output_activate() != SUCCESS) {
            shutdown_executor();
            zend_destroy_rsrc_list(&EG(regular_list));
            virtual_cwd_deactivate();
            zend_interned_strings_deactivate();
            zend_interned_strings_switch_storage(false);
            return FAILURE;
        }
        core_active = true;
    }
    return SUCCESS;
}

PHP_NANO_API void php_nano_set_cli_arguments(int argc, char **argv) {
    cli_argc = argc;
    cli_argv = argv;
}

PHP_NANO_API void php_nano_deactivate_core(void) {
    if (core_active) {
        php_output_end_all();
        php_output_deactivate();
        zend_call_destructors();
        shutdown_executor();
        zend_ini_deactivate();
        zend_destroy_rsrc_list(&EG(regular_list));
        virtual_cwd_deactivate();
        zend_interned_strings_deactivate();
        /* MSHUTDOWN hooks run after request deactivation and may create or
         * inspect persistent strings, so restore module storage first. */
        zend_interned_strings_switch_storage(false);
        core_active = false;
    }
}

PHP_NANO_API void php_nano_shutdown_core(void) {
    if (!core_started) {
        return;
    }
    php_nano_deactivate_core();
    if (ini_started) {
        zend_ini_shutdown();
        ini_started = false;
    }
    php_output_shutdown();
    zend_interned_strings_switch_storage(false);

    zend_destroy_rsrc_list(&EG(persistent_list));
    auto *core = static_cast<zend_module_entry *>(
        zend_hash_str_find_ptr(&module_registry, ZEND_STRL("core")));
    if (core != nullptr) {
        core->module_started = 0;
    }
    zend_destroy_modules();
    zend_hash_destroy(CG(function_table));
    zend_hash_graceful_reverse_destroy(CG(class_table));
    zend_hash_destroy(CG(auto_globals));
    zend_hash_destroy(EG(zend_constants));
    std::free(CG(function_table));
    std::free(CG(class_table));
    std::free(CG(auto_globals));
    std::free(EG(zend_constants));
    CG(function_table) = nullptr;
    CG(class_table) = nullptr;
    CG(auto_globals) = nullptr;
    EG(function_table) = nullptr;
    EG(class_table) = nullptr;
    EG(zend_constants) = nullptr;

    zend_flf_capacity = 0;
    zend_flf_count = 0;
    std::free(zend_flf_functions);
    std::free(zend_flf_handlers);
    zend_flf_functions = nullptr;
    zend_flf_handlers = nullptr;
    if (CG(map_ptr_real_base) != nullptr) {
        std::free(CG(map_ptr_real_base));
        CG(map_ptr_real_base) = nullptr;
        CG(map_ptr_base) = ZEND_MAP_PTR_BIASED_BASE(nullptr);
        CG(map_ptr_size) = 0;
        CG(map_ptr_last) = 0;
    }
    if (CG(internal_run_time_cache) != nullptr) {
        pefree(CG(internal_run_time_cache), true);
        CG(internal_run_time_cache) = nullptr;
    }
    zend_map_ptr_static_last = 0;
    zend_map_ptr_static_size = 0;
    zend_destroy_rsrc_list_dtors();
    php_shutdown_stream_wrappers(0);
    virtual_cwd_shutdown();
    zend_shutdown_extensions();
    zend_shutdown_strtod();
    zend_attributes_shutdown();
    zend_interned_strings_dtor();
    php_nano_shutdown_bcmath();
    shutdown_memory_manager(true, true);
    gc_globals_dtor();
    zend_printf_to_smart_string = nullptr;
    zend_printf_to_smart_str = nullptr;
    zend_random_bytes_insecure = nullptr;
    zend_error_cb = nullptr;
    cli_argc = 0;
    cli_argv = nullptr;
    core_started = false;
}

} // extern "C"
