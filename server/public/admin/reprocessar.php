<?php
declare(strict_types=1);

require_once __DIR__ . '/../../src/admin.php';
require_once __DIR__ . '/../../src/claude.php';
require_once __DIR__ . '/../../src/db.php';

require_admin();
require_method('POST');

// Streaming manual pra o browser ver progresso. Cada ideia gera 1 linha.
// Idempotente: pula ideias ja corrigidas (`reprocessada_em` not null).
ignore_user_abort(true);
set_time_limit(0);
header('Content-Type: text/plain; charset=utf-8');
header('X-Accel-Buffering: no');  // nginx nao bufferiza
ob_implicit_flush(true);
while (ob_get_level()) ob_end_flush();

$body = read_json_body();
$ids_especificos = $body['ids'] ?? null;  // null = todas pendentes
$limit = min(200, max(1, (int)($body['limit'] ?? 50)));

$pdo = db();

if (is_array($ids_especificos) && count($ids_especificos) > 0) {
    $ids = array_values(array_filter(array_map('intval', $ids_especificos), fn($x) => $x > 0));
    $placeholders = implode(',', array_fill(0, count($ids), '?'));
    $stmt = $pdo->prepare("select id, texto from ideias where id in ($placeholders) order by id asc");
    $stmt->execute($ids);
} else {
    $stmt = $pdo->prepare("
        select id, texto from ideias
        where reprocessada_em is null
        order by id asc
        limit ?
    ");
    $stmt->bindValue(1, $limit, PDO::PARAM_INT);
    $stmt->execute();
}

$total = 0; $mudou = 0; $erros = 0;
foreach ($stmt->fetchAll() as $r) {
    $total++;
    $id = (int) $r['id'];
    $antes = (string) $r['texto'];
    try {
        $class = claude_classifica_ideia($antes);
        // Preferencia: titulo limpo > texto corrigido > manter como esta
        $novo = !empty($class['titulo'])
            ? $class['titulo']
            : (!empty($class['texto_corrigido']) ? $class['texto_corrigido'] : $antes);
        if ($novo !== $antes) {
            $pdo->prepare('update ideias set texto = :t, reprocessada_em = now() where id = :i')
                ->execute(['t' => $novo, 'i' => $id]);
            // Se tem lembrete, atualiza titulo também
            if (!empty($class['titulo'])) {
                $pdo->prepare('update lembretes set texto = :t where ideia_id = :i and feito_em is null')
                    ->execute(['t' => $class['titulo'], 'i' => $id]);
            }
            $mudou++;
            echo "#$id ✓ MUDOU\n  antes:  $antes\n  depois: $novo\n\n";
        } else {
            $pdo->prepare('update ideias set reprocessada_em = now() where id = :i')->execute(['i' => $id]);
            echo "#$id ─ ok (sem mudança)\n";
        }
    } catch (Throwable $e) {
        $erros++;
        echo "#$id ✗ ERRO: " . $e->getMessage() . "\n";
    }
    @ob_flush(); @flush();
}

echo "\n=== fim ===\n";
echo "processadas: $total\nmudadas: $mudou\nerros: $erros\n";
