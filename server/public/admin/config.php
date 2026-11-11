<?php
declare(strict_types=1);

require_once __DIR__ . '/../../src/admin.php';
require_once __DIR__ . '/../../src/config.php';

require_admin();

if (($_SERVER['REQUEST_METHOD'] ?? 'GET') === 'GET') {
    // retorna configs + defaults pra UI mostrar
    json_out([
        'config'   => config_all(),
        'defaults' => CONFIG_DEFAULTS,
    ]);
}

if (($_SERVER['REQUEST_METHOD'] ?? '') === 'POST') {
    $body = read_json_body();
    $chaves_validas = array_keys(CONFIG_DEFAULTS);
    $atualizadas = [];
    foreach ($body as $k => $v) {
        if (!in_array($k, $chaves_validas, true)) continue;
        $valor = (string) $v;
        // valida numerico (todos os defaults são números)
        if (!is_numeric($valor)) continue;
        config_set($k, $valor);
        $atualizadas[$k] = $valor;
    }
    json_out(['ok' => true, 'atualizadas' => $atualizadas]);
}

json_err('método não permitido', 405);
