<?php
declare(strict_types=1);

require_once __DIR__ . '/env.php';

/**
 * Cliente MQTT leve: conecta, publica 1 msg, desconecta.
 * Fire-and-forget (QoS 0) com retain=true pra Core2 reconectar e pegar ultimo estado.
 *
 * Nao usa nenhuma biblioteca externa — monta o pacote MQTT 3.1.1 na mao.
 * Serve so pro nosso caso: PUBLISH authenticated + retain + QoS 0.
 */
function mqtt_publish(string $topic, array $payload, bool $retain = true): void {
    $host = env('MQTT_HOST', 'patogochi-mqtt');  // nome na rede docker
    $port = (int) env('MQTT_PORT', '1883');       // interno, sem TLS
    $user = env('MQTT_USER', 'patogochi');
    $pass = env('MQTT_PASS', '');
    if ($pass === '') return;  // nao configurado, silencia

    $body = json_encode($payload, JSON_UNESCAPED_UNICODE);

    $sock = @fsockopen($host, $port, $errno, $errstr, 2);
    if (!$sock) {
        error_log("mqtt_publish: nao conectou em $host:$port — $errstr");
        return;
    }
    stream_set_timeout($sock, 3);

    try {
        // --- CONNECT ---
        $client_id = 'phppub-' . bin2hex(random_bytes(4));
        $vh  = chr(0) . chr(4) . 'MQTT' . chr(4);  // protocol MQTT 3.1.1
        $vh .= chr(0xC2);                          // flags: user+pass, clean session
        $vh .= chr(0) . chr(60);                   // keepalive 60s
        $pl  = _mqtt_str($client_id) . _mqtt_str($user) . _mqtt_str($pass);
        _mqtt_write($sock, 0x10, $vh . $pl);

        // esperar CONNACK (4 bytes)
        $ack = fread($sock, 4);
        if (strlen($ack) < 4 || ord($ack[3]) !== 0) {
            error_log("mqtt_publish: CONNACK falhou: " . bin2hex($ack));
            return;
        }

        // --- PUBLISH (QoS 0, retain opcional) ---
        $pubvh = _mqtt_str($topic);
        $flags = 0x30 | ($retain ? 0x01 : 0x00);
        _mqtt_write($sock, $flags, $pubvh . $body);

        // --- DISCONNECT ---
        fwrite($sock, chr(0xE0) . chr(0));
    } finally {
        fclose($sock);
    }
}

function _mqtt_str(string $s): string {
    $len = strlen($s);
    return chr(($len >> 8) & 0xFF) . chr($len & 0xFF) . $s;
}

function _mqtt_write($sock, int $type_flags, string $body): void {
    $len = strlen($body);
    $out = chr($type_flags);
    // variable length encoding
    do {
        $byte = $len & 0x7F;
        $len >>= 7;
        if ($len > 0) $byte |= 0x80;
        $out .= chr($byte);
    } while ($len > 0);
    fwrite($sock, $out . $body);
}

// ============================================================
// Helpers de alto nivel pra cada evento do pato
// ============================================================

/** Converte estado do pato em config de LED (RGB + hz). */
function led_para_estado(string $estado, int $fome): array {
    // vermelho pulsante forte
    if ($estado === 'passando_mal' || $estado === 'morto') {
        return ['r' => 255, 'g' => 0, 'b' => 0, 'hz' => 2];
    }
    // laranja piscando — fome baixa
    if ($estado === 'triste_fome' || $fome < 30) {
        return ['r' => 255, 'g' => 80, 'b' => 0, 'hz' => 1];
    }
    // verde pulsante — comeu bem
    if ($estado === 'empolgado') {
        return ['r' => 0, 'g' => 255, 'b' => 60, 'hz' => 3];
    }
    // amarelo lento — deboche
    if ($estado === 'debochando') {
        return ['r' => 255, 'g' => 200, 'b' => 0, 'hz' => 1];
    }
    return ['r' => 0, 'g' => 0, 'b' => 0, 'hz' => 0];  // off
}

/** Publica mudanca de estado do pato (estado + led). */
function mqtt_pub_estado(array $pato): void {
    mqtt_publish('patogochi/estado', [
        'sprite'  => $pato['estado'],
        'estado'  => $pato['estado'],
        'hp'      => (int) $pato['fome'],
        'versao'  => (int) $pato['versao'],
    ]);
    mqtt_publish('patogochi/led',
        led_para_estado($pato['estado'], (int) $pato['fome'])
    );
}

/** Publica novo coco. */
function mqtt_pub_coco(int $lembrete_id, int $total): void {
    mqtt_publish('patogochi/coco', [
        'id'    => $lembrete_id,
        'total' => $total,
    ]);
}

/** Publica alerta urgente. */
function mqtt_pub_alerta(string $tipo, string $nivel = 'urgente'): void {
    mqtt_publish('patogochi/alerta', [
        'tipo'  => $tipo,
        'nivel' => $nivel,
    ], retain: false);  // alerta nao fica retido
}
