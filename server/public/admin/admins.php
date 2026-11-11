<?php
declare(strict_types=1);

require_once __DIR__ . '/../../src/admin.php';
require_once __DIR__ . '/../../src/db.php';

require_admin();

$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
$pdo    = db();

if ($method === 'GET') {
    $rows = $pdo->query('select id, username, criado_em from admins order by criado_em asc')->fetchAll();
    json_out(['admins' => $rows, 'logado' => admin_atual()]);
}

if ($method === 'POST') {
    $body = read_json_body();
    $acao = (string)($body['acao'] ?? '');

    if ($acao === 'criar') {
        $u = trim((string)($body['username'] ?? ''));
        $p = (string)($body['password'] ?? '');
        if (strlen($u) < 2 || strlen($u) > 30) json_err('username 2-30 chars', 400);
        if (!preg_match('/^[a-zA-Z0-9_\-]+$/', $u)) json_err('username so alfa-numerico + _ -', 400);
        if (strlen($p) < 8) json_err('senha minimo 8 chars', 400);

        $hash = password_hash($p, PASSWORD_BCRYPT);
        try {
            $pdo->prepare('insert into admins (username, pass_hash) values (?, ?)')
                ->execute([$u, $hash]);
            json_out(['ok' => true, 'username' => $u]);
        } catch (PDOException $e) {
            if (str_contains($e->getMessage(), 'duplicate')) json_err('username ja existe', 409);
            throw $e;
        }
    }

    if ($acao === 'remover') {
        $u = trim((string)($body['username'] ?? ''));
        if ($u === '') json_err('username vazio', 400);
        if (strtolower($u) === strtolower(admin_atual())) {
            json_err('nao pode remover a si mesmo', 400);
        }
        $pdo->prepare('delete from admins where lower(username) = lower(?)')->execute([$u]);
        json_out(['ok' => true, 'removido' => $u]);
    }

    if ($acao === 'trocar_senha') {
        $u = trim((string)($body['username'] ?? admin_atual()));
        $p = (string)($body['password'] ?? '');
        if (strlen($p) < 8) json_err('senha minimo 8 chars', 400);
        $hash = password_hash($p, PASSWORD_BCRYPT);
        $pdo->prepare('update admins set pass_hash=? where lower(username) = lower(?)')
            ->execute([$hash, $u]);
        json_out(['ok' => true, 'username' => $u]);
    }

    json_err('acao invalida (criar|remover|trocar_senha)', 400);
}

json_err('método não permitido', 405);
