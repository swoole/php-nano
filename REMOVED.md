# Removed subsystems

The native runtime does not contain or link:

- the runtime PHP compiler, parser/scanner, opcode execution loop, and Zend VM
  handlers; the executor data structures and internal-call APIs needed by AOT
  functions remain;
- runtime code evaluation and source loading: `eval`, `include`,
  `include_once`, `require`, and `require_once`;
- SAPI, automatic filesystem discovery/loading of `php.ini`, dynamic module
  loading, and runtime extension loading;
- `Zend/Optimizer` and its op_array optimization passes;
- socket transports, remote and user-defined stream wrappers, sockets, DNS,
  processes, shell execution, signals, or dynamic library loading; local
  `file` and `glob` streams remain available;
- external PCRE2, ICU, OpenSSL, zlib, libxml, SQLite, curl, or any other
  third-party library;
- platform thread or event-loop libraries.

The built-in modules retain only APIs inside the C11/C++17/POSIX capability
policy (and the smaller WASI subset). For example,
SPL container classes remain while its filesystem iterators are currently
removed; standard file/directory/stat and hash file/stream functions remain.
Other extensions are installed as Composer source packages and linked into
the application at build time.

The compiler may still use a normal PHP installation. This document describes
only dependencies of the generated native artifact.

The vendored tree intentionally omits the parser-generator inputs and generated
token headers (`zend_language_parser.y`, `zend_language_parser.h`,
`zend_language_scanner.l`, and `zend_language_scanner.h`), together with the
unused `zend_compile.c`, `zend_ast.c`, and `zend_highlight.c` implementations.
Runtime ABI headers such as `zend_compile.h` and `zend_ast.h` remain because
class metadata, Reflection, and executor structures expose their types even
without a parser. Nano's required non-compiler helpers are selected into
`src/zend_compile_runtime.c`.

All upstream `.phpt` files are omitted. They require the PHP CLI, source
compiler, and ZendVM test runner that Nano deliberately does not provide.
Executable Nano coverage lives in `tests/*.cpp` and runs through CTest.
