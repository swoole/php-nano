/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright © Zend Technologies Ltd., a subsidiary company of          |
   |     Perforce Software, Inc., and Contributors.                       |
   +----------------------------------------------------------------------+
   | This source file is subject to the Modified BSD License that is      |
   | bundled with this package in the file LICENSE, and is available      |
   | through the World Wide Web at <https://www.php.net/license/>.        |
   |                                                                      |
   | SPDX-License-Identifier: BSD-3-Clause                                |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@php.net>                                 |
   |          Zeev Suraski <zeev@php.net>                                 |
   |          Nikita Popov <nikic@php.net>                                |
   +----------------------------------------------------------------------+
*/

/*
 * PHP Nano does not compile PHP source at runtime. This translation unit
 * retains the non-compiler ABI helpers from PHP 8.6 zend_compile.c that are
 * also used by internal classes, Reflection, errors, and property metadata.
 * Function bodies intentionally follow the upstream implementations.
 */

#include "zend.h"
#include "zend_API.h"
#include "zend_compile.h"
#include "zend_exceptions.h"
#include "zend_execute.h"
#include "zend_object_handlers.h"
#include "zend_portability.h"
#include "zend_string.h"
#include "zend_virtual_cwd.h"

#include <ctype.h>
#include <string.h>

#ifndef ZTS
ZEND_API zend_compiler_globals compiler_globals;
ZEND_API zend_executor_globals executor_globals;
#endif

static bool zend_get_unqualified_name(const zend_string *name, const char **result, size_t *result_len) /* {{{ */
{
	const char *ns_separator = zend_memrchr(ZSTR_VAL(name), '\\', ZSTR_LEN(name));
	if (ns_separator != NULL) {
		*result = ns_separator + 1;
		*result_len = ZSTR_VAL(name) + ZSTR_LEN(name) - *result;
		return true;
	}

	return false;
}
/* }}} */

struct reserved_class_name {
	const char *name;
	size_t len;
};
static const struct reserved_class_name reserved_class_names[] = {
	{ZEND_STRL("bool")},
	{ZEND_STRL("false")},
	{ZEND_STRL("float")},
	{ZEND_STRL("int")},
	{ZEND_STRL("null")},
	{ZEND_STRL("parent")},
	{ZEND_STRL("self")},
	{ZEND_STRL("static")},
	{ZEND_STRL("string")},
	{ZEND_STRL("true")},
	{ZEND_STRL("void")},
	{ZEND_STRL("never")},
	{ZEND_STRL("iterable")},
	{ZEND_STRL("object")},
	{ZEND_STRL("mixed")},
	/* These are not usable as class names because they're proper tokens,
	 * but they are here for class aliases. */
	{ZEND_STRL("array")},
	{ZEND_STRL("callable")},
	{NULL, 0}
};

static bool zend_is_reserved_class_name(const zend_string *name) /* {{{ */
{
	const struct reserved_class_name *reserved = reserved_class_names;

	const char *uqname = ZSTR_VAL(name);
	size_t uqname_len = ZSTR_LEN(name);
	zend_get_unqualified_name(name, &uqname, &uqname_len);

	for (; reserved->name; ++reserved) {
		if (uqname_len == reserved->len
			&& zend_binary_strcasecmp(uqname, uqname_len, reserved->name, reserved->len) == 0
		) {
			return true;
		}
	}

	return false;
}
/* }}} */

void zend_assert_valid_class_name(const zend_string *name, const char *type) /* {{{ */
{
	if (zend_is_reserved_class_name(name)) {
		zend_error_noreturn(E_COMPILE_ERROR,
			"Cannot use \"%s\" as %s as it is reserved", ZSTR_VAL(name), type);
	}
	if (zend_string_equals_literal(name, "_")) {
		zend_error(E_DEPRECATED, "Using \"_\" as %s is deprecated since 8.4", type);
	}
}
/* }}} */

ZEND_API zend_string *zend_get_compiled_filename(void) /* {{{ */
{
	return CG(compiled_filename);
}
/* }}} */

ZEND_API uint32_t zend_get_compiled_lineno(void) /* {{{ */
{
	return CG(zend_lineno);
}
/* }}} */

ZEND_API bool zend_is_compiling(void) /* {{{ */
{
	return CG(in_compilation);
}

