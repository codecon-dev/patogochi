<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/db.php';

require_method('GET');

// Query string: ?limit=50&cursor=<ideia_id>&username=<opcional-pra-marcar-votos>
$limit = max(1, min(100, (int)($_GET['limit'] ?? 50)));
$cursor = isset($_GET['cursor']) && is_numeric($_GET['cursor']) ? (int)$_GET['cursor'] : null;
$username = trim((string)($_GET['username'] ?? ''));

// Lembretes (biscoito_sorte) ficam no /lembretes.php — nao aparecem no feed publico
$where = "where i.tipo <> 'biscoito_sorte'";
if ($cursor) $where .= ' and i.id < :cursor';
$sql = "
    select
      i.id,
      i.texto,
      i.tipo,
      i.origem,
      i.criada_em,
      coalesce(sum(v.valor), 0)::int as score,
      count(v.id)::int as total_votos
    from ideias i
    left join votos v on v.ideia_id = i.id
    $where
    group by i.id
    order by i.id desc
    limit $limit
";
$stmt = db()->prepare($sql);
if ($cursor) $stmt->bindValue(':cursor', $cursor, PDO::PARAM_INT);
$stmt->execute();
$rows = $stmt->fetchAll();

// Voto do usuário atual pra cada ideia (opcional)
$meus_votos = [];
if ($username !== '' && count($rows) > 0) {
    $ids = array_column($rows, 'id');
    $placeholders = implode(',', array_fill(0, count($ids), '?'));
    $q = db()->prepare("
        select vo.ideia_id, vo.valor
          from votos vo
          join voters vr on vr.id = vo.voter_id
         where lower(vr.username) = lower(?)
           and vo.ideia_id in ($placeholders)
    ");
    $q->execute(array_merge([$username], $ids));
    foreach ($q->fetchAll() as $v) {
        $meus_votos[(int)$v['ideia_id']] = (int)$v['valor'];
    }
}

$ideias = array_map(fn($r) => [
    'id'          => (int) $r['id'],
    'texto'       => $r['texto'],
    'tipo'        => $r['tipo'],
    'origem'      => $r['origem'],
    'criada_em'   => $r['criada_em'],
    'score'       => (int) $r['score'],
    'total_votos' => (int) $r['total_votos'],
    'meu_voto'    => $meus_votos[(int) $r['id']] ?? 0,  // -1 | 0 | 1
], $rows);

$proximo_cursor = count($rows) === $limit ? end($rows)['id'] : null;

json_out([
    'ideias'          => $ideias,
    'proximo_cursor'  => $proximo_cursor !== null ? (int) $proximo_cursor : null,
]);
