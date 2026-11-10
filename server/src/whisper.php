<?php
declare(strict_types=1);

/**
 * Transcreve audio (opus/wav/mp3/m4a/webm) → texto pt-BR via whisper-server local.
 *
 * whisper-server fica rodando permanentemente dentro do container (supervisord)
 * com o modelo small-q5_1 já carregado na memória. Economiza ~4s por request
 * em relação a rodar whisper-cli (que recarrega o modelo a cada chamada).
 *
 * Pipeline:
 *   1. ffmpeg: formato qualquer → WAV 16kHz mono
 *   2. POST multipart pro whisper-server em http://127.0.0.1:8081/inference
 */

const WHISPER_URL = 'http://127.0.0.1:8081/inference';

function whisper_transcreve(string $audio_path): string {
    if (!is_readable($audio_path)) {
        throw new RuntimeException("audio nao legivel: $audio_path");
    }

    $tmp = sys_get_temp_dir() . '/whisper_' . bin2hex(random_bytes(6));
    $wav = $tmp . '.wav';

    // 1) converte pra WAV 16kHz mono
    $cmd = sprintf(
        'ffmpeg -y -i %s -ar 16000 -ac 1 -f wav %s 2>&1',
        escapeshellarg($audio_path),
        escapeshellarg($wav)
    );
    exec($cmd, $out, $rc);
    if ($rc !== 0 || !is_file($wav)) {
        throw new RuntimeException('ffmpeg falhou: ' . implode("\n", $out));
    }

    // 2) POST multipart pro whisper-server (modelo ja carregado na memoria)
    try {
        $texto = _whisper_http($wav);
    } finally {
        @unlink($wav);
    }

    if ($texto === '') {
        throw new RuntimeException('whisper-server retornou vazio');
    }
    return $texto;
}

function _whisper_http(string $wav_path): string {
    $ch = curl_init(WHISPER_URL);
    curl_setopt_array($ch, [
        CURLOPT_POST           => true,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT        => 60,
        CURLOPT_CONNECTTIMEOUT => 3,
        CURLOPT_POSTFIELDS     => [
            'file'           => new CURLFile($wav_path, 'audio/wav', 'audio.wav'),
            'response_format'=> 'text',
            'language'       => 'pt',
            'temperature'    => '0',
        ],
    ]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $err  = curl_error($ch);
    curl_close($ch);

    if ($body === false || $code !== 200) {
        throw new RuntimeException("whisper-server HTTP $code: " . ($err ?: $body));
    }
    return trim((string) $body);
}
