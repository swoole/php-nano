/*
   +----------------------------------------------------------------------+
   | PHP Nano                                                            |
   +----------------------------------------------------------------------+
   | C++17 clock and sleep providers for PHP's standard time functions. |
   | SPDX-License-Identifier: BSD-3-Clause                               |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "php_nano_extension.h"

extern "C" {
#include "ext/random/php_random.h"
}

#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <thread>

namespace {

using SystemClock = std::chrono::system_clock;
using SteadyClock = std::chrono::steady_clock;

std::chrono::nanoseconds system_time_since_epoch()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        SystemClock::now().time_since_epoch());
}

double system_time_seconds()
{
    return std::chrono::duration<double>(SystemClock::now().time_since_epoch()).count();
}

} // namespace

extern "C" {

PHP_NANO_API int64_t php_nano_system_time_seconds(void)
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        SystemClock::now().time_since_epoch()).count();
}

PHP_NANO_API void php_nano_system_time(int64_t *seconds, int32_t *microseconds)
{
    const auto elapsed = system_time_since_epoch();
    const auto whole_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    const auto fractional = std::chrono::duration_cast<std::chrono::microseconds>(
        elapsed - whole_seconds);
    *seconds = whole_seconds.count();
    *microseconds = static_cast<int32_t>(fractional.count());
}

PHP_NANO_API zend_string *php_nano_unique_id(zend_string *prefix, bool more_entropy)
{
    int64_t seconds;
    int32_t microseconds;
    php_nano_system_time(&seconds, &microseconds);

    const char *prefix_value = prefix ? ZSTR_VAL(prefix) : "";
    const auto seconds_value = static_cast<uint64_t>(seconds);
    const auto microseconds_value = static_cast<unsigned int>(microseconds) % 0x100000U;

    if (more_entropy) {
        uint32_t bytes;
        if (php_random_bytes_silent(&bytes, sizeof(bytes)) == FAILURE) {
            bytes = static_cast<uint32_t>(php_random_generate_fallback_seed());
        }
        const double seed = (static_cast<double>(bytes) / UINT32_MAX) * 10.0;
        return zend_strpprintf(
            0,
            "%s%08" PRIx64 "%05x%.8F",
            prefix_value,
            seconds_value,
            microseconds_value,
            seed);
    }

    return zend_strpprintf(
        0,
        "%s%08" PRIx64 "%05x",
        prefix_value,
        seconds_value,
        microseconds_value);
}

ZEND_FUNCTION(uniqid)
{
    zend_string *prefix = nullptr;
    bool more_entropy = false;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(prefix)
        Z_PARAM_BOOL(more_entropy)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_STR(php_nano_unique_id(prefix, more_entropy));
}

ZEND_FUNCTION(sleep)
{
    zend_long seconds;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(seconds)
    ZEND_PARSE_PARAMETERS_END();

    if (seconds < 0) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    RETURN_LONG(0);
}

ZEND_FUNCTION(usleep)
{
    zend_long microseconds;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(microseconds)
    ZEND_PARSE_PARAMETERS_END();

    if (microseconds < 0) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }

    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

ZEND_FUNCTION(time_nanosleep)
{
    zend_long seconds;
    zend_long nanoseconds;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(seconds)
        Z_PARAM_LONG(nanoseconds)
    ZEND_PARSE_PARAMETERS_END();

    if (seconds < 0) {
        zend_argument_value_error(1, "must be greater than or equal to 0");
        RETURN_THROWS();
    }
    if (nanoseconds < 0 || nanoseconds > 999999999) {
        zend_argument_value_error(2, "must be between 0 and 999999999");
        RETURN_THROWS();
    }

    std::this_thread::sleep_for(
        std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanoseconds));
    RETURN_TRUE;
}

ZEND_FUNCTION(time_sleep_until)
{
    double timestamp;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_DOUBLE(timestamp)
    ZEND_PARSE_PARAMETERS_END();

    const double now = system_time_seconds();
    if (!std::isfinite(timestamp) || timestamp <= now) {
        zend_argument_value_error(1, "must be greater than the current time");
        RETURN_THROWS();
    }

    std::this_thread::sleep_until(SystemClock::time_point(
        std::chrono::duration_cast<SystemClock::duration>(std::chrono::duration<double>(timestamp))));
    RETURN_TRUE;
}

ZEND_FUNCTION(hrtime)
{
    bool as_number = false;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(as_number)
    ZEND_PARSE_PARAMETERS_END();

    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        SteadyClock::now().time_since_epoch()).count();

    if (as_number) {
        RETURN_LONG(static_cast<zend_long>(nanoseconds));
    }

    zval seconds;
    zval remainder;
    ZVAL_LONG(&seconds, static_cast<zend_long>(nanoseconds / 1000000000));
    ZVAL_LONG(&remainder, static_cast<zend_long>(nanoseconds % 1000000000));
    RETURN_ARR(zend_new_pair(&seconds, &remainder));
}

ZEND_FUNCTION(microtime)
{
    bool as_float = false;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(as_float)
    ZEND_PARSE_PARAMETERS_END();

    const auto elapsed = system_time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed - seconds);

    if (as_float) {
        RETURN_DOUBLE(static_cast<double>(seconds.count())
            + static_cast<double>(microseconds.count()) / 1000000.0);
    }

    RETURN_STR(zend_strpprintf(
        0,
        "%.8F %lld",
        static_cast<double>(microseconds.count()) / 1000000.0,
        static_cast<long long>(seconds.count())));
}

ZEND_FUNCTION(gettimeofday)
{
    bool as_float = false;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(as_float)
    ZEND_PARSE_PARAMETERS_END();

    const auto elapsed = system_time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed - seconds);

    if (as_float) {
        RETURN_DOUBLE(static_cast<double>(seconds.count())
            + static_cast<double>(microseconds.count()) / 1000000.0);
    }

    array_init(return_value);
    add_assoc_long(return_value, "sec", static_cast<zend_long>(seconds.count()));
    add_assoc_long(return_value, "usec", static_cast<zend_long>(microseconds.count()));
    /* Nano's process-independent default timezone is UTC. */
    add_assoc_long(return_value, "minuteswest", 0);
    add_assoc_long(return_value, "dsttime", 0);
}

} // extern "C"
