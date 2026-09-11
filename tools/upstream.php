#!/usr/bin/env php
<?php

declare(strict_types=1);

const EXIT_USAGE = 2;

function fail(string $message, int $code = 1): never
{
    fwrite(STDERR, $message . PHP_EOL);
    exit($code);
}

function usage(): never
{
    fail(
        "Usage:\n"
        . "  php tools/upstream.php metadata\n"
        . "  php tools/upstream.php audit --source=/path/to/php-src\n"
        . "  php tools/upstream.php plan --base=/path/to/old/php-src --target=/path/to/new/php-src\n\n"
        . "  php tools/upstream.php regenerate --source=/path/to/php-src\n\n"
        . "'audit' and 'plan' are read-only. 'audit' verifies the recorded divergence allowlist.\n"
        . "'plan' classifies files for a future three-way upstream update.\n"
        . "'regenerate' is the only mutating command and runs that release's gen_stub.php.",
        EXIT_USAGE,
    );
}

/** @return array<string, string|bool> */
function parseOptions(array $arguments): array
{
    $options = [];
    foreach ($arguments as $argument) {
        if (!str_starts_with($argument, '--')) {
            fail("Unexpected argument: {$argument}", EXIT_USAGE);
        }
        $parts = explode('=', substr($argument, 2), 2);
        $options[$parts[0]] = $parts[1] ?? true;
    }
    return $options;
}

function normalizedDirectory(string $path, string $label): string
{
    $resolved = realpath($path);
    if ($resolved === false || !is_dir($resolved)) {
        fail("{$label} directory does not exist: {$path}", EXIT_USAGE);
    }
    return rtrim($resolved, DIRECTORY_SEPARATOR);
}

/** @return array<string, mixed> */
function loadConfiguration(string $repository): array
{
    $path = $repository . '/tools/upstream-sync.json';
    $configuration = json_decode((string) file_get_contents($path), true);
    if (!is_array($configuration)) {
        fail("Invalid upstream configuration: {$path}");
    }
    return $configuration;
}

/** @return list<string> */
function upstreamOwnedFiles(string $repository, array $roots, string $base): array
{
    $files = [];
    foreach ($roots as $root) {
        $directory = $repository . '/' . $root;
        if (!is_dir($directory)) {
            continue;
        }
        $iterator = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($directory, FilesystemIterator::SKIP_DOTS),
        );
        foreach ($iterator as $file) {
            if (!$file->isFile()) {
                continue;
            }
            $relative = str_replace('\\', '/', substr($file->getPathname(), strlen($repository) + 1));
            if (is_file($base . '/' . $relative)) {
                $files[] = $relative;
            }
        }
    }
    sort($files, SORT_STRING);
    return $files;
}

function contentEqual(string $left, string $right): bool
{
    return filesize($left) === filesize($right)
        && hash_file('sha256', $left) === hash_file('sha256', $right);
}

function matchesAny(string $path, array $patterns): bool
{
    foreach ($patterns as $pattern) {
        if (fnmatch($pattern, basename($path))) {
            return true;
        }
    }
    return false;
}

function printGroup(string $title, array $paths): void
{
    if ($paths === []) {
        return;
    }
    echo PHP_EOL, $title, ' (', count($paths), "):\n";
    foreach ($paths as $path) {
        echo '  ', $path, PHP_EOL;
    }
}

function audit(string $repository, array $configuration, array $options): never
{
    $sourceOption = $options['source'] ?? null;
    if (!is_string($sourceOption)) {
        fail('audit requires --source=/path/to/php-src', EXIT_USAGE);
    }
    $source = normalizedDirectory($sourceOption, 'Upstream source');
    $allowed = $configuration['intentional_divergences'] ?? [];
    $generatedPatterns = $configuration['generated_patterns'] ?? [];
    $files = upstreamOwnedFiles($repository, $configuration['roots'] ?? [], $source);

    $same = [];
    $intentional = [];
    $generated = [];
    $unexpected = [];
    foreach ($files as $relative) {
        $localPath = $repository . '/' . $relative;
        $sourcePath = $source . '/' . $relative;
        if (contentEqual($localPath, $sourcePath)) {
            $same[] = $relative;
        } elseif (matchesAny($relative, $generatedPatterns)) {
            $generated[] = $relative;
        } elseif (isset($allowed[$relative])) {
            $intentional[] = $relative;
        } else {
            $unexpected[] = $relative;
        }
    }

    $stale = [];
    foreach ($allowed as $relative => $_category) {
        $localPath = $repository . '/' . $relative;
        $sourcePath = $source . '/' . $relative;
        if (!is_file($localPath) || !is_file($sourcePath) || contentEqual($localPath, $sourcePath)) {
            $stale[] = $relative;
        }
    }

    $forbidden = [];
    foreach ($configuration['forbidden_imports'] ?? [] as $relative) {
        if (is_file($repository . '/' . $relative)) {
            $forbidden[] = $relative;
        }
    }

    echo 'PHP Nano upstream audit', PHP_EOL;
    echo '  source:                ', $source, PHP_EOL;
    echo '  upstream-owned files:  ', count($files), PHP_EOL;
    echo '  byte-identical:         ', count($same), PHP_EOL;
    echo '  intentional patches:   ', count($intentional), PHP_EOL;
    echo '  generated outputs:     ', count($generated), PHP_EOL;
    echo '  unexpected divergence: ', count($unexpected), PHP_EOL;
    echo '  stale allowlist entry: ', count($stale), PHP_EOL;
    echo '  forbidden import:      ', count($forbidden), PHP_EOL;

    printGroup('Unexpected divergences', $unexpected);
    printGroup('Stale allowlist entries', $stale);
    printGroup('Forbidden VM/parser/compiler imports', $forbidden);
    if ($unexpected !== [] || $stale !== [] || $forbidden !== []) {
        exit(1);
    }
    exit(0);
}

