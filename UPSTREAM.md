# Upstream provenance

The initial runtime is derived from PHP 8.6.0beta3:

- release tag: `php-8.6.0beta3`
- release version: `8.6.0beta3` (`PHP_VERSION_ID=80600`)
- tag target commit: `fba33fcb5f9e175a3a29eda1bbb19edee5c0fc57`
- source archive SHA-256:
  `d28ad1a28c28d1f32795ab048a2494b4417f4c941b1584d46aafde731b6947db`
- local integration source: `/home/swoole/soft/php/php-8.6.0beta3`

Imported files remain in their php-src relative directories. The central value
and container algorithms below are kept byte-identical to that release:

- top-level `Zend/*.h` and `TSRM/*.h` headers;
- `Zend/zend_string.c` and `zend_hash.c`;
- `Zend/zend_sort.c`, `zend_operators.c`, `zend_gc.c`, and `zend_strtod.c`;
- `Zend/zend_list.c`.
- `main/spprintf.c`, `main/spprintf.h`, `main/snprintf.h`, and
  `main/php_globals.h`.
- `ext/bcmath/libbcmath`, including its original source, headers, tests,
  README, and license, is copied from the same PHP 8.6.0beta3 tree. Nano uses
  the library internally for PHPX high-precision fallback arithmetic; the
  public bcmath extension is not registered.

Files that select out the VM, SAPI, or operating-system branches carry narrow
`PHP_NANO` guards while retaining the upstream implementation on the normal PHP
branch. `Zend/zend_alloc.c` is necessarily adapted to the C11 allocator because
Nano does not permit the original mmap/page-management backend;
`Zend/zend_variables.c` maps the unreachable constant-AST destructor slot to a
no-op because the parser and AST runtime are absent. `Zend/zend_exceptions.c`
keeps a raised exception pending when an AOT direct call has no Zend execute
frame; PHPX immediately converts that pending exception to TypePHP's generated
C++ unwind.

PHP Nano maintains only the source-selection manifest, generated target
configuration, static-extension registry, native-call bridge, and host boundary.
`src/zend_compile_runtime.c` contains the unchanged public helper bodies selected
from PHP 8.6's `Zend/zend_compile.c`; the runtime compiler portions of that file
are not compiled into Nano.
Missing runtime behavior should normally be supplied by importing its original
php-src translation unit, not by replacing Zend algorithms locally. Interpreter,
compiler, SAPI, dynamic-loading, and unsupported host-capability units are not
selected into the final application. All derived source is distributed under
the bundled BSD-3-Clause license.
