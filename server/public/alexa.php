<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/db.php';
require_once __DIR__ . '/../src/regras.php';
require_once __DIR__ . '/../src/claude.php';

// --- Validacao de assinatura Amazon ---
// Pra skills em modo dev da pra pular. Quando publicar skill, descomentar.
// Mais info: https://developer.amazon.com/en-US/docs/alexa/custom-skills/host-a-custom-skill-as-a-web-service.html

$raw  = file_get_contents('php://input') ?: '';
$body = json_decode($raw, true) ?: [];

if (!isset($body['request'])) {
    json_err('request invalida', 400);
}

$type   = $body['request']['type']   ?? '';
$intent = $body['request']['intent']['name'] ?? '';
$slots  = $body['request']['intent']['slots'] ?? [];

$resp = match (true) {
    $type === 'LaunchRequest'               => alexa_launch(),
    $intent === 'StatusIntent'              => alexa_status(),
    $intent === 'CocosIntent'               => alexa_cocos(),
    $intent === 'AlimentarIntent'           => alexa_alimentar($slots),
    $intent === 'ReviverIntent'             => alexa_reviver($slots),
    $intent === 'AMAZON.HelpIntent'         => alexa_speak(
        'Pode perguntar como eu tô, quantos cocôs tenho, ou me alimentar com uma ideia. Fala aí.',
        false
    ),
    $intent === 'AMAZON.StopIntent'         => alexa_speak('Tchau, dono.', true),
    $intent === 'AMAZON.CancelIntent'       => alexa_speak('Tá, deixa pra lá.', true),
    $intent === 'AMAZON.FallbackIntent'     => alexa_speak('Não entendi, dono. Tenta de novo?', false),
    $type === 'SessionEndedRequest'         => alexa_resp(''),
    default                                 => alexa_speak('Hã? Repete aí.', false),
};

header('Content-Type: application/json; charset=utf-8');
echo json_encode($resp, JSON_UNESCAPED_UNICODE);
exit;

// ============================================================
// Intent handlers
// ============================================================

function alexa_launch(): array {
    $p = pato_lido_com_decaimento();
    $coco = coco_contador();
    $base = match ($p['estado']) {
        'morto'        => 'Eu tô morto, dono. Diz "revive o pato" com uma desculpa.',
        'passando_mal' => 'Tô passando mal. Da próxima, escolhe melhor as ideias.',
        'triste_fome' => 'Tô com fome. Me alimenta com alguma ideia.',
        default        => 'Oi dono, quer saber como tô ou tem ideia nova pra me alimentar?',
    };
    if ($coco > 0 && $p['estado'] !== 'morto') {
        $base .= " E ó, tem $coco cocô na tela.";
    }
    return alexa_speak($base, false);
}

function alexa_status(): array {
    $p = pato_lido_com_decaimento();
    $coco = coco_contador();
    $hp = (int) $p['fome'];
    $estado = $p['estado'];

    $frase_estado = match ($estado) {
        'morto'        => "Tô morto. Fome zero, ninguém me alimentou.",
        'passando_mal' => "Tô passando mal, última ideia foi ruim.",
        'empolgado'    => "Tô empolgado, última ideia foi boa.",
        'debochando'   => "Tô debochando aqui, a ideia foi meio sem graça.",
        'triste_fome' => "Tô com fome, HP $hp. Me alimenta.",
        'dormindo'    => "Tava cochilando, mas pode falar.",
        default        => "Tô de boa, HP $hp.",
    };
    if ($coco > 0 && $estado !== 'morto') {
        $frase_estado .= " E tenho $coco cocô na tela.";
    }
    return alexa_speak($frase_estado, true);
}

