<?php
// Servidor de mentira do Patogochi: segue o API.md, sem transcrição nem Claude.
// Serve pra testar o Core2 enquanto o back-end real não existe.
//
// Rodar (da raiz do repo):
//   php -S 0.0.0.0:8080 tools/mock-servidor/index.php
//
// Cada áudio recebido fica salvo em tools/mock-servidor/gravacoes/ pra você ouvir.

const TOKEN = 'troque-isso';  // mesmo valor de PATO_TOKEN em firmware/include/config.h

header('Content-Type: application/json; charset=utf-8');

function responde(int $status, array $dados): void {
    http_response_code($status);
    echo json_encode($dados, JSON_UNESCAPED_UNICODE);
    exit;
}

function loga(string $msg): void {
    file_put_contents('php://stderr', '[pato] ' . $msg . "\n");
}

if (($_SERVER['HTTP_X_PATO_TOKEN'] ?? '') !== TOKEN) {
    responde(401, ['erro' => 'token errado']);
}

$rota = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
$metodo = $_SERVER['REQUEST_METHOD'];

if ($metodo === 'POST' && $rota === '/ideia') {
    $tipo = $_SERVER['CONTENT_TYPE'] ?? '';
    $corpo = file_get_contents('php://input');

    if (str_starts_with($tipo, 'audio/wav')) {
        $dir = __DIR__ . '/gravacoes';
        if (!is_dir($dir)) mkdir($dir);
        $arquivo = $dir . '/' . date('Ymd-His') . '.wav';
        file_put_contents($arquivo, $corpo);
        $segundos = max(0, strlen($corpo) - 44) / (16000 * 2);
        $texto = sprintf('(mock) ouvi %.1f segundos de áudio', $segundos);
        loga("áudio salvo: $arquivo (" . round($segundos, 1) . 's)');
    } elseif (str_starts_with($tipo, 'application/json')) {
        $texto = json_decode($corpo, true)['texto'] ?? '';
        loga("texto: $texto");
    } else {
        responde(415, ['erro' => 'mande audio/wav ou application/json']);
    }

    $opcoes = [
        ['pipoca', 'empolgado', 'Isso aí eu assistiria. Comi feliz.'],
        ['moeda_chocolate', 'empolgado', 'Cheiro de dinheiro. Gostei.'],
        ['minhoca', 'debochando', 'Mais um side project que você não vai terminar.'],
        ['biscoito_sorte', 'empolgado', 'Profundo. Quase chorei.'],
        ['comida_estragada', 'passando_mal', 'Essa ideia me deu dor de barriga.'],
    ];
    [$comida, $reacao, $opiniao] = $opcoes[array_rand($opcoes)];

    responde(200, [
        'texto' => $texto,
        'comida' => $comida,
        'reacao' => $reacao,
        'opiniao' => $opiniao,
        'lembrete' => null,
    ]);
}

if ($metodo === 'GET' && $rota === '/estado') {
    responde(200, ['fome' => 72, 'cocos' => 0, 'morto' => false, 'mortes' => 0, 'lembretes_vencidos' => []]);
}

responde(404, ['erro' => "rota não existe: $metodo $rota"]);
