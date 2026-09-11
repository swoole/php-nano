/* SPDX-License-Identifier: BSD-3-Clause */

#include "php.h"
#include "php_nano_extension.h"
#include "zend_exceptions.h"
extern "C" {
#include "ext/bcmath/libbcmath/src/bcmath.h"
#include "ext/standard/php_var.h"
}

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stddef.h>
#include <string.h>

static_assert(sizeof(zend_long) == 8, "TypePHP integers must stay 64-bit");
static_assert(sizeof(zval) == 16, "zval layout must match PHP 8.6");
static_assert(offsetof(zend_string, val) >= 20, "zend_string header layout changed");

namespace {

void test_scalar_zvals() {
    zval value;
    ZVAL_LONG(&value, INT64_C(9223372036854775807));
    assert(Z_TYPE(value) == IS_LONG);
    assert(Z_LVAL(value) == ZEND_LONG_MAX);

    ZVAL_DOUBLE(&value, 3.5);
    assert(Z_TYPE(value) == IS_DOUBLE);
    assert(Z_DVAL(value) == 3.5);

    ZVAL_BOOL(&value, true);
    assert(Z_TYPE(value) == IS_TRUE);
    ZVAL_NULL(&value);
    assert(Z_TYPE(value) == IS_NULL);
}

void test_strings() {
    zend_string *hello = zend_string_init("hello", 5, false);
    assert(ZSTR_LEN(hello) == 5);
    assert(strcmp(ZSTR_VAL(hello), "hello") == 0);
    assert(zend_string_hash_val(hello) == zend_hash_func("hello", 5));

    zend_string *copy = zend_string_copy(hello);
    assert(copy == hello);
    assert(GC_REFCOUNT(hello) == 2);
    zend_string_release(copy);
    assert(GC_REFCOUNT(hello) == 1);

    zend_string *message = zend_string_concat2(
        ZSTR_VAL(hello), ZSTR_LEN(hello), " native", 7);
    assert(zend_string_equals_cstr(message, "hello native", 12));
    zend_string_release(message);
    zend_string_release(hello);

    assert(zend_string_init_fast("", 0) == zend_empty_string);
    assert(ZSTR_IS_INTERNED(zend_empty_string));

    zend_string *safe = zend_string_safe_alloc(3, 4, 2, false);
    assert(ZSTR_LEN(safe) == 14);
    assert(GC_TYPE(safe) == IS_STRING);
    zend_string_efree(safe);
}

void test_arrays() {
    HashTable *array = zend_new_array(2);
    assert(zend_hash_num_elements(array) == 0);
    assert(zend_array_is_list(array));

    zval first;
    ZVAL_LONG(&first, 10);
    assert(zend_hash_next_index_insert(array, &first) != nullptr);

    zval second;
    ZVAL_STRING(&second, "two");
    assert(zend_hash_next_index_insert(array, &second) != nullptr);
    assert(zend_array_is_list(array));
    assert(zend_hash_num_elements(array) == 2);
    assert(Z_LVAL_P(zend_hash_index_find(array, 0)) == 10);
    assert(strcmp(Z_STRVAL_P(zend_hash_index_find(array, 1)), "two") == 0);

    zval named;
    ZVAL_TRUE(&named);
    assert(zend_hash_str_update(array, "enabled", 7, &named) != nullptr);
    assert(!zend_array_is_list(array));
    assert(Z_TYPE_P(zend_hash_str_find(array, "enabled", 7)) == IS_TRUE);

    HashTable *copy = zend_array_dup(array);
    zval replacement;
    ZVAL_LONG(&replacement, 99);
    zend_hash_index_update(copy, 0, &replacement);
    assert(Z_LVAL_P(zend_hash_index_find(array, 0)) == 10);
    assert(Z_LVAL_P(zend_hash_index_find(copy, 0)) == 99);

    assert(zend_hash_index_del(copy, 1) == SUCCESS);
    assert(zend_hash_index_find(copy, 1) == nullptr);
    assert(zend_hash_num_elements(copy) == 2);

    zend_array_release(copy);
    zend_array_release(array);
}

void test_zval_copy_lifetime() {
    zval original;
    ZVAL_STRING(&original, "shared");
    zval copied;
    ZVAL_COPY(&copied, &original);
    assert(GC_REFCOUNT(Z_STR(original)) == 2);
    zval_ptr_dtor(&original);
    assert(strcmp(Z_STRVAL(copied), "shared") == 0);
    zval_ptr_dtor(&copied);

    zval empty;
    ZVAL_EMPTY_ARRAY(&empty);
    assert(Z_ARR(empty) == &zend_empty_array);
    zval_ptr_dtor(&empty);
}

void test_unserialize_uses_request_strings() {
    static constexpr char serialized[] = "a:1:{s:16:\"nano-request-key\";i:8;}";
    zval result;
    ZVAL_UNDEF(&result);
    php_unserialize_with_options(
        &result, serialized, sizeof(serialized) - 1, nullptr, "unserialize");
    assert(Z_TYPE(result) == IS_ARRAY);
    assert(zend_hash_num_elements(Z_ARR(result)) == 1);
    const Bucket *bucket = Z_ARR(result)->arData;
    assert(bucket->key != nullptr);
    assert(!(GC_FLAGS(bucket->key) & IS_STR_PERSISTENT));
    assert(Z_TYPE(bucket->val) == IS_LONG);
    assert(Z_LVAL(bucket->val) == 8);
    zval_ptr_dtor(&result);
}

void test_internal_class_objects() {
    assert(zend_standard_class_def != nullptr);

    zval object;
    assert(object_init_ex(&object, zend_standard_class_def) == SUCCESS);
    assert(Z_TYPE(object) == IS_OBJECT);
    assert(Z_OBJCE(object) == zend_standard_class_def);
    assert(zend_string_equals_literal(zend_standard_class_def->name, "stdClass"));
    zval_ptr_dtor(&object);
}

void test_bcmath_backend() {
    bc_num left = nullptr;
    bc_num right = nullptr;
    static constexpr char left_text[] = "999999999999999999999999999999";
    static constexpr char right_text[] = "1";
    assert(bc_str2num(
        &left, left_text, left_text + sizeof(left_text) - 1, 0, nullptr, false));
    assert(bc_str2num(
        &right, right_text, right_text + sizeof(right_text) - 1, 0, nullptr, false));
    bc_num sum = bc_add(left, right, 0);
    zend_string *encoded = bc_num2str(sum);
    assert(zend_string_equals_literal(encoded, "1000000000000000000000000000000"));
    zend_string_release(encoded);
    bc_free_num(&sum);
    bc_free_num(&right);
    bc_free_num(&left);
}

void test_aot_exception_without_vm_frame() {
    assert(EG(current_execute_data) == nullptr);
    assert(EG(exception) == nullptr);
    zend_object *exception = zend_throw_exception(
        zend_ce_value_error, "nano direct-call exception", 0);
    assert(exception != nullptr);
    assert(EG(exception) == exception);
    zend_clear_exception();
    assert(EG(exception) == nullptr);
}

void test_started_extensions() {
    assert(php_nano_find_extension("standard") != nullptr);
    assert(php_nano_find_extension("date") != nullptr);
    assert(php_nano_find_extension("hash") != nullptr);
    assert(php_nano_find_extension("json") != nullptr);
    assert(php_nano_find_extension("pcre") != nullptr);
    assert(php_nano_find_extension("SPL") != nullptr);
    assert(php_nano_find_extension("Reflection") != nullptr);
    assert(php_nano_find_extension("random") != nullptr);
    assert(php_nano_find_extension("filter") != nullptr);
    /* libbcmath is an internal arithmetic backend, not a public extension. */
    assert(php_nano_find_extension("bcmath") == nullptr);
    assert(php_nano_find_extension("missing") == nullptr);
    assert(!zend_hash_str_exists(EG(class_table), ZEND_STRL("fiber")));
    assert(!zend_hash_str_exists(EG(class_table), ZEND_STRL("generator")));
    assert(!zend_hash_str_exists(EG(class_table), ZEND_STRL("reflectiongenerator")));
    assert(!zend_hash_str_exists(EG(class_table), ZEND_STRL("reflectionfiber")));
    assert(zend_hash_str_exists(EG(class_table), ZEND_STRL("reflectionreference")));

    static const char *unsupported_functions[] = {
        "dl",
        "exec",
        "shell_exec",
        "fsockopen",
        "stream_socket_server",
        "stream_socket_client",
        "gethostbyname",
        "dns_get_record",
        "getenv",
        "putenv",
        "header",
        "mail",
        "openlog",
        "filter_input",
        "bcadd",
        "highlight_file",
        "highlight_string",
        "php_strip_whitespace",
    };
    for (const char *name : unsupported_functions) {
        assert(!zend_hash_str_exists(EG(function_table), name, strlen(name)));
    }

    static const char *supported_functions[] = {
        "ob_start",
        "ini_get",
        "fopen",
        "file_get_contents",
        "md5_file",
        "hash",
        "hash_file",
        "json_encode",
        "preg_match",
        "random_int",
        "filter_var",
        "date",
        "array_merge",
        "base64_encode",
        "urlencode",
        "serialize",
        "unserialize",
        "version_compare",
        "print_r",
        "uniqid",
        "strlen",
        "phpinfo",
        "phpversion",
        "phpcredits",
        "php_sapi_name",
        "php_uname",
        "php_ini_scanned_files",
        "php_ini_loaded_file",
    };
    for (const char *name : supported_functions) {
        assert(zend_hash_str_exists(EG(function_table), name, strlen(name)));
    }
    assert(php_nano_startup_extensions(nullptr, 0) == FAILURE);
}

void test_cli_globals(int expected_argc, const char *expected_last_argument) {
    zval *argument_count = zend_hash_str_find(
        &EG(symbol_table), ZEND_STRL("argc"));
    assert(argument_count != nullptr);
    assert(Z_TYPE_P(argument_count) == IS_LONG);
    assert(Z_LVAL_P(argument_count) == expected_argc);

    zval *arguments = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("argv"));
    assert(arguments != nullptr);
    assert(Z_TYPE_P(arguments) == IS_ARRAY);
    assert(zend_hash_num_elements(Z_ARR_P(arguments))
        == static_cast<uint32_t>(expected_argc));
    if (expected_argc > 0) {
        zval *last = zend_hash_index_find(
            Z_ARR_P(arguments), static_cast<zend_ulong>(expected_argc - 1));
        assert(last != nullptr);
        assert(Z_TYPE_P(last) == IS_STRING);
        assert(strcmp(Z_STRVAL_P(last), expected_last_argument) == 0);
    }
}

void test_complete_restart_cycle() {
    php_nano_shutdown_composer_extensions();
    assert(php_nano_find_extension("standard") == nullptr);
    char executable[] = "php-nano-tests";
    char argument[] = "restart-argument";
    char *arguments[] = {executable, argument};
    php_nano_set_cli_arguments(2, arguments);
    const zend_result restart_result = php_nano_startup_composer_extensions();
    assert(restart_result == SUCCESS);
    if (restart_result != SUCCESS) {
        abort();
    }
    test_scalar_zvals();
    test_strings();
    test_arrays();
    test_zval_copy_lifetime();
    test_unserialize_uses_request_strings();
    test_internal_class_objects();
    test_bcmath_backend();
    test_aot_exception_without_vm_frame();
    test_started_extensions();
    test_cli_globals(2, argument);
}

} // namespace

extern "C" int typephp_nano_project_main() {
    test_scalar_zvals();
    test_strings();
    test_arrays();
    test_zval_copy_lifetime();
    test_unserialize_uses_request_strings();
    test_internal_class_objects();
    test_bcmath_backend();
    test_aot_exception_without_vm_frame();
    test_started_extensions();
    test_complete_restart_cycle();
    return 0;
}