static zend_always_inline bool zend_auto_global_check(zend_auto_global *auto_global)
{
	if (auto_global == NULL) {
		return false;
	}
	if (auto_global->armed) {
		auto_global->armed = auto_global->auto_global_callback(auto_global->name);
	}
	return true;
}

ZEND_API bool zend_is_auto_global_str(const char *name, size_t len)
{
	return zend_auto_global_check(zend_hash_str_find_ptr(CG(auto_globals), name, len));
}

ZEND_API bool zend_is_auto_global(zend_string *name)
{
	return zend_auto_global_check(zend_hash_find_ptr(CG(auto_globals), name));
}

ZEND_API zend_result zend_register_auto_global(
	zend_string *name, bool jit, zend_auto_global_callback auto_global_callback)
{
	zend_auto_global auto_global;
	auto_global.name = name;
	auto_global.auto_global_callback = auto_global_callback;
	auto_global.jit = jit;
	return zend_hash_add_mem(
		CG(auto_globals), name, &auto_global, sizeof(zend_auto_global)) != NULL
		? SUCCESS : FAILURE;
}
/* }}} */

zend_string *zval_make_interned_string(zval *zv)
{
	ZEND_ASSERT(Z_TYPE_P(zv) == IS_STRING);
	Z_STR_P(zv) = zend_new_interned_string(Z_STR_P(zv));
	if (ZSTR_IS_INTERNED(Z_STR_P(zv))) {
		Z_TYPE_FLAGS_P(zv) = 0;
	}
	return Z_STR_P(zv);
}

ZEND_API zend_string *zend_create_member_string(const zend_string *class_name, const zend_string *member_name) {
	return zend_string_concat3(
		ZSTR_VAL(class_name), ZSTR_LEN(class_name),
		"::", sizeof("::") - 1,
		ZSTR_VAL(member_name), ZSTR_LEN(member_name));
}

static zend_string *add_type_string(zend_string *type, zend_string *new_type, bool is_intersection) {
	zend_string *result;
	if (type == NULL) {
		return zend_string_copy(new_type);
	}

	if (is_intersection) {
		result = zend_string_concat3(ZSTR_VAL(type), ZSTR_LEN(type),
			"&", 1, ZSTR_VAL(new_type), ZSTR_LEN(new_type));
		zend_string_release(type);
	} else {
		result = zend_string_concat3(
			ZSTR_VAL(type), ZSTR_LEN(type), "|", 1, ZSTR_VAL(new_type), ZSTR_LEN(new_type));
		zend_string_release(type);
	}
	return result;
}

static zend_string *resolve_class_name(zend_string *name, const zend_class_entry *scope) {
	if (scope) {
		if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_SELF))) {
			name = scope->name;
		} else if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_PARENT)) && scope->parent) {
			name = scope->parent->name;
		}
	}

	/* The resolved name for anonymous classes contains null bytes. Cut off everything after the
	 * null byte here, to avoid larger parts of the type being omitted by printing code later. */
	size_t len = strlen(ZSTR_VAL(name));
	if (len != ZSTR_LEN(name)) {
		return zend_string_init(ZSTR_VAL(name), len, 0);
	}
	return zend_string_copy(name);
}

static zend_string *add_intersection_type(zend_string *str,
	const zend_type_list *intersection_type_list,
	bool is_bracketed)
{
	const zend_type *single_type;
	zend_string *intersection_str = NULL;

	ZEND_TYPE_LIST_FOREACH(intersection_type_list, single_type) {
		ZEND_ASSERT(!ZEND_TYPE_HAS_LIST(*single_type));
		ZEND_ASSERT(ZEND_TYPE_HAS_NAME(*single_type));

		intersection_str = add_type_string(intersection_str, ZEND_TYPE_NAME(*single_type), /* is_intersection */ true);
	} ZEND_TYPE_LIST_FOREACH_END();

	ZEND_ASSERT(intersection_str);

	if (is_bracketed) {
		zend_string *result = zend_string_concat3("(", 1, ZSTR_VAL(intersection_str), ZSTR_LEN(intersection_str), ")", 1);
		zend_string_release(intersection_str);
		intersection_str = result;
	}
	str = add_type_string(str, intersection_str, /* is_intersection */ false);
	zend_string_release(intersection_str);
	return str;
}

