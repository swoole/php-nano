# PHP Nano

`php-nano` is TypePHP's small native runtime. It preserves the
PHP/Zend names and data structures needed by PHPX while removing the PHP
interpreter, Zend VM, SAPI, network streams, and general operating-system service APIs. Its
sources are compiled directly into a TypePHP native executable in
place of `libphp`.

The compiler remains a build-time tool: PHP, Composer, and `nikic/php-parser`
are not linked into the generated native program.

## Runtime composition

- PHP 8.6 `zval`, `zend_string`, `Bucket`, and `zend_array` definitions;
- PHP 8.6 implementations of strings, HashTable, variables, sorting,
  operators, numeric conversion, GC, and resource lists; the allocator keeps
  Zend's API and algorithms but selects a C11 `malloc` backend instead of the
  upstream mmap/page-management backend;
- native class metadata and `zend_call_function`-compatible virtual method
  dispatch through compiler-generated C++ trampolines;
- PHP output buffering with a native/WASI console writer;
- PHP's stream core restricted to local `file` and `glob` wrappers, including
  standard file/directory/stat and hash file/stream APIs;
- PHP 8.6 timelib and DateTime APIs, with clocks and sleeps supplied through
  the C++17 standard library;
- generated target configuration plus the original PHP 8.6 `php.h`;
- direct native and `wasm32-wasip2` source builds;
- no application dependency beyond the selected C/C++ toolchain runtime.

PHP Nano contains a fixed built-in PHP layer: Core, date, hash, json, pcre,
random, Reflection, SPL, standard, and filter. Their PHP 8.6 source trees
are vendored directly. Nano registers only the subset that satisfies its host
capability boundary. Other PHP extensions are Composer source packages using
the `swoole/php-ext-*` naming convention. The final extension set is fixed at
build time; dynamic libraries and runtime extension loading are not supported.
The Zend INI registry and module-defined settings are retained, while automatic
discovery/loading of `php.ini` remains disabled. Application file access does
not imply runtime configuration or extension discovery.

The `ctype_*` functions are compiler intrinsics backed directly by PHPX and
the C++ standard character-classification API. They do not require or register
PHP's ctype extension.

PHPX high-precision values use the normal GMP, MPFR, and mpdecimal backends in
a regular build. Nano instead compiles PHP 8.6's bundled libbcmath source as a
private fallback backend, without registering the public bcmath extension or
adding an external library dependency. Its documented precision differences
are listed in [SUPPORTED.md](SUPPORTED.md).

The normative dependency and capability rules are in
[CONSTRAINTS.md](CONSTRAINTS.md); its Native POSIX baseline, WASI subset, and
forbidden host capabilities are defined in
[PORTABILITY_ALLOWLIST.md](PORTABILITY_ALLOWLIST.md). See
[EXTENSIONS.md](EXTENSIONS.md) for the Composer package contract and
upstream-source policy.

PHP Nano does not contain a copied PHPX facade. Composer installs the normal
`swoole/phpx` sources beside this package. TypePHP compiles those sources with
`PHPX_NANO`, which removes APIs such as `eval`, `include`, and `require` while
keeping the existing PHPX value classes and implementation.

## Build integration

This package deliberately has no independent production build system. It is
installed by Composer and publishes an exact native source manifest in
`composer.json`.
TypePHP loads the listed sources directly into the application's source set.
It compiles PHP `.c` files as C11, compiles PHPX and generated TypePHP
sources as C++17, and performs the final link with the C++ linker.

The repository's `CMakeLists.txt` is only a CI/developer test harness. It reads
the same Composer manifest and compiles all production sources into the
`php-nano-runtime` test archive. Host builds also link `php-nano-smoke` and run
it through CTest; WASI Preview 2 links the same smoke program and executes it
with Wasmtime. iOS and Android CI compile-check the complete archive without
trying to execute a cross-compiled program. The smoke executable uses a
non-dispatching test host for the two dynamic-call ABI hooks that PHPX
implements in a real TypePHP program. It is not installed or invoked by
TypePHP applications. There is no CMake,
Autoconf, Automake, `configure`, or intermediate runtime archive in the native
application build path. The current TypePHP source builder exposes
the host-native and `wasm32-wasip2` targets. CI exercises Linux and macOS host
builds, WASI Preview 2 build/execution, plus iPhoneOS `arm64` and Android
`arm64-v8a` cross-compilation. Windows is intentionally not a php-nano runtime
target: TypePHP uses its full PHP/PHPX DLL distribution and applies the Nano
restrictions in the compiler.

For a standalone repository check:

```shell
cmake -S . -B build/tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```
