/* SPDX-License-Identifier: BSD-3-Clause */

#include "php_nano_extension.h"

#include <cstdio>
#include <exception>

extern "C" int typephp_nano_project_main();

extern "C" zend_module_entry basic_functions_module;
extern "C" zend_module_entry date_module_entry;
extern "C" zend_module_entry hash_module_entry;
extern "C" zend_module_entry json_module_entry;
extern "C" zend_module_entry pcre_module_entry;
extern "C" zend_module_entry spl_module_entry;
extern "C" zend_module_entry reflection_module_entry;
extern "C" zend_module_entry random_module_entry;
extern "C" zend_module_entry filter_module_entry;

extern "C" zend_result php_nano_startup_composer_extensions()
{
    static zend_module_entry *extensions[] = {
        &basic_functions_module,
        &date_module_entry,
        &hash_module_entry,
        &json_module_entry,
        &pcre_module_entry,
        &spl_module_entry,
        &reflection_module_entry,
        &random_module_entry,
        &filter_module_entry,
    };
    return php_nano_startup_extensions(
        extensions, sizeof(extensions) / sizeof(extensions[0]));
}

extern "C" void php_nano_shutdown_composer_extensions()
{
    php_nano_shutdown_extensions();
}

int main(int argc, char **argv)
{
    php_nano_set_cli_arguments(argc, argv);
    if (php_nano_startup_composer_extensions() != SUCCESS) {
        std::fputs("php-nano smoke startup failed\n", stderr);
        return 1;
    }

    int exit_code = 1;
    try {
        exit_code = typephp_nano_project_main();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "php-nano smoke exception: %s\n", exception.what());
    } catch (...) {
        std::fputs("php-nano smoke exception\n", stderr);
    }

    php_nano_shutdown_composer_extensions();
    return exit_code;
}
