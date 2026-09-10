# PHP Nano portability and host-capability policy

This file is normative. It defines two profiles: Native and WASI. The WASI
profile is a strict subset of Native.

## Native standards baseline

Native PHP Nano targets Linux, macOS, iOS, and Android. Windows is deliberately
outside the php-nano runtime target set; TypePHP uses the complete PHP/PHPX DLL
runtime there and applies Nano language/capability restrictions in the compiler.

Native runtime source may use:

- ISO C11;
- ISO C++17; and
- POSIX.1-2008 interfaces supplied by the target platform.

POSIX is an implementation baseline, not an automatic grant of every POSIX
capability. The forbidden categories below remain forbidden even when POSIX
defines the underlying function.

The only currently reviewed Native host function outside ISO C/C++ and POSIX
is `flock`. New exceptions require an explicit entry in this file and a symbol
audit test. Toolchain runtime imports used to implement standard-library
facilities are implementation details, not new PHP APIs.

## Native retained capabilities

Native retains local filesystem and console I/O. It may use ordinary POSIX path,
descriptor, directory, metadata, ownership, clock, and account-lookup functions
when they implement an exposed Nano API. This includes `open`, `read`, `write`,
`dup`, `dup2`, `stat`, `opendir`, `mkstemp`, `chown`, `getuid`, and related
POSIX families.

PHP file access is limited to local `file` and `glob` streams. A file descriptor
does not authorize a socket, process pipe, device-control channel, or arbitrary
system call.

## WASI subset

The `wasm32-wasip2` profile keeps only facilities representable by the selected
WASI libc/component environment. It may retain console I/O, clocks, entropy,
arguments, and capability-based access to preopened files and directories.

WASI does not have to match Native. Operations such as ownership changes,
Unix account lookup, `umask`, and `flock` are omitted when WASI cannot provide
their PHP semantics.

An unsupported operation follows one of two rules:

1. A user-visible PHP function, method, class, or syntax form is absent from the
   generated target API. A direct TypePHP call is rejected at compile time with
   a target-specific error.
2. A small internal compatibility symbol may exist only when unmodified php-src
   core code needs it to link. Such a symbol must fail deterministically with
   `ENOTSUP`/`FAILURE`; it must not be registered as a user API.

This follows the useful separation in the php-wasm runtime: public capability
selection happens at build/API generation time, while internal WASI shims exist
only to preserve core linkage. PHP Nano does not inherit php-wasm's optional
third-party libraries or HTTP/network integrations.

## Explicitly forbidden capabilities

Both Native and WASI exclude:

- sockets, DNS, network clients/servers, transports, and remote stream wrappers;
- process creation and command execution, including pipes to child processes;
- dynamic code and module loading;
- signals and execution timers;
- virtual-memory mapping and page protection;
- system logging; and
- generic device-control or raw syscall interfaces.

Socket support remains excluded even though POSIX defines it and the Native
platforms provide it. No socket extension, socket transport, DNS function, or
remote wrapper may be registered.

## Enforcement

- Composer source manifests admit only reviewed runtime sources.
- php-src `.stub.php` guards and generated arginfo define each target's public API.
- TypePHP rejects statically known unsupported calls before C/C++ generation.
- Link flags reject third-party libraries.
- Object and final-artifact audits reject forbidden capability families.

The symbol auditor is intentionally a forbidden-capability audit rather than an
exhaustive list of every ISO/POSIX symbol. Adding a new public capability still
requires updating the target API, its tests, and this policy when applicable.