function plan(string $repository, array $configuration, array $options): never
{
    $baseOption = $options['base'] ?? null;
    $targetOption = $options['target'] ?? null;
    if (!is_string($baseOption) || !is_string($targetOption)) {
        fail('plan requires both --base=/old/php-src and --target=/new/php-src', EXIT_USAGE);
    }
    $base = normalizedDirectory($baseOption, 'Base source');
    $target = normalizedDirectory($targetOption, 'Target source');
    $allowed = $configuration['intentional_divergences'] ?? [];
    $generatedPatterns = $configuration['generated_patterns'] ?? [];
    $files = upstreamOwnedFiles($repository, $configuration['roots'] ?? [], $base);

    $unchanged = [];
    $fastForward = [];
    $carry = [];
    $merge = [];
    $generated = [];
    $removed = [];
    $unexpected = [];

    foreach ($files as $relative) {
        $localPath = $repository . '/' . $relative;
        $basePath = $base . '/' . $relative;
        $targetPath = $target . '/' . $relative;
        if (!is_file($targetPath)) {
            $removed[] = $relative;
            continue;
        }
        if (matchesAny($relative, $generatedPatterns)) {
            $generated[] = $relative;
            continue;
        }

        $localChanged = !contentEqual($localPath, $basePath);
        $upstreamChanged = !contentEqual($targetPath, $basePath);
        if (!$localChanged && !$upstreamChanged) {
            $unchanged[] = $relative;
        } elseif (!$localChanged) {
            $fastForward[] = $relative;
        } elseif (!isset($allowed[$relative])) {
            $unexpected[] = $relative;
        } elseif (!$upstreamChanged) {
            $carry[] = $relative;
        } else {
            $merge[] = $relative;
        }
    }

    echo 'PHP Nano upstream sync plan (read-only)', PHP_EOL;
    echo '  base:                  ', $base, PHP_EOL;
    echo '  target:                ', $target, PHP_EOL;
    echo '  unchanged:             ', count($unchanged), PHP_EOL;
    echo '  copy from new release: ', count($fastForward), PHP_EOL;
    echo '  carry Nano patch:      ', count($carry), PHP_EOL;
    echo '  needs three-way merge: ', count($merge), PHP_EOL;
    echo '  regenerate from stub:  ', count($generated), PHP_EOL;
    echo '  removed upstream:      ', count($removed), PHP_EOL;
    echo '  unexpected local edit: ', count($unexpected), PHP_EOL;

    printGroup('Files needing three-way merge', $merge);
    printGroup('Files removed by the target release', $removed);
    printGroup('Unexpected local edits', $unexpected);
    exit($removed === [] && $unexpected === [] ? 0 : 1);
}

function regenerate(string $repository, array $options): never
{
    $sourceOption = $options['source'] ?? null;
    if (!is_string($sourceOption)) {
        fail('regenerate requires --source=/path/to/php-src', EXIT_USAGE);
    }
    $source = normalizedDirectory($sourceOption, 'Upstream source');
    $generator = $source . '/build/gen_stub.php';
    if (!is_file($generator)) {
        fail("The matching php-src stub generator is missing: {$generator}");
    }

    $command = escapeshellarg(PHP_BINARY)
        . ' ' . escapeshellarg($generator)
        . ' --force-regeneration ' . escapeshellarg($repository);
    echo 'Generating arginfo with ', $generator, PHP_EOL;
    passthru($command, $status);
    if ($status !== 0) {
        fail("gen_stub.php failed with exit status {$status}", $status);
    }
    echo "Arginfo regeneration completed. Review the resulting Git diff.\n";
    exit(0);
}

function metadata(array $configuration): never
{
    $upstream = $configuration['upstream'] ?? [];
    foreach (['version', 'tag', 'commit', 'archive_url', 'archive_sha256'] as $name) {
        $value = $upstream[$name] ?? null;
        if (!is_string($value) || $value === '' || str_contains($value, "\n")) {
            fail("Invalid upstream metadata field: {$name}");
        }
        echo $name, '=', $value, PHP_EOL;
    }
    exit(0);
}

$repository = dirname(__DIR__);
$configuration = loadConfiguration($repository);
$arguments = $_SERVER['argv'];
array_shift($arguments);
$command = array_shift($arguments);
if (!is_string($command)) {
    usage();
}
$options = parseOptions($arguments);
match ($command) {
    'metadata' => metadata($configuration),
    'audit' => audit($repository, $configuration, $options),
    'plan' => plan($repository, $configuration, $options),
    'regenerate' => regenerate($repository, $options),
    default => usage(),
};
