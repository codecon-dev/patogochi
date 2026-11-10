<?php
declare(strict_types=1);

require_once __DIR__ . '/env.php';

/**
 * Faz Alexa falar algo via Voice Monkey (um gateway que dispara Routines).
 *
 * O monkey na dashboard precisa ter uma Routine na Alexa configurada com
 * "When Voice Monkey says..." -> "Alexa Says: {text}" (variavel dinamica).
 *
 * Fire-and-forget: se Voice Monkey estiver fora, logamos mas nao falhamos
 * a request original (nao queremos que o pato quebre por causa da Alexa).
 */
function alexa_fala(string $texto, int $timeout = 2): void {
    _vm_call('announce', env('VOICEMONKEY_DEVICE', ''), $texto, $timeout);
}

/**
 * Alexa anuncia morte com marcha funebre + obituario via SSML.
 * Usa <audio src="..."/> apontando pro marcha.mp3 servido pelo patogochi.
 * SSML Alexa: audio mp3 mono 22kHz 48kbps, max 240s.
 */
function alexa_funeral(string $obituario, int $timeout = 4): void {
    $token  = env('VOICEMONKEY_TOKEN', '');
    $device = env('VOICEMONKEY_DEVICE', '');
    if ($token === '' || $device === '') return;

    // escapa &, <, > pro SSML
    $safe = htmlspecialchars($obituario, ENT_XML1 | ENT_QUOTES, 'UTF-8');
    $ssml = '<speak>'
          . '<audio src="https://patogochi.codegus.com/marcha.mp3"/>'
          . '<break time="500ms"/>'
          . $safe
          . '</speak>';

    $url = 'https://api-v3.voicemonkey.io/announce'
         . '?token='  . urlencode($token)
         . '&device=' . urlencode($device)
         . '&speech=' . urlencode($ssml);

    $ctx = stream_context_create([
        'http' => ['method' => 'GET', 'timeout' => $timeout, 'ignore_errors' => true],
    ]);
    @file_get_contents($url, false, $ctx);
}

/**
 * Rate-limita anúncios pela Alexa por "canal" (ex: 'voto', 'passando_mal', etc).
 * Retorna true se pode falar, false se ainda está dentro do cooldown.
 * Atualiza o timestamp do ultimo anuncio no sucesso.
 */
function alexa_pode_falar(string $canal, int $cooldown_s = 10): bool {
    $arq = sys_get_temp_dir() . "/alexa_last_$canal";
    if (is_file($arq)) {
        $last = (int) file_get_contents($arq);
        if ((time() - $last) < $cooldown_s) {
            return false;
        }
    }
    file_put_contents($arq, (string) time());
    return true;
}

function _vm_call(string $endpoint, string $device, string $texto, int $timeout): void {
    $token = env('VOICEMONKEY_TOKEN', '');
    if ($token === '' || $device === '') return;

    $url = "https://api-v3.voicemonkey.io/$endpoint"
         . '?token='  . urlencode($token)
         . '&device=' . urlencode($device)
         . '&speech=' . urlencode($texto);

    $ctx = stream_context_create([
        'http' => [
            'method'  => 'GET',
            'timeout' => $timeout,
            'ignore_errors' => true,
        ],
    ]);
    $res = @file_get_contents($url, false, $ctx);
    if ($res === false) {
        error_log("alexa_fala: falha ao chamar Voice Monkey");
    }
}

/**
 * Alexa fala frase gerada pelo Claude, com um prompt curto por contexto.
 * Fallback: frases fixas se Claude falhar.
 */
function alexa_fala_sarcastica(string $contexto, string $fallback): void {
    require_once __DIR__ . '/env.php';
    $bin   = env('CLAUDE_BIN', 'claude');
    $model = env('CLAUDE_MODEL', 'claude-haiku-4-5');

    $prompt = "Voce eh a Alexa anunciando o estado de um pato tamagotchi na casa do dono. "
            . "Gere UMA frase curta (max 15 palavras), sarcastica, em pt-BR, sem introducao, "
            . "sem aspas, como se a Alexa fosse falar em voz alta agora. "
            . "Contexto: $contexto";

    $cmd = sprintf(
        '%s -p %s --model %s --output-format text 2>&1',
        escapeshellcmd($bin),
        escapeshellarg($prompt),
        escapeshellarg($model)
    );
    $r = trim(shell_exec($cmd) ?? '');
    if ($r === '' || strlen($r) > 250) {
        $r = $fallback;
    }
    alexa_fala($r);
}
