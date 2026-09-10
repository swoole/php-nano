# ABI policy

The project starts at runtime version 8.6 and follows PHP 8.6 names and basic
data layouts where they reduce PHPX migration cost. This is a source-level
compatibility strategy, not a promise of binary compatibility with `libphp`.

`php-nano` and PHPX native sources are compiled into the same final target by
the same C++17 toolchain. No intermediate runtime archive defines a separate
binary boundary. Before TypePHP 1.0, the native ABI may change whenever the
runtime can become smaller or safer.
