# Intentional divergences from PHP

- `zend_long` is always signed 64-bit, including wasm32, to preserve TypePHP's
  integer semantics across targets.
- `zend_call_function` is implemented by the PHPX TypePHP Nano helper. It
  dispatches registered internal/AOT handlers only; it does not execute
  opcodes or evaluate/load PHP source.
- Runtime AST conversion to partial/first-class callables is rejected because
  upstream implements it by compiling a new op-array (and optionally caching
  it in Opcache).
- Zend MM retains its upstream allocation algorithms. POSIX Nano targets use a
  `posix_memalign()`/`free()` boundary; Windows retains php-src's VirtualAlloc
  implementation. Huge-page advice and in-place mapping growth/truncation are
  unavailable on the POSIX Nano path.
- Zend bailout uses C11 `setjmp()`/`longjmp()` instead of POSIX
  `sigsetjmp()`/`siglongjmp()`.
- Execution timers and signal handling are not compiled for Nano targets.
- PHP wall-clock, monotonic-clock, and sleep functions use the C++17
  `<chrono>` and `<thread>` APIs. PHP's date parsing, calendar arithmetic,
  timezone database, and DateTime classes continue to use the vendored PHP
  8.6 timelib sources.
- Zend allocation uses the reviewed target allocator; OS page-advice paths are
  not compiled for POSIX Nano targets.
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
  `basic_functions.stub.php`. Nano retains local filesystem access, but does not
  expose or configure PHP's `open_basedir` policy.
- Runtime constant AST values and complex arginfo defaults that require the
  PHP parser are rejected. Generated Nano arginfo uses literals handled
  without AST parsing.
