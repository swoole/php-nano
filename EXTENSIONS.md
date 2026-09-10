# Static Composer extensions

PHP Nano retains an extension lifecycle but has no dynamic loader. The set of
extensions is fully determined by Composer before compilation.

Core, date, hash, json, pcre, random, Reflection, SPL, standard, and
filter belong to PHP Nano's fixed built-in layer. They are not Composer
extension packages and cannot be replaced by one. Their registered API is
restricted by [CONSTRAINTS.md](CONSTRAINTS.md).

The source-composition build registers the complete fixed layer. Standard
arginfo and function tables are generated from PHP's guarded
`basic_functions.stub.php`; date retains PHP's timelib and generated DateTime
class tables; hash retains the in-memory and file/stream algorithms; JSON uses PHP 8.6's
original encoder and generated parser/scanner; PCRE uses the bundled PCRE2
sources with JIT disabled; random retains PHP's engines and obtains entropy
through C++17 `std::random_device`; and the pure-value portions of Reflection,
SPL, and filter retain their upstream class/function implementations.

The standard subset keeps its upstream APIs for local files, directories,
stat, the file-only stream layer, Base64 and URL encoding/decoding,
`parse_url`, serialization, variable inspection, version comparison,
constants, and `uniqid`. Both direct TypePHP calls and
Zend function-table dispatch use the same implementations. `uniqid` obtains
time and optional entropy from Nano's C++17 clock/random adapters.

PHP 8.6's bundled `ext/bcmath/libbcmath` sources are also compiled into the
fixed runtime as PHPX's private arithmetic backend for `BigInt`, `BigFloat`,
and `Decimal`. This source-only helper is not a registered Zend extension:
`php_nano_find_extension("bcmath")` returns null and the public `bc*()` API is
absent. A future `swoole/php-ext-bcmath` package may expose that public API, but
must reuse the runtime backend and must not compile a second copy of the
libbcmath globals into one application.

Capability-dependent APIs are absent rather than registered as failing stubs:

- PCRE JIT is not registered; hash file/stream functions are registered and
  use the file-only stream layer;
- SPL filesystem classes and its include-based default autoloader are not
  registered;
- `ReflectionFiber`, `ReflectionGenerator`, and
  `ReflectionExtension::info()` are not registered; `ReflectionReference`
  remains available for Nano's retained reference semantics;
- filter SAPI-input functions and URI validation are not registered. Value
  filtering, sanitizing, regex validation, and callbacks remain available.
- `parse_str` is not registered because PHP implements it through SAPI input
  handling; URL encoding and `parse_url` remain available without SAPI.

Native extension packages must:

- use the package name `swoole/php-ext-*`;
- require an ABI-compatible `swoole/php-nano`;
- publish `extra.typephp-native` with `kind: extension`;
- list exact source and include paths;
- export the original `zend_module_entry` symbol named by
  `extension.module-entry`.

The package suffix matches the extension name, with underscores represented as
hyphens: extension `pdo_mysql` is packaged as `swoole/php-ext-pdo-mysql`.

For example:

```json
{
  "name": "swoole/php-ext-example",
  "extra": {
    "typephp-native": {
      "kind": "extension",
      "abi": 80600,
      "c-standard": 11,
      "cxx-standard": 17,
      "include-dirs": ["include"],
      "sources": ["ext/example/example.c"],
      "extension": {
        "name": "example",
        "module-entry": "example_module_entry"
      }
    }
  }
}
```

TypePHP discovers all installed packages with this metadata, validates their
ABI, compiles their sources into the application, and generates a fixed
extension table. Startup follows table order; shutdown runs in reverse order.
Both `.c` and C++ source entries are accepted; PHP extension `.c` files remain C
translation units. Original `zend_module_dep` requirements are honored, so dependencies start
before dependents even when Composer discovery order differs. Duplicate names,
missing/cyclic dependencies, conflicts, and ABI mismatches fail startup/build.

The registry consumes PHP 8.6 `zend_module_entry` directly and runs the normal
MINIT/RINIT/RSHUTDOWN/MSHUTDOWN callbacks. Extension ports should preserve
their PHP 8.6 source files without local edits
where practical. Native-only source selection and registration adapters live
outside the upstream source tree. If unchanged extension code needs a missing
Zend symbol or structure, the compatibility is added to PHP Nano instead of
being patched independently into every extension.

This mechanism deliberately has no `dlopen`, shared extension modules,
directory scanning, `php.ini`, or runtime loading API.
