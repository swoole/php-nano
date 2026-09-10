# Supported targets and dependency contract

The currently implemented and tested targets compile the source directly into
the final application as C11 and C++17 translation units:

- host-native Linux;
- `wasm32-wasip2` through WASI SDK.

The Native runtime contract targets Linux, macOS, Android NDK, and Apple SDK
toolchains. Cross-toolchain profiles require their own build and execution
tests before they are listed as implemented. Windows deliberately uses the
complete PHP/PHPX DLL runtime instead of php-nano.

Only C11, C++17, POSIX.1-2008, and compiler runtime facilities are permitted;
the runtime may not add third-party link dependencies. CMake and other project
generators are not build dependencies. The WASI target is a smaller capability
subset and does not use mmap or signal emulation libraries. PHP Nano exposes
local filesystem access through its file-only PHP stream layer and target
C/POSIX/WASI filesystem surface. It does not expose network, process, shell,
socket, remote-stream, or dynamic-loader APIs. Socket capability remains absent
even on POSIX hosts. The Zend INI registry remains available; Nano does not scan
the filesystem for a `php.ini` file.

On WASI, unsupported user-visible calls are removed from generated arginfo and
direct TypePHP calls fail at compile time. Internal compatibility symbols may
return `ENOTSUP` only when needed to link retained php-src core code; those
symbols are never registered as callable PHP APIs.

PHP Nano retains a static extension registry with startup, reverse-order
shutdown, ABI validation, duplicate-name rejection, and lookup by name. TypePHP
generates the registry from Composer package metadata. It never scans the
filesystem or loads an extension at runtime.

The registered stream wrappers are exactly `file` and `glob`. Standard
file/directory/stat functions and the hash extension's file/stream operations
use these streams. Socket transports, remote wrappers, process pipes, and
runtime registration of user stream wrappers are unavailable.

The registered built-in API includes the explicit safe subsets of Core, date,
hash, JSON, PCRE, random, Reflection, SPL, standard, and filter. Safe standard
functions are present in the Zend function table as well as PHPX's direct-call
facade, so variable-function calls preserve normal PHP dispatch semantics.

PHPX's `BigInt`, `BigFloat`, and `Decimal` value types remain available without
GMP, MPFR, or mpdecimal. In a Nano build they use PHP 8.6's bundled libbcmath as
an internal source-only arithmetic backend. This does not register PHP's public
`bcmath` extension or its `bc*()` functions. `BigInt` remains arbitrary
precision for integer operations. The fallback stores `BigFloat` results with
up to 64 fractional decimal places and `Decimal` results with up to 50
fractional decimal places; digits beyond those limits are truncated by
libbcmath. Nano `Decimal::pow()` accepts integer exponents only. These limits
are deliberate fallback semantics, not claims of bit-for-bit equivalence with
the normal MPFR/mpdecimal backends.

Fiber and Generator are excluded from the registered class set. TypePHP rejects
`yield` and `yield from` while compiling a Nano target, including generator
closures and arrow functions. Ordinary closures and synchronous object dispatch
remain available.