function alexa_cocos(): array {
    $total = coco_contador();
    if ($total === 0) {
        return alexa_speak('Zero cocôs. Tela limpa, dono.', true);
    }
    $lembrete = db()->query('
        select texto from lembretes
         where virou_coco=true and feito_em is null
         order by prazo asc limit 1
    ')->fetchColumn();
    $frase = "$total cocôs acumulados.";
    if ($lembrete) {
        $frase .= " O mais antigo é: $lembrete.";
    }
    return alexa_speak($frase, true);
}

function alexa_alimentar(array $slots): array {
    $texto = trim((string)($slots['ideia']['value'] ?? ''));
    if ($texto === '') {
        return alexa_speak('Fala uma ideia pra eu comer.', false);
    }

    // pato morto?
    $p = db()->query('select estado from pato where id=1')->fetch();
    if ($p && $p['estado'] === 'morto') {
        return alexa_speak('Tô morto, não como. Me revive primeiro.', true);
    }

    $class = claude_classifica_ideia($texto);
    $texto_final = !empty($class['titulo'])
        ? $class['titulo']
        : (!empty($class['texto_corrigido']) ? $class['texto_corrigido'] : $texto);

    db()->prepare('
        insert into ideias (origem, texto, tipo, opiniao, prazo)
        values (:o, :t, :tp, :op, :pz) returning id
    ')->execute([
        'o'=>'alexa', 't'=>$texto_final, 'tp'=>$class['tipo'], 'op'=>'', 'pz'=>$class['prazo'],
    ]);
    $ideia_id = (int) db()->lastInsertId('ideias_id_seq');

    if (!empty($class['prazo'])) {
        $texto_lembrete = !empty($class['titulo']) ? $class['titulo'] : $texto_final;
        db()->prepare('insert into lembretes (ideia_id, texto, prazo) values (:i,:t,:p)')
            ->execute(['i'=>$ideia_id, 't'=>$texto_lembrete, 'p'=>$class['prazo']]);
    }

    pato_alimenta($class['tipo'], $class['qualidade'], $texto_final);

    $frase = match ($class['tipo']) {
        'pipoca'           => 'Comi uma pipoca. Boa ideia.',
        'moeda_chocolate'  => 'Comi uma moeda de chocolate. Cheiro de dinheiro.',
        'biscoito_sorte'   => 'Biscoito da sorte com lembrete. Anotei.',
        'minhoca'          => 'Minhoca, ideia sem pé nem cabeça, mas comi.',
        'comida_estragada' => 'Comida estragada. Tô passando mal, sério mesmo.',
        default            => 'Comi.',
    };
    return alexa_speak($frase, true);
}

function alexa_reviver(array $slots): array {
    $p = db()->query('select * from pato where id=1')->fetch();
    if ($p['estado'] !== 'morto') {
        return alexa_speak('Tô vivo ainda, dono. Não precisa reviver nada.', true);
    }

    $desculpa = trim((string)($slots['desculpa']['value'] ?? ''));
    if ($desculpa === '') {
        return alexa_speak('Fala sua desculpa. "Pato, me desculpa porque..."', false);
    }

    // Claude julga sinceridade
    $bin   = env('CLAUDE_BIN', 'claude');
    $model = env('CLAUDE_MODEL', 'claude-haiku-4-5');
    $prompt = "Voce eh um pato morto julgando se o dono realmente se arrependeu. "
            . "Responda SO SIM ou NAO:\n\n$desculpa";
    $cmd = sprintf('%s -p %s --model %s --output-format text 2>&1',
        escapeshellcmd($bin), escapeshellarg($prompt), escapeshellarg($model));
    $r = strtoupper(trim(shell_exec($cmd) ?? ''));

    if (!str_starts_with($r, 'SIM')) {
        return alexa_speak('Não acreditei. Tenta de novo, mais sincero.', false);
    }

    db()->exec("update pato set estado='parado', fome=50, estado_ate=null, morto_desde=null, invulneravel_ate=now() + interval '1 hour', versao=versao+1, atualizado_em=now() where id=1");
    db()->exec('update mortes set reviveu_em=now() where reviveu_em is null');
    return alexa_speak('Tudo bem, voltei. Mas da próxima me alimenta direito.', true);
}

// ============================================================
// Helpers
// ============================================================

function alexa_speak(string $texto, bool $fecha): array {
    return alexa_resp($texto, $fecha);
}

function alexa_resp(string $texto = '', bool $fecha = true): array {
    return [
        'version' => '1.0',
        'response' => [
            'outputSpeech' => [
                'type' => 'PlainText',
                'text' => $texto,
            ],
            'shouldEndSession' => $fecha,
        ],
    ];
}
