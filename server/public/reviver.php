<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/regras.php';
require_once __DIR__ . '/../src/claude.php';
require_once __DIR__ . '/../src/whisper.php';
require_once __DIR__ . '/../src/mqtt.php';

require_method('POST');
require_api_token();

// --- parse: aceita multipart (audio|texto|metodo) OU JSON puro ---
$content_type = $_SERVER['CONTENT_TYPE'] ?? '';
if (str_starts_with($content_type, 'multipart/form-data')) {
    $metodo  = (string)($_POST['metodo'] ?? 'desculpa');
    $texto   = trim((string)($_POST['texto'] ?? ''));
    $batidas = (int)   ($_POST['batidas'] ?? 0);
    $bpm     = (float) ($_POST['bpm']     ?? 0);
    $audio_recebido = false;
    if (!empty($_FILES['audio']) && $_FILES['audio']['error'] === UPLOAD_ERR_OK) {
        $audio_recebido = true;
        try {
            $texto = whisper_transcreve($_FILES['audio']['tmp_name']);
        } catch (Throwable $e) {
            json_err('transcricao falhou: ' . $e->getMessage(), 500);
        }
    }
} else {
    $body    = read_json_body();
    $metodo  = (string)($body['metodo']  ?? '');
    $texto   = trim((string)($body['texto'] ?? ''));
    $batidas = (int)   ($body['batidas'] ?? 0);
    $bpm     = (float) ($body['bpm']     ?? 0);
    $audio_recebido = false;
}

$p = db()->query('select * from pato where id=1')->fetch();
if ($p['estado'] !== 'morto') {
    json_err('o pato tá vivo', 409);
}

if ($metodo === 'desculpa') {
    if ($texto === '') json_err('texto vazio (manda audio ou texto)', 400);

    $bin   = env('CLAUDE_BIN', 'claude');
    $model = env('CLAUDE_MODEL', 'claude-haiku-4-5');
    $prompt = "Classifique o texto abaixo em UMA palavra (SIM ou NAO) de acordo com a sinceridade de um pedido de desculpas. "
            . "Criterios: SIM se o texto expressa arrependimento genuino com pelo menos 2 destes elementos: (a) admissao de culpa, (b) reconhecimento do dano causado, (c) promessa de mudanca. NAO caso contrario. "
            . "Responda EXATAMENTE uma palavra: SIM ou NAO.\n\nTexto: $texto";
    // Timeout de 20s pra não segurar o /reviver se Claude travar.
    // Se Claude falhar ou devolver bosta -> aceita (modo generoso).
    $cmd = sprintf(
        'timeout 20 %s -p %s --model %s --output-format text 2>&1',
        escapeshellcmd($bin), escapeshellarg($prompt), escapeshellarg($model)
    );
    $r_raw  = shell_exec($cmd);
    $r      = strtoupper(trim((string) $r_raw));
    $primeira_linha = strtoupper(strtok($r, "\n") ?: '');

    // FALLBACK GENEROSO: se Claude explicitamente disse NAO -> rejeita.
    // Qualquer outra coisa (timeout, erro, resposta estranha, SIM) -> aceita.
    $recusou_explicitamente =
           $primeira_linha === 'NAO'
        || $primeira_linha === 'NAO.'
        || str_starts_with($primeira_linha, 'NAO ');

    if ($recusou_explicitamente) {
        json_out([
            'ok' => false,
            'transcrito' => $audio_recebido,
            'texto' => $texto,
            'opiniao' => 'o pato não acreditou. tenta de novo.',
        ]);
    }
} elseif ($metodo === 'massagem') {
    if ($batidas < 30) json_err('precisa 30 batidas', 400);
    if ($bpm < 100 || $bpm > 120) json_err('ritmo errado (100-120 bpm)', 400);
} else {
    json_err('metodo deve ser "desculpa" ou "massagem"', 400);
}

// Revive com fome 50 + escudo de invulnerabilidade de 1h
db()->exec("
    update pato
       set estado='parado',
           fome=50,
           estado_ate=null,
           morto_desde=null,
           invulneravel_ate=now() + interval '1 hour',
           versao=versao+1,
           atualizado_em=now()
     where id=1
");
db()->exec('update mortes set reviveu_em=now() where reviveu_em is null');

$p = db()->query('select * from pato where id=1')->fetch();
mqtt_pub_estado($p);
mqtt_pub_alerta('reviveu', 'info');

$resp = estado_json($p) + ['ok' => true];
if ($audio_recebido) {
    $resp['transcrito'] = true;
    $resp['texto'] = $texto;
}
json_out($resp);
