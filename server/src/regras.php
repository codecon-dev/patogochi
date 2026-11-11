<?php
declare(strict_types=1);

require_once __DIR__ . '/db.php';
require_once __DIR__ . '/mqtt.php';
require_once __DIR__ . '/alexa.php';

// Constantes do jogo (modo video — pato esvazia em 3h, morre 30 min depois)
const FOME_DECAIMENTO_POR_HORA_DIA   = 33.3;   // 100 -> 0 em 3h
const FOME_DECAIMENTO_POR_HORA_NOITE = 8.3;    // 1/4 do dia (pato dormindo)
const SONO_HORA_INICIO               = 21;     // 21:00 (America/Sao_Paulo)
const SONO_HORA_FIM                  = 7;      // 07:00
const FOME_BOOST_IDEIA_BOA           = 30;
const FOME_BOOST_IDEIA_MEH           = 15;
const FOME_BOOST_IDEIA_RUIM          = 5;
const FOME_LIMITE_TRISTE             = 30;
const PASSANDO_MAL_DURACAO_S         = 600;    // 10 min
const DEBOCHANDO_DURACAO_S           = 45;
const EMPOLGADO_DURACAO_S            = 60;
const MORRE_APOS_FOME_ZERO_S         = 30 * 60; // 30 min

/**
 * Calcula quantos "pontos de fome" o pato perdeu entre dois momentos,
 * considerando que durante o sono (SONO_HORA_INICIO - SONO_HORA_FIM)
 * o decaimento cai pro valor noturno.
 *
 * Integra em passos de 1 minuto pra não precisar fazer matemática chata
 * com intervalos cruzando meia-noite.
 */
function fome_decaimento_no_intervalo(DateTimeImmutable $de, DateTimeImmutable $ate): float {
    if ($ate <= $de) return 0.0;
    $tz = new DateTimeZone('America/Sao_Paulo');
    $cursor = $de->setTimezone($tz);
    $fim    = $ate->setTimezone($tz);
    $total = 0.0;
    // passo de 60s — sobra de 1 ponto no final é desprezível
    while ($cursor < $fim) {
        $h = (int) $cursor->format('G');
        $dormindo = $h >= SONO_HORA_INICIO || $h < SONO_HORA_FIM;
        $taxa_h = $dormindo ? FOME_DECAIMENTO_POR_HORA_NOITE : FOME_DECAIMENTO_POR_HORA_DIA;
        $total += $taxa_h / 60.0;  // 1 minuto
        $cursor = $cursor->modify('+1 minute');
    }
    return $total;
}

/**
 * Le o pato, aplica decaimento de fome baseado em tempo, salva se mudou,
 * retorna linha atualizada.
 */
function pato_lido_com_decaimento(): array {
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $p = $pdo->query('select * from pato where id=1 for update')->fetch();
        if (!$p) { throw new RuntimeException('pato nao inicializado'); }

        $agora = new DateTimeImmutable('now');
        $ult   = new DateTimeImmutable($p['atualizado_em']);
        $decaimento = fome_decaimento_no_intervalo($ult, $agora);

        $fome_nova = (int) max(0, $p['fome'] - round($decaimento));
        $estado    = $p['estado'];
        $estado_ate= $p['estado_ate'];

        // Expira estado temporário (passando_mal / empolgado)
        if ($estado_ate && new DateTimeImmutable($estado_ate) <= $agora) {
            $estado = 'parado';
            $estado_ate = null;
        }

        // Fome baixa → triste_fome (não sobrepõe passando_mal)
        if ($estado === 'parado' && $fome_nova < FOME_LIMITE_TRISTE) {
            $estado = 'triste_fome';
        }

        // Morte: fome=0 há mais de 12h, EXCETO se tiver escudo de invulnerabilidade ativo
        $invulneravel = !empty($p['invulneravel_ate'])
            && new DateTimeImmutable($p['invulneravel_ate']) > $agora;

        if ($p['estado'] !== 'morto' && $fome_nova === 0 && !$invulneravel) {
            if (!$p['morto_desde']) {
                $pdo->prepare('update pato set morto_desde=now() where id=1')->execute();
                $p['morto_desde'] = $agora->format('c');
            }
            $morto_desde = new DateTimeImmutable($p['morto_desde']);
            if ($agora->getTimestamp() - $morto_desde->getTimestamp() >= MORRE_APOS_FOME_ZERO_S) {
                $estado = 'morto';
            }
        } elseif ($fome_nova > 0 && $p['morto_desde']) {
            $pdo->prepare('update pato set morto_desde=null where id=1')->execute();
        }
        // Enquanto invulneravel, pato nunca chega a fome 0 (fica no minimo 1)
        if ($invulneravel && $fome_nova === 0) {
            $fome_nova = 1;
        }

        if ($fome_nova !== (int)$p['fome'] || $estado !== $p['estado']) {
            $pdo->prepare('
                update pato
                   set fome=:f, estado=:e, estado_ate=:ea, versao=versao+1, atualizado_em=now()
                 where id=1
            ')->execute([
                'f'=>$fome_nova, 'e'=>$estado, 'ea'=>$estado_ate,
            ]);
            $p['fome']=$fome_nova; $p['estado']=$estado; $p['estado_ate']=$estado_ate;
            $p['versao']=(int)$p['versao']+1;
        }
        $pdo->commit();
        return $p;
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
}

