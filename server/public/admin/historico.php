<?php
declare(strict_types=1);

require_once __DIR__ . '/../../src/admin.php';
require_once __DIR__ . '/../../src/db.php';

require_admin();
require_method('GET');

$horas = max(1, min(168, (int)($_GET['horas'] ?? 24)));

$stmt = db()->prepare("
    select fome, estado, criado_em
      from pato_hp_historico
     where criado_em > now() - make_interval(hours => :h)
     order by criado_em asc
");
$stmt->execute(['h' => $horas]);
$rows = $stmt->fetchAll();

$pontos = array_map(fn($r) => [
    'fome' => (int) $r['fome'],
    'estado' => $r['estado'],
    'criado_em' => $r['criado_em'],
], $rows);

json_out([
    'pontos' => $pontos,
    'horas'  => $horas,
]);
