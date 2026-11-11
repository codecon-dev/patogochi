<?php
declare(strict_types=1);

require_once __DIR__ . '/../../src/admin.php';
require_once __DIR__ . '/../../src/db.php';

require_admin();
require_method('GET');

$pdo = db();

$por_pagina = max(5, min(50, (int)($_GET['por_pagina'] ?? 20)));
$recurso    = (string)($_GET['recurso'] ?? 'tudo');  // tudo|ideias|lembretes|mortes
$pagina     = max(1, (int)($_GET['pagina'] ?? 1));
$offset     = ($pagina - 1) * $por_pagina;

function paginar(PDO $pdo, string $tabela_sql, string $count_sql, int $por_pagina, int $offset): array {
    $itens = $pdo->prepare($tabela_sql . " limit :l offset :o");
    $itens->bindValue(':l', $por_pagina, PDO::PARAM_INT);
    $itens->bindValue(':o', $offset, PDO::PARAM_INT);
    $itens->execute();
    $total = (int) $pdo->query($count_sql)->fetchColumn();
    return ['itens' => $itens->fetchAll(), 'total' => $total];
}

$resp = ['pato' => $pdo->query('select * from pato where id=1')->fetch()];

if ($recurso === 'tudo' || $recurso === 'ideias') {
    $r = paginar(
        $pdo,
        "select i.id, i.texto, i.tipo, i.origem, i.criada_em,
                coalesce(sum(v.valor), 0)::int as score
           from ideias i
           left join votos v on v.ideia_id = i.id
          group by i.id
          order by i.id desc",
        "select count(*) from ideias",
        $por_pagina, $offset
    );
    $resp['ideias']       = $r['itens'];
    $resp['ideias_total'] = $r['total'];
}

if ($recurso === 'tudo' || $recurso === 'lembretes') {
    $r = paginar(
        $pdo,
        "select id, texto, prazo, virou_coco, feito_em, criado_em
           from lembretes
          order by coalesce(feito_em, '9999-12-31'::timestamptz) desc,
                   prazo asc",
        "select count(*) from lembretes",
        $por_pagina, $offset
    );
    $resp['lembretes']       = $r['itens'];
    $resp['lembretes_total'] = $r['total'];
}

if ($recurso === 'tudo' || $recurso === 'mortes') {
    $r = paginar(
        $pdo,
        "select id, morreu_em, reviveu_em, left(obituario, 200) as obituario
           from mortes order by id desc",
        "select count(*) from mortes",
        $por_pagina, $offset
    );
    $resp['mortes']       = $r['itens'];
    $resp['mortes_total'] = $r['total'];
}

$resp['paginacao'] = ['pagina' => $pagina, 'por_pagina' => $por_pagina];

json_out($resp);