zend_string *zend_type_to_string_resolved(const zend_type type, const zend_class_entry *scope) {
	zend_string *str = NULL;

	/* Pure intersection type */
	if (ZEND_TYPE_IS_INTERSECTION(type)) {
		ZEND_ASSERT(!ZEND_TYPE_IS_UNION(type));
		str = add_intersection_type(str, ZEND_TYPE_LIST(type), /* is_bracketed */ false);
	} else if (ZEND_TYPE_HAS_LIST(type)) {
		/* A union type might not be a list */
		const zend_type *list_type;
		ZEND_TYPE_LIST_FOREACH(ZEND_TYPE_LIST(type), list_type) {
			if (ZEND_TYPE_IS_INTERSECTION(*list_type)) {
				str = add_intersection_type(str, ZEND_TYPE_LIST(*list_type), /* is_bracketed */ true);
				continue;
			}
			ZEND_ASSERT(!ZEND_TYPE_HAS_LIST(*list_type));
			ZEND_ASSERT(ZEND_TYPE_HAS_NAME(*list_type));

			zend_string *name = ZEND_TYPE_NAME(*list_type);
			zend_string *resolved = resolve_class_name(name, scope);
			str = add_type_string(str, resolved, /* is_intersection */ false);
			zend_string_release(resolved);
		} ZEND_TYPE_LIST_FOREACH_END();
	} else if (ZEND_TYPE_HAS_NAME(type)) {
		str = resolve_class_name(ZEND_TYPE_NAME(type), scope);
	}

	uint32_t type_mask = ZEND_TYPE_PURE_MASK(type);

	if (type_mask == MAY_BE_ANY) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_MIXED), /* is_intersection */ false);

		return str;
	}
	if (type_mask & MAY_BE_STATIC) {
		zend_string *name = ZSTR_KNOWN(ZEND_STR_STATIC);
		// During compilation of eval'd code the called scope refers to the scope calling the eval
		if (scope && !zend_is_compiling()) {
			const zend_class_entry *called_scope = zend_get_called_scope(EG(current_execute_data));
			if (called_scope) {
				name = called_scope->name;
			}
		}
		str = add_type_string(str, name, /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_CALLABLE) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_CALLABLE), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_OBJECT) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_OBJECT), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_ARRAY) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_ARRAY), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_STRING) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_STRING), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_LONG) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_INT), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_DOUBLE) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_FLOAT), /* is_intersection */ false);
	}
	if ((type_mask & MAY_BE_BOOL) == MAY_BE_BOOL) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_BOOL), /* is_intersection */ false);
	} else if (type_mask & MAY_BE_FALSE) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_FALSE), /* is_intersection */ false);
	} else if (type_mask & MAY_BE_TRUE) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_TRUE), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_VOID) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_VOID), /* is_intersection */ false);
	}
	if (type_mask & MAY_BE_NEVER) {
		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_NEVER), /* is_intersection */ false);
	}

	if (type_mask & MAY_BE_NULL) {
		bool is_union = !str || memchr(ZSTR_VAL(str), '|', ZSTR_LEN(str)) != NULL;
		bool has_intersection = !str || memchr(ZSTR_VAL(str), '&', ZSTR_LEN(str)) != NULL;
		if (!is_union && !has_intersection) {
			zend_string *nullable_str = zend_string_concat2("?", 1, ZSTR_VAL(str), ZSTR_LEN(str));
			zend_string_release(str);
			return nullable_str;
		}

		str = add_type_string(str, ZSTR_KNOWN(ZEND_STR_NULL_LOWERCASE), /* is_intersection */ false);
	}
	return str;
}

ZEND_API zend_string *zend_type_to_string(zend_type type) {
	return zend_type_to_string_resolved(type, NULL);
}

ZEND_API zend_string *zend_mangle_property_name(const char *src1, size_t src1_length, const char *src2, size_t src2_length, bool internal) /* {{{ */
{
	size_t prop_name_length = 1 + src1_length + 1 + src2_length;
	zend_string *prop_name = zend_string_alloc(prop_name_length, internal);

	ZSTR_VAL(prop_name)[0] = '\0';
	memcpy(ZSTR_VAL(prop_name) + 1, src1, src1_length+1);
	memcpy(ZSTR_VAL(prop_name) + 1 + src1_length + 1, src2, src2_length+1);
	return prop_name;
}
/* }}} */