/** Conta cocôs pendentes na tela. */
function coco_contador(): int {
    return (int) db()->query(
        'select count(*) from lembretes where virou_coco=true and feito_em is null'
    )->fetchColumn();
}

/** Monta JSON de estado padrão que /estado.php e /marcar-feito.php retornam. */
function estado_json(?array $p = null): array {
    $p ??= pato_lido_com_decaimento();
    return [
        'sprite'         => $p['estado'],
        'estado'         => $p['estado'],
        'hp'             => (int) $p['fome'],
        'coco'           => coco_contador(),
        'mortes'         => (int) $p['mortes'],
        'ultima_opiniao' => $p['ultima_opiniao'],
        'versao'         => (int) $p['versao'],
    ];
}

/**
 * Aplica efeito da ideia classificada no pato.
 * $qualidade: 'boa' (empolgado) ou 'meh' (debochando). Ignorado para comida_estragada.
 * $texto_ideia: texto bruto da ideia (opcional) — se for comida_estragada, Alexa usa pra comentar.
 *
 * biscoito_sorte (lembrete) NÃO alimenta: só anota a tarefa, pato continua como estava.
 * Retorna o pato sem mudanças.
 */
function pato_alimenta(string $tipo, string $qualidade = 'boa', string $texto_ideia = ''): array {
    // Lembrete não é comida — não afeta fome nem estado
    if ($tipo === 'biscoito_sorte') {
        return pato_lido_com_decaimento();
    }

    if ($tipo === 'comida_estragada') {
        $estado = 'passando_mal';
        $boost  = FOME_BOOST_IDEIA_RUIM;
        $dur    = PASSANDO_MAL_DURACAO_S;
    } elseif ($qualidade === 'meh') {
        $estado = 'debochando';
        $boost  = FOME_BOOST_IDEIA_MEH;
        $dur    = DEBOCHANDO_DURACAO_S;
    } else {
        $estado = 'empolgado';
        $boost  = FOME_BOOST_IDEIA_BOA;
        $dur    = EMPOLGADO_DURACAO_S;
    }

    db()->prepare('
        update pato
           set fome = least(100, fome + :b),
               estado = :e,
               estado_ate = now() + make_interval(secs => :d),
               ultima_refeicao = now(),
               ultima_opiniao = null,
               versao = versao + 1,
               atualizado_em = now()
         where id = 1
    ')->execute(['b'=>$boost, 'e'=>$estado, 'd'=>$dur]);

    $p = db()->query('select * from pato where id=1')->fetch();
    mqtt_pub_estado($p);

    // Alexa so anuncia quando o pato passa mal (nao em cada refeicao boa)
    if ($p['estado'] === 'passando_mal') {
        $contexto = $texto_ideia !== ''
            ? "O dono sugeriu a ideia \"$texto_ideia\" e o pato classificou como ruim/cliche. Ele ta passando mal agora. Comece a frase com \"Que ideia horrivel!\" e complete comentando por que aquela ideia foi ruim em 1 frase curta."
            : 'O pato comeu uma ideia ruim e ta passando mal. Comece com "Que ideia horrivel!" e complete com uma frase sarcastica.';
        alexa_fala_sarcastica($contexto, 'Que ideia horrivel! O pato ta passando mal agora.');
    }

    return $p;
}
