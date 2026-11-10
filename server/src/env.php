<?php
declare(strict_types=1);

function env(string $key, ?string $default = null): ?string {
    static $loaded = false;
    if (!$loaded) {
        $path = __DIR__ . '/../config/.env';
        if (is_readable($path)) {
            foreach (file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
                if ($line === '' || $line[0] === '#') continue;
                [$k, $v] = array_pad(explode('=', $line, 2), 2, '');
                $_ENV[trim($k)] = trim($v);
            }
        }
        $loaded = true;
    }
    return $_ENV[$key] ?? getenv($key) ?: $default;
}