ZEND_API zend_result zend_unmangle_property_name_ex(const zend_string *name, const char **class_name, const char **prop_name, size_t *prop_len) /* {{{ */
{
	size_t class_name_len;
	size_t anonclass_src_len;

	*class_name = NULL;

	if (!ZSTR_LEN(name) || ZSTR_VAL(name)[0] != '\0') {
		*prop_name = ZSTR_VAL(name);
		if (prop_len) {
			*prop_len = ZSTR_LEN(name);
		}
		return SUCCESS;
	}
	if (ZSTR_LEN(name) < 3 || ZSTR_VAL(name)[1] == '\0') {
		zend_error(E_NOTICE, "Illegal member variable name");
		*prop_name = ZSTR_VAL(name);
		if (prop_len) {
			*prop_len = ZSTR_LEN(name);
		}
		return FAILURE;
	}

	class_name_len = zend_strnlen(ZSTR_VAL(name) + 1, ZSTR_LEN(name) - 2);
	if (class_name_len >= ZSTR_LEN(name) - 2 || ZSTR_VAL(name)[class_name_len + 1] != '\0') {
		zend_error(E_NOTICE, "Corrupt member variable name");
		*prop_name = ZSTR_VAL(name);
		if (prop_len) {
			*prop_len = ZSTR_LEN(name);
		}
		return FAILURE;
	}

	*class_name = ZSTR_VAL(name) + 1;
	anonclass_src_len = zend_strnlen(*class_name + class_name_len + 1, ZSTR_LEN(name) - class_name_len - 2);
	if (class_name_len + anonclass_src_len + 2 != ZSTR_LEN(name)) {
		class_name_len += anonclass_src_len + 1;
	}
	*prop_name = ZSTR_VAL(name) + class_name_len + 2;
	if (prop_len) {
		*prop_len = ZSTR_LEN(name) - class_name_len - 2;
	}
	return SUCCESS;
}
/* }}} */

uint32_t zend_get_class_fetch_type(const zend_string *name) /* {{{ */
{
	if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_SELF))) {
		return ZEND_FETCH_CLASS_SELF;
	} else if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_PARENT))) {
		return ZEND_FETCH_CLASS_PARENT;
	} else if (zend_string_equals_ci(name, ZSTR_KNOWN(ZEND_STR_STATIC))) {
		return ZEND_FETCH_CLASS_STATIC;
	} else {
		return ZEND_FETCH_CLASS_DEFAULT;
	}
}
/* }}} */

ZEND_API void zend_initialize_class_data(zend_class_entry *ce, bool nullify_handlers) /* {{{ */
{
	bool persistent_hashes = ce->type == ZEND_INTERNAL_CLASS;

	ce->refcount = 1;
	ce->ce_flags = ZEND_ACC_CONSTANTS_UPDATED;
	ce->ce_flags2 = 0;

	if (CG(compiler_options) & ZEND_COMPILE_GUARDS) {
		ce->ce_flags |= ZEND_ACC_USE_GUARDS;
	}

	ce->default_properties_table = NULL;
	ce->default_static_members_table = NULL;
	zend_hash_init(&ce->properties_info, 8, NULL, NULL, persistent_hashes);
	zend_hash_init(&ce->constants_table, 8, NULL, NULL, persistent_hashes);
	zend_hash_init(&ce->function_table, 8, NULL, ZEND_FUNCTION_DTOR, persistent_hashes);

	ce->doc_comment = NULL;

	ZEND_MAP_PTR_INIT(ce->static_members_table, NULL);
	ZEND_MAP_PTR_INIT(ce->mutable_data, NULL);

	ce->default_object_handlers = &std_object_handlers;
	ce->default_properties_count = 0;
	ce->default_static_members_count = 0;
	ce->properties_info_table = NULL;
	ce->attributes = NULL;
	ce->enum_backing_type = IS_UNDEF;
	ce->backed_enum_table = NULL;

	if (nullify_handlers) {
		ce->constructor = NULL;
		ce->destructor = NULL;
		ce->clone = NULL;
		ce->__get = NULL;
		ce->__set = NULL;
		ce->__unset = NULL;
		ce->__isset = NULL;
		ce->__call = NULL;
		ce->__callstatic = NULL;
		ce->__tostring = NULL;
		ce->__serialize = NULL;
		ce->__unserialize = NULL;
		ce->__debugInfo = NULL;
		ce->create_object = NULL;
		ce->get_iterator = NULL;
		ce->iterator_funcs_ptr = NULL;
		ce->arrayaccess_funcs_ptr = NULL;
		ce->get_static_method = NULL;
		ce->parent = NULL;
		ce->parent_name = NULL;
		ce->num_interfaces = 0;
		ce->interfaces = NULL;
		ce->num_traits = 0;
		ce->num_hooked_props = 0;
		ce->num_hooked_prop_variance_checks = 0;
		ce->trait_names = NULL;
		ce->trait_aliases = NULL;
		ce->trait_precedences = NULL;
		ce->serialize = NULL;
		ce->unserialize = NULL;
		if (ce->type == ZEND_INTERNAL_CLASS) {
			ce->info.internal.module = NULL;
			ce->info.internal.builtin_functions = NULL;
		}
	}
}
/* }}} */

