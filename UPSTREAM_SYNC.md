# Synchronizing php-src

PHP Nano treats the copied PHP tree as vendored upstream code. Nano behavior
should be expressed through source selection, generated configuration, stub
conditions, and small host adapters before an upstream file is patched.

The machine-readable policy is in `tools/upstream-sync.json`. Every modified
php-src file must be listed there with one narrow reason. Generated arginfo and
declaration headers are classified separately and must be regenerated from the
corresponding `.stub.php`; they are never hand-merged.

## Audit the current baseline

Download or check out the exact source recorded in `UPSTREAM.md`, then run:

```shell
php tools/upstream.php audit --source=/path/to/php-8.6.0beta3
```

The audit fails when it finds an unrecorded modification or an obsolete
allowlist entry. Removing an upstream patch therefore also requires removing
its exception from `tools/upstream-sync.json`.

Regenerate all arginfo and declaration outputs with the generator from the
same php-src release, rather than keeping a fork of the generator:

```shell
php tools/upstream.php regenerate --source=/path/to/php-8.6.0beta3
```

This is the only mutating subcommand. Review its Git diff before continuing.

## Plan an update

Keep both the recorded base release and the proposed new release available:

```shell
php tools/upstream.php plan \
    --base=/path/to/php-8.6.0beta3 \
    --target=/path/to/php-8.6.0
```

The read-only plan separates files into:

- byte-identical files that need no work;
- unmodified vendor files that can be copied directly from the new release;
- Nano patches that can be carried unchanged;
- files changed by both projects that require a three-way merge;
- generated headers that must be regenerated;
- files removed by upstream, which require an explicit source-manifest review.

Do not copy a complete php-src directory over this repository. PHP Nano
intentionally omits the VM implementation and generator, parser/scanner,
runtime AST/compiler implementation, SAPI, dynamic loader, unsupported
extension units, and PHPT tests. The exact files that must not return are listed
as `forbidden_imports` in `tools/upstream-sync.json`, and the audit fails if one
is added. Structural ABI declarations such as `zend_ast.h`, `zend_compile.h`,
`zend_vm.h`, and the opcode number definitions remain only where selected Zend
data structures require them; they do not provide a VM or parser. New upstream
files are imported only when a selected translation unit or public ABI header
requires them.

## Patch placement rules

Use this order when resolving a difference:

1. Exclude or include an unchanged translation unit in `composer.json`.
2. Select public functions in the upstream `.stub.php` and regenerate arginfo.
3. Configure portable upstream branches through generated configuration.
4. Add a generally named host hook under `include/` with its implementation in
   `src/`.
5. Patch php-src only when none of the above can express the difference.

An unavoidable upstream patch should guard the smallest possible block, retain
the upstream branch verbatim, and use a capability name rather than a consumer
name. Product-specific behavior belongs in the consuming project.

After updating, regenerate stubs and run the native, macOS, iOS, Android, WASI,
normal Nano application, and TypePHP-OS smoke builds before changing the
version recorded in `UPSTREAM.md`.
