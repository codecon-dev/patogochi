<?php
declare(strict_types=1);

function json_out(array $data, int $code = 200): never {
    http_response_code($code);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($data, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

function json_err(string $msg, int $code = 400): never {
    json_out(['error' => $msg], $code);
}

function read_json_body(): array {
    $raw = file_get_contents('php://input') ?: '';
    if ($raw === '') return [];
    $j = json_decode($raw, true);
    if (!is_array($j)) json_err('json inválido', 400);
    return $j;
}

function require_method(string $method): void {
    if (($_SERVER['REQUEST_METHOD'] ?? 'GET') !== $method) {
        json_err('método não permitido', 405);
    }
}

/** Exige X-Api-Token igual ao API_TOKEN do .env. Comparacao em tempo constante. */
function require_api_token(): void {
    require_once __DIR__ . '/env.php';
    $expected = env('API_TOKEN', '');
    if ($expected === '') {
        json_err('servidor sem API_TOKEN configurado', 500);
    }
    $sent = $_SERVER['HTTP_X_API_TOKEN'] ?? '';
    if ($sent === '' || !hash_equals($expected, $sent)) {
        json_err('token ausente ou invalido', 401);
    }
}
