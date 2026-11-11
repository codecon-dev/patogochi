<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/db.php';

require_method('GET');
require_api_token();

// Pendentes: vencidos (prazo <= now) primeiro, depois os futuros em ordem crescente
$rows = db()->query("
    select id, texto, prazo as vence_em, virou_coco as vencido
    from lembretes
    where feito_em is null
    order by
      (prazo <= now()) desc,
      prazo asc
")->fetchAll();

$lembretes = array_map(fn($r) => [
    'id'       => (int) $r['id'],
    'texto'    => $r['texto'],
    'vence_em' => $r['vence_em'],
    'vencido'  => (bool) $r['vencido'],
], $rows);

json_out(['lembretes' => $lembretes]);
