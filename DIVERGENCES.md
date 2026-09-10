# Intentional divergences from PHP

- `zend_long` is always signed 64-bit, including wasm32, to preserve TypePHP's
  integer semantics across targets.
- `zend_call_function` is implemented by the PHPX TypePHP Nano helper. It
  dispatches registered internal/AOT handlers only; it does not execute
  opcodes or evaluate/load PHP source.
- Runtime AST conversion to partial/first-class callables is rejected because
  upstream implements it by compiling a new op-array (and optionally caching
  it in Opcache).
- Zend MM retains its upstream allocation algorithms, but its platform page
  allocator is replaced by C11 `aligned_alloc()`/`free()`. Huge-page advice and
  in-place mapping growth/truncation are unavailable.
- Zend bailout uses C11 `setjmp()`/`longjmp()` instead of POSIX
  `sigsetjmp()`/`siglongjmp()`.
- Execution timers and signal handling are not compiled for Nano targets.
- PHP wall-clock, monotonic-clock, and sleep functions use the C++17
  `<chrono>` and `<thread>` APIs. PHP's date parsing, calendar arithmetic,
  timezone database, and DateTime classes continue to use the vendored PHP
  8.6 timelib sources.
- Zend allocation uses the C11 allocation API for Nano targets; OS virtual
  memory mapping and page-advice paths are not compiled.
- WASI builds do not link the SDK's mmap or signal emulation libraries.
- Non-standard libc case-insensitive string helpers are implemented by the
  Nano C11 portability unit.
- Arrays, strings, objects, cyclic collection, and interned strings use their
  selected PHP 8.6 Zend implementation units.
- `ext/standard/html.c` omits the SAPI/default-charset lookup in Nano. The
  original UTF-8 decoder remains compiled for JSON, while Nano has no SAPI or
  SAPI-provided default charset state.
- PHP output buffering is retained, but its final writer is the native/WASI
  console writer; SAPI headers and URL output rewriting are absent.
- Standard functions and arginfo are generated from PHP 8.6's guarded
  `basic_functions.stub.php`. `ini_set()` omits the `open_basedir` path checks because
  Nano exposes neither `open_basedir` nor filesystem access.
- Runtime constant AST values and complex arginfo defaults that require the
  PHP parser are rejected. Generated Nano arginfo uses literals handled
  without AST parsing.
