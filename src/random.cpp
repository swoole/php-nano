/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | C++17 entropy provider for PHP's random extension.                  |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "php_nano_extension.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>

extern "C" {

PHP_NANO_API zend_result php_nano_random_bytes(void *bytes, size_t size) {
    try {
        std::random_device device;
        std::uniform_int_distribution<unsigned int> distribution(0, std::numeric_limits<unsigned char>::max());
        auto *output = static_cast<unsigned char *>(bytes);
        for (size_t index = 0; index < size; ++index) {
            output[index] = static_cast<unsigned char>(distribution(device));
        }
        return SUCCESS;
    } catch (...) {
        return FAILURE;
    }
}

PHP_NANO_API uint64_t php_nano_random_seed(void) {
    uint64_t seed;
    if (php_nano_random_bytes(&seed, sizeof(seed)) == SUCCESS) {
        return seed;
    }

    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    uintptr_t address = reinterpret_cast<uintptr_t>(&seed);
    seed = static_cast<uint64_t>(ticks) ^ static_cast<uint64_t>(address);
    seed ^= seed >> 30;
    seed *= UINT64_C(0xbf58476d1ce4e5b9);
    seed ^= seed >> 27;
    seed *= UINT64_C(0x94d049bb133111eb);
    return seed ^ (seed >> 31);
}

}  // extern "C"
