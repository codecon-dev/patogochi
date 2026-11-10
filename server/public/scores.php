<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/db.php';

require_method('GET');

// Query string: ?ids=1,2,3&username=X
$ids_raw = (string)($_GET['ids'] ?? '');
$username = trim((string)($_GET['username'] ?? ''));

if ($ids_raw === '') json_out(['scores' => []]);

// filtra inteiros positivos
$ids = array_values(array_filter(array_map('intval', explode(',', $ids_raw)), fn($x) => $x > 0));
if (empty($ids)) json_out(['scores' => []]);
if (count($ids) > 200) $ids = array_slice($ids, 0, 200);

$placeholders = implode(',', array_fill(0, count($ids), '?'));
$stmt = db()->prepare("
    select v.ideia_id,
           coalesce(sum(v.valor), 0)::int as score,
           count(v.id)::int               as total
      from votos v
     where v.ideia_id in ($placeholders)
     group by v.ideia_id
");
$stmt->execute($ids);
$scores = [];
foreach ($stmt->fetchAll() as $r) {
    $scores[(int)$r['ideia_id']] = [
        'score' => (int) $r['score'],
        'total' => (int) $r['total'],
    ];
}

// Preenche zero pras ideias sem voto ainda
$result = [];
foreach ($ids as $id) {
    $result[$id] = $scores[$id] ?? ['score' => 0, 'total' => 0];
    $result[$id]['meu_voto'] = 0;
}

// Voto do usuário pra cada ideia (se passou username)
if ($username !== '') {
    $stmt = db()->prepare("
        select vo.ideia_id, vo.valor
          from votos vo
          join voters vr on vr.id = vo.voter_id
         where lower(vr.username) = lower(?)
           and vo.ideia_id in ($placeholders)
    ");
    $stmt->execute(array_merge([$username], $ids));
    foreach ($stmt->fetchAll() as $v) {
        $result[(int)$v['ideia_id']]['meu_voto'] = (int) $v['valor'];
    }
}

// Formato de retorno: mapa id -> {score, total, meu_voto}
json_out(['scores' => $result]);
