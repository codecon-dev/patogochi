<?php
declare(strict_types=1);

require_once __DIR__ . '/env.php';
require_once __DIR__ . '/http.php';
require_once __DIR__ . '/db.php';

/**
 * HTTP Basic Auth multi-user.
 * - Primeiro tenta na tabela `admins` (bcrypt via password_verify)
 * - Fallback: ADMIN_USER/ADMIN_PASS do .env (pra bootstrap quando tabela vazia)
 */
function require_admin(): void {
    $sent_user = $_SERVER['PHP_AUTH_USER'] ?? '';
    $sent_pass = $_SERVER['PHP_AUTH_PW']   ?? '';

    if ($sent_user === '' || $sent_pass === '') {
        _admin_challenge();
    }

    if (_admin_valida_db($sent_user, $sent_pass)) return;
    if (_admin_valida_env($sent_user, $sent_pass)) return;
    _admin_challenge();
}

function _admin_valida_db(string $user, string $pass): bool {
    try {
        $stmt = db()->prepare('select pass_hash from admins where lower(username) = lower(?)');
        $stmt->execute([$user]);
        $hash = $stmt->fetchColumn();
        if (!$hash) return false;
        return password_verify($pass, (string) $hash);
    } catch (Throwable $e) {
        return false;  // tabela não existe ainda
    }
}

function _admin_valida_env(string $user, string $pass): bool {
    $u = env('ADMIN_USER', '');
    $p = env('ADMIN_PASS', '');
    if ($u === '' || $p === '') return false;
    return hash_equals($u, $user) && hash_equals($p, $pass);
}

function _admin_challenge(): never {
    header('WWW-Authenticate: Basic realm="Patogochi Admin"');
    http_response_code(401);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode(['error' => 'auth necessaria']);
    exit;
}

/** Nome do admin logado (depois de passar require_admin). */
function admin_atual(): string {
    return (string)($_SERVER['PHP_AUTH_USER'] ?? '');
}
