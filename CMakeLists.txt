cmake_minimum_required(VERSION 3.19)

project(php_nano_tests LANGUAGES C CXX)

include(CheckIncludeFile)

# CMake is intentionally a repository test harness only. TypePHP consumers
# compile the Composer source manifest directly and do not use this project.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/composer.json" PHP_NANO_COMPOSER_JSON)

# Reads a path array. The member is optional, so a platform block the manifest
# does not define yields an empty list instead of a configure error.
function(php_nano_read_manifest_array output_name)
    string(
        JSON entry_count
        ERROR_VARIABLE member_error
        LENGTH "${PHP_NANO_COMPOSER_JSON}"
        extra typephp-native ${ARGN}
    )
    set(entries)
    if(NOT member_error AND entry_count GREATER 0)
        math(EXPR last_index "${entry_count} - 1")
        foreach(index RANGE 0 ${last_index})
            string(
                JSON relative_path
                GET "${PHP_NANO_COMPOSER_JSON}"
                extra typephp-native ${ARGN} ${index}
            )
            list(APPEND entries "${CMAKE_CURRENT_SOURCE_DIR}/${relative_path}")
        endforeach()
    endif()
    set(${output_name} "${entries}" PARENT_SCOPE)
endfunction()

# Reads a string array. Optional for the same reason as the path array above.
function(php_nano_read_manifest_string_array output_name)
    string(
        JSON entry_count
        ERROR_VARIABLE member_error
        LENGTH "${PHP_NANO_COMPOSER_JSON}"
        extra typephp-native ${ARGN}
    )
    set(entries)
    if(NOT member_error AND entry_count GREATER 0)
        math(EXPR last_index "${entry_count} - 1")
        foreach(index RANGE 0 ${last_index})
            string(
                JSON entry
                GET "${PHP_NANO_COMPOSER_JSON}"
                extra typephp-native ${ARGN} ${index}
            )
            list(APPEND entries "${entry}")
        endforeach()
    endif()
    set(${output_name} "${entries}" PARENT_SCOPE)
endfunction()

php_nano_read_manifest_array(PHP_NANO_SOURCES sources)
php_nano_read_manifest_array(PHP_NANO_INCLUDE_DIRS include-dirs)
php_nano_read_manifest_string_array(PHP_NANO_DEFINES defines)
php_nano_read_manifest_string_array(
    PHP_NANO_STANDALONE_TEST_EXCLUDE_COMPONENTS
    standalone-test-exclude-components
)

