<?php
declare(strict_types=1);

require_once __DIR__ . '/db.php';

/**
 * Config dinâmica (tabela config no Postgres).
 * Cache de 10s em memória do processo pra não martelar o DB em cada chamada.
 *
 * Fallback pros defaults atuais do jogo quando chave não existe no DB —
 * assim o servidor funciona mesmo com a tabela vazia.
 */

const CONFIG_DEFAULTS = [
    'fome_decaimento_dia'    => '33.3',   // por hora, 07h-20h
    'fome_decaimento_noite'  => '1.0',    // por hora, 21h-06h
    'sono_hora_inicio'       => '21',
    'sono_hora_fim'          => '7',
    'fome_boost_boa'         => '30',
    'fome_boost_meh'         => '15',
    'fome_boost_ruim'        => '5',
    'fome_limite_triste'     => '30',
    'passando_mal_duracao_s' => '600',
    'debochando_duracao_s'   => '45',
    'empolgado_duracao_s'    => '60',
    'morre_apos_fome_zero_s' => '1800',   // 30 min
    'invulneravel_apos_reviver_s' => '3600',
];

function config_all(): array {
    static $cache = null;
    static $cache_ate = 0;
    if ($cache !== null && time() < $cache_ate) return $cache;

    $db_values = [];
    try {
        $stmt = db()->query('select chave, valor from config');
        foreach ($stmt->fetchAll() as $r) {
            $db_values[$r['chave']] = $r['valor'];
        }
    } catch (Throwable $e) {
        // tabela não existe ainda ou erro de conexão — segue com defaults
    }

    $cache = array_merge(CONFIG_DEFAULTS, $db_values);
    $cache_ate = time() + 10;
    return $cache;
}

function config(string $chave, string|float|int|null $default = null): string {
    $all = config_all();
    if (isset($all[$chave])) return $all[$chave];
    if ($default !== null) return (string) $default;
    return CONFIG_DEFAULTS[$chave] ?? '';
}

function config_float(string $chave): float {
    return (float) config($chave);
}

function config_int(string $chave): int {
    return (int) config($chave);
}

/** Salva uma chave (upsert). Usado pelo painel admin. */
function config_set(string $chave, string $valor): void {
    db()->prepare('
        insert into config (chave, valor) values (:k, :v)
        on conflict (chave) do update set valor = excluded.valor, atualizado_em = now()
    ')->execute(['k' => $chave, 'v' => $valor]);
}
