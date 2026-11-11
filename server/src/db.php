<?php
declare(strict_types=1);

require_once __DIR__ . '/env.php';

function db(): PDO {
    static $pdo = null;
    if ($pdo !== null) return $pdo;

    $dsn = sprintf(
        'pgsql:host=%s;port=%s;dbname=%s;sslmode=require',
        env('DB_HOST'),
        env('DB_PORT', '5432'),
        env('DB_NAME', 'postgres')
    );
    $pdo = new PDO($dsn, env('DB_USER'), env('DB_PASSWORD'), [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
    ]);
    return $pdo;
}