# Platform-scoped members mirror the composition a TypePHP consumer selects for
# the host; the key matches ComposerNativePackage::platformKey().
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PHP_NANO_PLATFORM windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PHP_NANO_PLATFORM macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PHP_NANO_PLATFORM ios)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(PHP_NANO_PLATFORM android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(PHP_NANO_PLATFORM wasip2)
else()
    set(PHP_NANO_PLATFORM linux)
endif()

php_nano_read_manifest_array(
    PHP_NANO_PLATFORM_SOURCES
    platforms ${PHP_NANO_PLATFORM} sources
)
php_nano_read_manifest_array(
    PHP_NANO_PLATFORM_INCLUDE_DIRS
    platforms ${PHP_NANO_PLATFORM} include-dirs
)
php_nano_read_manifest_string_array(
    PHP_NANO_PLATFORM_DEFINES
    platforms ${PHP_NANO_PLATFORM} defines
)

# Check the complete Composer manifest before narrowing it for this standalone
# harness. This keeps CI able to catch a source path that was added to the
# manifest but omitted from the repository.
foreach(source_path IN LISTS PHP_NANO_SOURCES PHP_NANO_PLATFORM_SOURCES)
    if(NOT EXISTS "${source_path}")
        message(FATAL_ERROR "Composer source does not exist: ${source_path}")
    endif()
endforeach()

# Components that depend on a consumer-provided network/TLS SDK are compiled
# by the final TypePHP application. The repository harness has no such SDK,
# especially while cross-compiling for iOS, Android, and WASI, so remove those
# component sources from its core smoke archive.
foreach(component_name IN LISTS PHP_NANO_STANDALONE_TEST_EXCLUDE_COMPONENTS)
    string(
        JSON component_source_count
        LENGTH "${PHP_NANO_COMPOSER_JSON}"
        extra typephp-native components "${component_name}" sources
    )
    if(component_source_count GREATER 0)
        math(EXPR component_last_source "${component_source_count} - 1")
        foreach(source_index RANGE 0 ${component_last_source})
            string(
                JSON component_source
                GET "${PHP_NANO_COMPOSER_JSON}"
                extra typephp-native components "${component_name}" sources
                ${source_index}
            )
            list(
                REMOVE_ITEM PHP_NANO_SOURCES
                "${CMAKE_CURRENT_SOURCE_DIR}/${component_source}"
            )
            list(
                REMOVE_ITEM PHP_NANO_PLATFORM_SOURCES
                "${CMAKE_CURRENT_SOURCE_DIR}/${component_source}"
            )
        endforeach()
    endif()
endforeach()

# Remaining components still contribute sources via the top-level manifest.
# Apply their defines too, matching TypePHP's selected-component composition.
string(
    JSON php_nano_component_count
    LENGTH "${PHP_NANO_COMPOSER_JSON}"
    extra typephp-native components
)
if(php_nano_component_count GREATER 0)
    math(EXPR php_nano_component_last "${php_nano_component_count} - 1")
    foreach(php_nano_component_index RANGE 0 ${php_nano_component_last})
        string(
            JSON php_nano_component_name
            MEMBER "${PHP_NANO_COMPOSER_JSON}"
            extra typephp-native components ${php_nano_component_index}
        )
        if(php_nano_component_name IN_LIST PHP_NANO_STANDALONE_TEST_EXCLUDE_COMPONENTS)
            continue()
        endif()
        php_nano_read_manifest_string_array(
            php_nano_component_defines
            components "${php_nano_component_name}" defines
        )
        list(APPEND PHP_NANO_DEFINES ${php_nano_component_defines})
    endforeach()
endif()

option(
    PHP_NANO_BUILD_SMOKE
    "Build the runnable host smoke test (disable when cross-compiling)"
    ON
)

check_include_file("sys/socket.h" PHP_NANO_HAVE_SYS_SOCKET_H)
check_include_file("netinet/in.h" PHP_NANO_HAVE_NETINET_IN_H)

function(php_nano_configure_target target_name)
    target_include_directories(${target_name} PRIVATE ${PHP_NANO_INCLUDE_DIRS})
    target_include_directories(
        ${target_name}
        PRIVATE ${PHP_NANO_PLATFORM_INCLUDE_DIRS}
    )
    target_compile_definitions(
        ${target_name}
        PRIVATE
            TYPEPHP_NATIVE=1
            PHP_NANO=1
            _POSIX_C_SOURCE=200809L
            ${PHP_NANO_DEFINES}
            ${PHP_NANO_PLATFORM_DEFINES}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
        target_compile_definitions(${target_name} PRIVATE ZEND_MM_ERROR=0)
    endif()
    if(PHP_NANO_HAVE_SYS_SOCKET_H)
        target_compile_definitions(${target_name} PRIVATE HAVE_SYS_SOCKET_H=1)
    endif()
    if(PHP_NANO_HAVE_NETINET_IN_H)
        target_compile_definitions(${target_name} PRIVATE HAVE_NETINET_IN_H=1)
    endif()
    if(MSVC)
        # Match TypePHP's MSVC driver: utf-8 sources, the conforming preprocessor
        # (so __has_attribute / __has_feature work), and large generated TUs.
        target_compile_options(
            ${target_name}
            PRIVATE /utf-8 /Zc:preprocessor /bigobj
        )
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target_name}
            PRIVATE -ffunction-sections -fdata-sections
        )
        if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
            target_compile_options(
                ${target_name}
                PRIVATE
                    -fwasm-exceptions
                    "SHELL:-mllvm -wasm-enable-sjlj"
                    "SHELL:-mllvm -wasm-use-legacy-eh=false"
            )
        endif()
    endif()
endfunction()

# Mobile CI builds this core archive without pretending that an iOS/Android
# executable can run on the GitHub host. TypePHP applications consume the
# complete composer.json manifest and select optional components themselves.
add_library(
    php-nano-runtime
    STATIC
    ${PHP_NANO_SOURCES}
    ${PHP_NANO_PLATFORM_SOURCES}
)
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

    # The Windows platform sources call the Win32 API directly.
    if(WIN32)
        target_link_libraries(
            php-nano-smoke
            PRIVATE advapi32 bcrypt pathcch shell32 user32 ws2_32
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
            set_target_properties(php-nano-smoke PROPERTIES SUFFIX ".wasm")
            target_link_options(
                php-nano-smoke
                PRIVATE -fwasm-exceptions -Wl,--gc-sections
            )
            target_link_libraries(php-nano-smoke PRIVATE setjmp unwind)
        elseif(APPLE)
            target_link_options(php-nano-smoke PRIVATE -Wl,-dead_strip)
        else()
            target_link_options(php-nano-smoke PRIVATE -Wl,--gc-sections)
        endif()
    endif()

    enable_testing()
    add_test(NAME php-nano-smoke COMMAND php-nano-smoke nano-argument)
endif()
