/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | Static Composer extension registry.                                 |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#include "php_nano_extension.h"

#include <cstring>
#include <vector>

namespace {

std::vector<zend_module_entry *> &started_extensions() {
    static std::vector<zend_module_entry *> extensions;
    return extensions;
}

size_t &request_started_count() {
    static size_t count = 0;
    return count;
}

zend_module_entry *find_available(
    zend_module_entry *const *extensions, size_t count, const char *name) {
    for (size_t index = 0; index < count; ++index) {
        if (extensions[index] != nullptr && extensions[index]->name != nullptr
            && std::strcmp(extensions[index]->name, name) == 0) {
            return extensions[index];
        }
    }
    return nullptr;
}

enum class DependencyState {
    Ready,
    Waiting,
    Invalid,
};

DependencyState dependency_state(
    const zend_module_entry *extension,
    zend_module_entry *const *extensions,
    size_t count) {
    if (extension->deps == nullptr) {
        return DependencyState::Ready;
    }
    for (const zend_module_dep *dependency = extension->deps;
         dependency->name != nullptr;
         ++dependency) {
        zend_module_entry *available = find_available(
            extensions, count, dependency->name);
        if (dependency->type == MODULE_DEP_CONFLICTS) {
            if (available != nullptr) {
                return DependencyState::Invalid;
            }
            continue;
        }
        if (dependency->type != MODULE_DEP_REQUIRED
            && dependency->type != MODULE_DEP_OPTIONAL) {
            return DependencyState::Invalid;
        }
        if (available == nullptr) {
            if (dependency->type == MODULE_DEP_REQUIRED) {
                return DependencyState::Invalid;
            }
            continue;
        }
        if (available->module_started == 0) {
            return DependencyState::Waiting;
        }
    }
    return DependencyState::Ready;
}

zend_result start_extension(zend_module_entry *extension, int module_number) {
    (void) module_number;
    extension = zend_register_module_ex(extension, MODULE_PERSISTENT);
    if (extension == nullptr || zend_startup_module_ex(extension) != SUCCESS) {
        return FAILURE;
    }
    started_extensions().push_back(extension);
    return SUCCESS;
}

} // namespace

extern "C" {

PHP_NANO_API zend_result php_nano_startup_extensions(
    zend_module_entry *const *extensions, size_t count) {
    if (php_nano_startup_core() != SUCCESS) {
        return FAILURE;
    }
    auto &started = started_extensions();
    if (!started.empty()) {
        return FAILURE;
    }

    std::vector<zend_module_entry *> available;
    available.reserve(count);
    available.insert(available.end(), extensions, extensions + count);

    const size_t available_count = available.size();
    for (size_t index = 0; index < available_count; ++index) {
        zend_module_entry *extension = available[index];
        if (extension == nullptr || extension->size != sizeof(zend_module_entry)
            || extension->zend_api != ZEND_MODULE_API_NO
            || extension->zend_debug != ZEND_DEBUG || extension->zts != USING_ZTS
            || extension->name == nullptr || extension->handle != nullptr) {
            php_nano_shutdown_extensions();
            return FAILURE;
        }
        extension->module_started = 0;
        for (size_t other = 0; other < index; ++other) {
            if (std::strcmp(available[other]->name, extension->name) == 0) {
                php_nano_shutdown_extensions();
                return FAILURE;
            }
        }
    }

    while (started.size() < available_count) {
        bool progressed = false;
        for (size_t index = 0; index < available_count; ++index) {
            zend_module_entry *extension = available[index];
            if (extension->module_started != 0) {
                continue;
            }
            const DependencyState state = dependency_state(
                extension, available.data(), available_count);
            if (state == DependencyState::Invalid) {
                php_nano_shutdown_extensions();
                return FAILURE;
            }
            if (state == DependencyState::Waiting) {
                continue;
            }
            if (start_extension(extension, static_cast<int>(index)) != SUCCESS) {
                php_nano_shutdown_extensions();
                return FAILURE;
            }
            progressed = true;
        }
        if (!progressed) {
            php_nano_shutdown_extensions();
            return FAILURE;
        }
    }

    if (php_nano_activate_core() != SUCCESS) {
        php_nano_shutdown_extensions();
        return FAILURE;
    }
    request_started_count() = 0;
    for (zend_module_entry *extension : started) {
        if (extension->request_startup_func != nullptr &&
            extension->request_startup_func(extension->type, extension->module_number) != SUCCESS) {
            php_nano_shutdown_extensions();
            return FAILURE;
        }
        ++request_started_count();
    }
    return SUCCESS;
}

PHP_NANO_API void php_nano_shutdown_extensions(void) {
    auto &started = started_extensions();
    size_t &request_count = request_started_count();
    while (request_count > 0) {
        zend_module_entry *extension = started[--request_count];
        if (extension->request_shutdown_func != nullptr) {
            extension->request_shutdown_func(extension->type, extension->module_number);
        }
    }
    php_nano_deactivate_core();
    while (!started.empty()) {
        zend_module_entry *extension = started.back();
        started.pop_back();
        if (extension->module_shutdown_func != nullptr) {
            extension->module_shutdown_func(extension->type, extension->module_number);
        }
        if (extension->globals_dtor != nullptr && extension->globals_ptr != nullptr) {
            extension->globals_dtor(extension->globals_ptr);
        }
        extension->module_started = 0;
    }
    php_nano_shutdown_core();
}

PHP_NANO_API zend_module_entry *php_nano_find_extension(const char *name) {
    if (name == nullptr) {
        return nullptr;
    }
    for (zend_module_entry *extension : started_extensions()) {
        if (std::strcmp(extension->name, name) == 0) {
            return extension;
        }
    }
    return nullptr;
}

} // extern "C"
