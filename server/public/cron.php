<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/regras.php';
require_once __DIR__ . '/../src/claude.php';
require_once __DIR__ . '/../src/mqtt.php';
require_once __DIR__ . '/../src/alexa.php';

// Só executa com token certo (chamado por cron do host)
$token = $_SERVER['HTTP_X_CRON_TOKEN'] ?? '';
if ($token === '' || $token !== env('CRON_TOKEN')) {
    json_err('não autorizado', 401);
}

$acoes = [];

// 1) Lembretes vencidos viram cocô
$stmt = db()->prepare('
    update lembretes
       set virou_coco = true
     where feito_em is null
       and virou_coco = false
       and prazo <= now()
     returning id
');
$stmt->execute();
$novos = $stmt->fetchAll(PDO::FETCH_COLUMN);
$acoes['lembretes_viraram_coco'] = count($novos);
if ($novos) {
    $total = (int) db()->query('select count(*) from lembretes where virou_coco=true and feito_em is null')->fetchColumn();
    foreach ($novos as $lembrete_id) {
        mqtt_pub_coco((int) $lembrete_id, $total);
    }
    // Alexa avisa que apareceu coco (so se tiver 3+)
    if ($total >= 3) {
        alexa_fala_sarcastica(
            "O pato acumulou $total cocos de lembretes vencidos, a tela ta uma zona",
            "Atencao: o pato ta cagando lembretes atrasados. Sao $total agora."
        );
    }
}

// 2) Aplica decaimento (também detecta morte dentro de pato_lido_com_decaimento)
$p = pato_lido_com_decaimento();
$acoes['pato'] = ['fome' => (int)$p['fome'], 'estado' => $p['estado']];
mqtt_pub_estado($p);

// 3) Se morreu agora e ainda não tem obituário, Claude escreve
if ($p['estado'] === 'morto') {
    $m = db()->query('select * from mortes order by id desc limit 1')->fetch();
    if (!$m || $m['reviveu_em'] !== null) {
        // Morte nova: registra e gera obituário
        $ult = db()->query('select id, texto from ideias order by id desc limit 1')->fetch();
        $obit = claude_obituario($ult['texto'] ?? 'nenhuma ideia recente');
        db()->prepare('
            insert into mortes (obituario, ultima_ideia_id)
            values (:o, :i)
        ')->execute(['o' => $obit, 'i' => $ult['id'] ?? null]);
        db()->exec('update pato set mortes = mortes + 1 where id=1');
        $acoes['morreu'] = $obit;
        mqtt_pub_alerta('morreu', 'critico');
        // Toca marcha funebre + Alexa le o obituario escrito pelo Claude
        alexa_funeral($obit);
    }
}

json_out(['ok' => true, 'acoes' => $acoes]);

function claude_obituario(string $ultima_ideia): string {
    $bin   = env('CLAUDE_BIN', 'claude');
    $model = env('CLAUDE_MODEL', 'claude-haiku-4-5');
    $prompt = "Escreva o obituario de um pato tamagotchi que morreu de fome porque o dono parou de ter ideias. "
            . "Tom: solene e absurdo. Max 3 frases. Cite a ultima ideia que ele comeu, entre aspas: \"$ultima_ideia\". "
            . "Responda SO o texto do obituario, sem introducao.";
    $cmd = sprintf(
        '%s -p %s --model %s --output-format text 2>&1',
        escapeshellcmd($bin),
        escapeshellarg($prompt),
        escapeshellarg($model)
    );
    return trim(shell_exec($cmd) ?? 'Aqui jaz o pato. RIP.');
}