/* {{{ zend_dirname
   Returns directory name component of path */
ZEND_API size_t zend_dirname(char *path, size_t len)
{
	char *end = path + len - 1;
	unsigned int len_adjust = 0;

#ifdef ZEND_WIN32
	/* Note that on Win32 CWD is per drive (heritage from CP/M).
	 * This means dirname("c:foo") maps to "c:." or "c:" - which means CWD on C: drive.
	 */
	if ((2 <= len) && isalpha((unsigned char)path[0]) && (':' == path[1])) {
		/* Skip over the drive spec (if any) so as not to change */
		path += 2;
		len_adjust += 2;
		if (2 == len) {
			/* Return "c:" on Win32 for dirname("c:").
			 * It would be more consistent to return "c:."
			 * but that would require making the string *longer*.
			 */
			return len;
		}
	}
#endif

	if (len == 0) {
		/* Illegal use of this function */
		return 0;
	}

	/* Strip trailing slashes */
	while (end >= path && IS_SLASH_P_EX(end, end == path)) {
		end--;
	}
	if (end < path) {
		/* The path only contained slashes */
		path[0] = DEFAULT_SLASH;
		path[1] = '\0';
		return 1 + len_adjust;
	}

	/* Strip filename */
	while (end >= path && !IS_SLASH_P_EX(end, end == path)) {
		end--;
	}
	if (end < path) {
		/* No slash found, therefore return '.' */
		path[0] = '.';
		path[1] = '\0';
		return 1 + len_adjust;
	}

	/* Strip slashes which came before the file name */
	while (end >= path && IS_SLASH_P_EX(end, end == path)) {
		end--;
	}
	if (end < path) {
		path[0] = DEFAULT_SLASH;
		path[1] = '\0';
		return 1 + len_adjust;
	}
	*(end+1) = '\0';

	return (size_t)(end + 1 - path) + len_adjust;
}
/* }}} */

ZEND_API void zend_set_function_arg_flags(zend_function *func) /* {{{ */
{
	uint32_t i, n;

	func->common.arg_flags[0] = 0;
	func->common.arg_flags[1] = 0;
	func->common.arg_flags[2] = 0;
	if (func->common.arg_info) {
		n = MIN(func->common.num_args, MAX_ARG_FLAG_NUM);
		i = 0;
		while (i < n) {
			ZEND_SET_ARG_FLAG(func, i + 1, ZEND_ARG_SEND_MODE(&func->common.arg_info[i]));
			i++;
		}
		if (UNEXPECTED((func->common.fn_flags & ZEND_ACC_VARIADIC) && ZEND_ARG_SEND_MODE(&func->common.arg_info[i]))) {
			uint32_t pass_by_reference = ZEND_ARG_SEND_MODE(&func->common.arg_info[i]);
			while (i < MAX_ARG_FLAG_NUM) {
				ZEND_SET_ARG_FLAG(func, i + 1, pass_by_reference);
				i++;
			}
		}
	}
}
/* }}} */
