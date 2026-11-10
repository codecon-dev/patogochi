<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/db.php';
require_once __DIR__ . '/../src/env.php';

require_method('POST');

$body     = read_json_body();
$ideia_id = (int)   ($body['ideia_id'] ?? 0);
$valor    = (int)   ($body['voto']     ?? 0);
$username = trim((string)($body['username'] ?? ''));

if ($ideia_id <= 0)                        json_err('ideia_id invalido', 400);
if (!in_array($valor, [-1, 0, 1], true))   json_err('voto deve ser -1, 0 ou 1', 400);
if (strlen($username) < 2 || strlen($username) > 30) json_err('username precisa ter 2-30 chars', 400);
if (!preg_match('/^[\w\- áéíóúâêôãõç]+$/iu', $username)) json_err('username com caracter invalido', 400);

$pdo = db();

// Garante que a ideia existe
$q = $pdo->prepare('select 1 from ideias where id = ?');
$q->execute([$ideia_id]);
if (!$q->fetchColumn()) json_err('ideia nao existe', 404);

// Hash do IP pra anti-spam (futuro — não bloqueia nada agora)
$ip = $_SERVER['HTTP_X_FORWARDED_FOR'] ?? $_SERVER['REMOTE_ADDR'] ?? '';
$ip = explode(',', $ip)[0];
$ip_hash = hash('sha256', $ip . '|' . env('API_TOKEN', 'patogochi'));

// Upsert do voter (username case-insensitive)
$pdo->prepare('
    insert into voters (username, ip_hash)
    values (:u, :h)
    on conflict (username) do nothing
')->execute(['u' => $username, 'h' => $ip_hash]);

$q = $pdo->prepare('select id from voters where lower(username) = lower(?)');
$q->execute([$username]);
$voter_id = (int) $q->fetchColumn();
if ($voter_id === 0) json_err('erro ao criar voter', 500);

// valor = 0 remove voto; -1/+1 upserta
if ($valor === 0) {
    $pdo->prepare('delete from votos where ideia_id = ? and voter_id = ?')
        ->execute([$ideia_id, $voter_id]);
} else {
    $pdo->prepare('
        insert into votos (ideia_id, voter_id, valor)
        values (:i, :v, :val)
        on conflict (ideia_id, voter_id)
        do update set valor = excluded.valor, atualizado_em = now()
    ')->execute(['i' => $ideia_id, 'v' => $voter_id, 'val' => $valor]);
}

// Score consolidado
$q = $pdo->prepare('
    select coalesce(sum(valor), 0)::int as score,
           count(*)::int                as total
      from votos where ideia_id = :i
');
$q->execute(['i' => $ideia_id]);
$r = $q->fetch();

json_out([
    'ok'          => true,
    'ideia_id'    => $ideia_id,
    'meu_voto'    => $valor,
    'score'       => (int) $r['score'],
    'total_votos' => (int) $r['total'],
]);
