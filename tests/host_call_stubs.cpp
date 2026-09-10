/* SPDX-License-Identifier: BSD-3-Clause */

/*
 * php-nano exposes Zend's dynamic-call ABI, while the AOT host bridge owns its
 * implementation (PHPX in a TypePHP application). The standalone smoke test
 * intentionally supplies a non-dispatching host: any unexpected dynamic call
 * fails instead of silently executing interpreter code.
 */

#include "php.h"
#include "zend_API.h"

extern "C" ZEND_API zend_result zend_call_function(
    zend_fcall_info *fci, zend_fcall_info_cache *)
{
    if (fci != nullptr && fci->retval != nullptr) {
        ZVAL_UNDEF(fci->retval);
    }
    return FAILURE;
}

extern "C" ZEND_API void zend_call_known_function_ex(
    zend_function *,
    zend_object *,
    zend_class_entry *,
    zval *retval,
    uint32_t,
    zval *,
    HashTable *,
    uint32_t)
{
    if (retval != nullptr) {
        ZVAL_UNDEF(retval);
    }
}
