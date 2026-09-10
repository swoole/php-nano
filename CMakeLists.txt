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

add_executable(
    php-nano-smoke
    ${PHP_NANO_SOURCES}
    tests/host_call_stubs.cpp
    tests/runtime_test.cpp
    tests/smoke_test.cpp
)

target_include_directories(php-nano-smoke PRIVATE ${PHP_NANO_INCLUDE_DIRS})
target_compile_definitions(
    php-nano-smoke
    PRIVATE
        TYPEPHP_NATIVE=1
        PHP_NANO=1
        _POSIX_C_SOURCE=200809L
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(php-nano-smoke PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(php-nano-smoke PRIVATE -Wl,-dead_strip)
    else()
        target_link_options(php-nano-smoke PRIVATE -Wl,--gc-sections)
    endif()
endif()

enable_testing()
add_test(NAME php-nano-smoke COMMAND php-nano-smoke nano-argument)
