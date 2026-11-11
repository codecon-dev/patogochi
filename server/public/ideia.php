<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/regras.php';
require_once __DIR__ . '/../src/claude.php';
require_once __DIR__ . '/../src/whisper.php';

require_method('POST');
require_api_token();

$content_type = $_SERVER['CONTENT_TYPE'] ?? '';
$texto = '';
$origem = 'core2';
$audio_recebido = false;

// --- multipart: audio e/ou texto ---
if (str_starts_with($content_type, 'multipart/form-data')) {
    $origem = (string)($_POST['origem'] ?? 'core2');
    $texto  = trim((string)($_POST['texto'] ?? ''));

    if (!empty($_FILES['audio']) && $_FILES['audio']['error'] === UPLOAD_ERR_OK) {
        $audio_recebido = true;
        try {
            $texto = whisper_transcreve($_FILES['audio']['tmp_name']);
        } catch (Throwable $e) {
            json_err('transcricao falhou: ' . $e->getMessage(), 500);
        }
    }
}
// --- json puro: so texto ---
else {
    $body   = read_json_body();
    $texto  = trim((string)($body['texto'] ?? ''));
    $origem = (string)($body['origem'] ?? 'core2');
}

if ($texto === '') json_err('texto vazio (envie "texto" ou "audio")', 400);
if (!in_array($origem, ['core2','alexa','web'], true)) json_err('origem invalida', 400);

// pato morto nao come
$pato = db()->query('select estado from pato where id=1')->fetch();
if ($pato && $pato['estado'] === 'morto') {
    json_out(['error' => 'pato morto'], 409);
}

$class = claude_classifica_ideia($texto);

// Preferencia: titulo limpo > texto corrigido > texto bruto
$texto_final = !empty($class['titulo'])
    ? $class['titulo']
    : (!empty($class['texto_corrigido']) ? $class['texto_corrigido'] : $texto);

// salva ideia (opiniao NULL, mantido NOT NULL no schema entao salvamos string vazia)
$stmt = db()->prepare('
    insert into ideias (origem, texto, tipo, opiniao, prazo)
    values (:o, :t, :tp, :op, :pz)
    returning id
');
$stmt->execute([
    'o'  => $origem,
    't'  => $texto_final,
    'tp' => $class['tipo'],
    'op' => '',
    'pz' => $class['prazo'],
]);
$ideia_id = (int) $stmt->fetchColumn();

// prazo vira lembrete (usa titulo limpo do Claude se tiver; senao texto corrigido)
if (!empty($class['prazo'])) {
    $texto_lembrete = !empty($class['titulo']) ? $class['titulo'] : $texto_final;
    db()->prepare('
        insert into lembretes (ideia_id, texto, prazo)
        values (:i, :t, :p)
    ')->execute([
        'i' => $ideia_id,
        't' => $texto_lembrete,
        'p' => $class['prazo'],
    ]);
}

// atualiza pato (texto vai pro alerta Alexa se for comida estragada)
$p = pato_alimenta($class['tipo'], $class['qualidade'], $texto);

json_out([
    'tipo'   => $class['tipo'],
    'prazo'  => $class['prazo'],
    'estado' => $p['estado'],
]);
