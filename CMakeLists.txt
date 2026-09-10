cmake_minimum_required(VERSION 3.19)

project(php_nano_tests LANGUAGES C CXX)

# CMake is intentionally a repository test harness only. TypePHP consumers
# compile the Composer source manifest directly and do not use this project.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/composer.json" PHP_NANO_COMPOSER_JSON)

function(php_nano_read_manifest_array output_name member_name)
    string(
        JSON entry_count
        LENGTH "${PHP_NANO_COMPOSER_JSON}"
        extra typephp-native "${member_name}"
    )
    set(entries)
    if(entry_count GREATER 0)
        math(EXPR last_index "${entry_count} - 1")
        foreach(index RANGE 0 ${last_index})
            string(
                JSON relative_path
                GET "${PHP_NANO_COMPOSER_JSON}"
                extra typephp-native "${member_name}" ${index}
            )
            list(APPEND entries "${CMAKE_CURRENT_SOURCE_DIR}/${relative_path}")
        endforeach()
    endif()
    set(${output_name} "${entries}" PARENT_SCOPE)
endfunction()

php_nano_read_manifest_array(PHP_NANO_SOURCES sources)
php_nano_read_manifest_array(PHP_NANO_INCLUDE_DIRS include-dirs)

option(
    PHP_NANO_BUILD_SMOKE
    "Build the runnable host smoke test (disable when cross-compiling)"
    ON
)

function(php_nano_configure_target target_name)
    target_include_directories(${target_name} PRIVATE ${PHP_NANO_INCLUDE_DIRS})
    target_compile_definitions(
        ${target_name}
        PRIVATE
            TYPEPHP_NATIVE=1
            PHP_NANO=1
            _POSIX_C_SOURCE=200809L
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target_name}
            PRIVATE -ffunction-sections -fdata-sections
        )
    endif()
endfunction()

# Mobile CI builds this complete archive to compile-check every production
# source without pretending that an iOS/Android executable can run on the
# GitHub host. TypePHP applications still consume composer.json directly.
add_library(php-nano-runtime STATIC ${PHP_NANO_SOURCES})
set_target_properties(php-nano-runtime PROPERTIES OUTPUT_NAME php-nano)
php_nano_configure_target(php-nano-runtime)

if(PHP_NANO_BUILD_SMOKE)
    add_executable(
        php-nano-smoke
        tests/host_call_stubs.cpp
        tests/runtime_test.cpp
        tests/smoke_test.cpp
    )
    php_nano_configure_target(php-nano-smoke)
    target_link_libraries(php-nano-smoke PRIVATE php-nano-runtime)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(APPLE)
            target_link_options(php-nano-smoke PRIVATE -Wl,-dead_strip)
        else()
            target_link_options(php-nano-smoke PRIVATE -Wl,--gc-sections)
        endif()
    endif()

    enable_testing()
    add_test(NAME php-nano-smoke COMMAND php-nano-smoke nano-argument)
endif()
