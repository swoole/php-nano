# PHP Nano core constraints

This file is normative. An implementation or extension that violates a
`MUST NOT` rule below is not compatible with PHP Nano, even when the same code
is valid in a normal PHP build.

## Language and dependency boundary

- PHP `.c` sources MUST compile as C11. GNU C and other compiler extensions
  MUST NOT be part of the runtime contract.
- PHPX and generated TypePHP sources MUST compile as C++17.
- A generated program MUST link only the C standard library, the C++ standard
  library, the POSIX interfaces supplied by the target, and compiler ABI/runtime
  support supplied by its toolchain.
- It MUST NOT link an additional system library or a separately built
  third-party library.
- Native runtime source MAY use C11, C++17, and POSIX.1-2008 interfaces.
  [PORTABILITY_ALLOWLIST.md](PORTABILITY_ALLOWLIST.md) defines the smaller WASI
  subset and the closed set of non-standard exceptions. POSIX membership does
  not authorize process, network, signal, virtual-memory, or other forbidden
  host-control capabilities. Toolchain
  implementations of standard C++ time and entropy facilities may expose
  their narrow clock, wait, and random-source imports (for example
  `clock_gettime`, `nanosleep`, `clock_time_get`, `poll_oneoff`, or WASI
  `random-get`) in the final artifact.
- Code bundled in the PHP 8.6 source tree, such as PCRE2, timelib, and
  libbcmath, MAY be compiled from source directly into the final program. It
  MUST NOT be replaced with or linked against a system installation.
- PHP, Composer, and `nikic/php-parser` are build-time tools only. The generated
  program MUST NOT depend on them or on `libphp`.

## Forbidden runtime capabilities

PHP Nano MUST NOT expose or internally call APIs that provide:

- process creation or command execution, including `fork`, `exec`, `system`,
  `passthru`, `shell_exec`, `popen`, and `proc_open`;
- dynamic code or module loading, including `eval`, `include`, `require`, `dl`,
  `dlopen`, and runtime extension discovery;
- sockets, network clients or servers, DNS, and remote stream wrappers;
- non-file PHP stream transports and user-defined stream wrappers;
- environment mutation, system logging, or other platform-specific host
  control.
- signal handling, execution timers, virtual-memory mapping, or memory-page
  protection, including `signal`, `sigaction`, `mmap`, `munmap`, `mprotect`,
  and `madvise`.

Wall-clock access, monotonic-clock access, and sleeping are intentional
capabilities. Their PHP APIs MUST be implemented through C++17 `<chrono>` and
`<thread>` rather than direct platform calls.

Cryptographic random-byte access is an intentional capability. PHP's random
extension MUST obtain entropy through C++17 `std::random_device`, never through
direct operating-system calls in PHP Nano source. Failure of that standard
library interface MUST remain observable as a PHP random exception.

Local filesystem access is an intentional capability. PHP Nano retains PHP's
stream core with only the `file` and `glob` wrappers, plus file/directory/stat
and hash file/stream functions. Its portability layer MAY use the filesystem
surface supplied by the target C runtime, C++17 filesystem implementation, or
WASI libc. It MUST NOT register socket transports, remote wrappers, process
pipes, or any stream path that introduces network or command execution.
Platform-specific ownership operations that a target cannot represent, such
as user/group changes in WASI, MUST remain unavailable on that target.
The allowlist is capability-based: WASI may expose a smaller subset without
forcing Native targets to discard a portable operation.

Unsupported functions and classes MUST be absent from the registered PHP API.
Leaving a public entry that fails only when called is not sufficient. Its source
unit MUST also be excluded from the final link whenever it introduces a
forbidden host reference. A target-specific internal compatibility symbol MAY
remain solely to link otherwise reusable php-src core code; on WASI it MUST
return `ENOTSUP`/`FAILURE` deterministically and MUST NOT be registered publicly.

## Built-in PHP layer

PHP Nano vendors the PHP 8.6 source for Core, date, hash, json, pcre, random,
Reflection, SPL, standard, and filter. Transitive source-only
dependencies required by these modules are vendored as well.

Being built in does not bypass the dependency boundary. Every function, class,
startup callback, and source unit is subject to the capability rules above.
Built-in function tables and arginfo MUST be generated from the original
php-src `.stub.php` files with `build/gen_stub.php`. Unsupported APIs are
selected out with `#if PHP_NANO` in those stubs. Nano MUST preserve the
original php-src algorithm implementations. Nano-specific changes belong in
source selection, target configuration, stub guards, or small host adapters;
upstream extension source files SHOULD remain byte-identical wherever
practical.

Optional extensions are statically selected by Composer and MUST use the
`swoole/php-ext-*` package naming convention. They start after the fixed
built-in layer and are subject to this same file.

## Execution model

- PHP Nano retains Zend values, arrays, strings, objects, ordinary classes,
  inheritance, interfaces, exceptions, and native call dispatch.
- It retains the Zend INI registry, module INI entries, and runtime value
  access/modification. Configuration does not imply filesystem discovery;
  applications provide defaults and configuration values statically.
- It does not contain the Zend VM or runtime PHP compiler.
- Generated VM handlers, parser/scanner implementations, and runtime AST
  evaluation are not linked. Zend execution-stack primitives and structural
  headers remain because internal/AOT calls use the same Zend ABI.
- Direct AOT-to-AOT calls do not manufacture ZendVM execute frames. When a
  retained Zend helper creates an exception outside an internal-call frame,
  Nano MUST leave it in `EG(exception)` so the PHPX boundary can translate it
  into the C++ `zend_object*` unwind used by generated `try`/`catch` code.
- PHP's output buffer stack and `PHPWRITE` remain available. SAPI headers,
  HTTP headers, and URL output rewriting are not part of Nano output.
- It does not contain `Zend/Optimizer`, which only optimizes ZendVM op_arrays.
- TypePHP MUST reject `eval`, `include`, `require`, and anonymous classes for a
  Nano target at compile time.
- Fiber and Generator syntax MUST be rejected at compile time. Their stackful
  suspension requires platform context-switching APIs; C++17 provides no
  standard equivalent, and the needed host implementation is outside Nano's
  capability boundary. Ordinary Closure objects and
  synchronous dynamic calls remain supported.
- The final link MUST use the C++ linker.

## Verification

Native and WASM builds MUST verify their declared link inputs, every selected
object file, and the final unresolved or imported symbol set. A build fails if
it discovers an external library or forbidden host capability, regardless of
whether section garbage collection would discard that code or a test exercised
the path.

The VM-entry restrictions are part of `--nano` on every platform. Windows is
not a php-nano runtime target: it keeps TypePHP's existing host compile/link
pipeline and connects to the complete PHP/PHPX DLL runtime through import
libraries. The Windows build MUST NOT add the `swoole/php-nano` or `swoole/phpx`
Composer source manifests to project `sources`. The compiler still applies the
same `eval`/`include`/`require` restrictions and rejects external-command
features.
