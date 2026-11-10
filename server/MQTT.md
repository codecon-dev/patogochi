# Patogochi MQTT

Canal **push** do servidor pro Core2 (e qualquer outro cliente IoT).
HTTP cobre read/write por polling; MQTT cobre eventos em tempo real.

## Broker

| Item | Valor |
|---|---|
| Host | `patogochi.codegus.com` |
| Porta | **`8883`** (TLS) |
| TLS | Let's Encrypt (cert do mesmo domínio HTTPS) |
| Auth | user `patogochi` / password |
| Protocolo | MQTT 3.1.1 |
| Clean session | `true` recomendado |
| Keepalive | 60s |

Senha é a mesma do `.env` da VM. Pedir pro Gustavo pessoalmente, não comitar.

## Tópicos

Todos os eventos saem com **retain=true** (menos alerta), então o Core2
reconecta e já recebe o último valor.

### `patogochi/estado`
Sempre que o pato come, revive ou cron detecta mudança.

```json
{
  "sprite": "empolgado",
  "estado": "empolgado",
  "hp": 78,
  "versao": 42
}
```

Valores possíveis de `sprite`/`estado`:
`parado`, `ouvindo`, `pensando`, `comendo`, `empolgado`, `debochando`,
`passando_mal`, `triste_fome`, `dormindo`, `morto`.

### `patogochi/led`
Config da barra de LED. RGB + frequência de pulso (Hz).

```json
{"r": 0, "g": 255, "b": 60, "hz": 3}
```

- `hz: 0` → LED fixo (sem pulsar)
- `hz: 1-3` → pulsar na frequência
- `r:0, g:0, b:0` → apagar

Mapeamento atual no server (`src/mqtt.php` → `led_para_estado`):

| Estado do pato | Cor | hz |
|---|---|---|
| `passando_mal` / `morto` | vermelho (255,0,0) | 2 |
| `triste_fome` / fome<30 | laranja (255,80,0) | 1 |
| `empolgado` | verde (0,255,60) | 3 |
| `debochando` | amarelo (255,200,0) | 1 |
| outros | apagado | 0 |

### `patogochi/coco`
Lembrete venceu → novo cocô, OU cocô foi marcado como feito.

```json
{"id": 12, "total": 3}
```

Quando `id: 0` + campo `removido`, é um cocô que foi marcado como feito:
```json
{"id": 0, "total": 2, "removido": 12}
```

Core2 mantém o `total` sincronizado e decide quantos cocôs desenhar.

### `patogochi/alerta`
Evento pontual (NÃO retido). Core2 pode tocar som ou destacar na tela.

```json
{"tipo": "morreu", "nivel": "critico"}
```

Tipos atuais: `morreu`, `reviveu`. Níveis: `info`, `urgente`, `critico`.

## Cliente ESP32 (Core2)

Lib recomendada: [`PubSubClient`](https://github.com/knolleary/pubsubclient) +
`WiFiClientSecure`.

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

WiFiClientSecure tls;
PubSubClient mqtt(tls);

void setup() {
  tls.setInsecure();                   // pula verificacao de cert (prototipo)
  // OU, pra producao: tls.setCACert(ISRG_ROOT_X1_PEM);

  mqtt.setServer("patogochi.codegus.com", 8883);
  mqtt.setCallback(on_mqtt);
  mqtt.setBufferSize(1024);

  while (!mqtt.connected()) {
    if (mqtt.connect("core2-pato", "patogochi", "<senha>")) {
      mqtt.subscribe("patogochi/estado");
      mqtt.subscribe("patogochi/led");
      mqtt.subscribe("patogochi/coco");
      mqtt.subscribe("patogochi/alerta");
    } else {
      delay(2000);
    }
  }
}

void loop() {
  mqtt.loop();
}

void on_mqtt(char* topic, byte* payload, unsigned int len) {
  // parse JSON com ArduinoJson
  // if topic == patogochi/led -> setLED(r,g,b,hz)
  // if topic == patogochi/estado -> drawSprite(sprite)
  // if topic == patogochi/coco -> redrawCocos(total)
  // if topic == patogochi/alerta -> buzz()
}
```

## Debug (do Mac)

Instalar cliente mosquitto:
```bash
brew install mosquitto
```

Subscribe em tudo:
```bash
mosquitto_sub -h patogochi.codegus.com -p 8883 \
  -u patogochi -P '<senha>' \
  --capath /etc/ssl/certs \
  -t 'patogochi/#' -v
```

Publish manual pra testar Core2 isoladamente:
```bash
mosquitto_pub -h patogochi.codegus.com -p 8883 \
  -u patogochi -P '<senha>' \
  --capath /etc/ssl/certs \
  -t 'patogochi/led' \
  -m '{"r":255,"g":0,"b":0,"hz":2}'
```

## Notas operacionais

- Mosquitto roda como container `patogochi-mqtt` na mesma rede Docker do PHP.
- PHP publica via `src/mqtt.php` sem lib externa (fsockopen + protocolo MQTT na mão).
- Cert LE é copiado do volume do Caddy pra `/opt/patogochi-mqtt/certs/`.
  Quando renovar, re-copiar e `docker compose restart patogochi-mqtt`
  (ver `tools/renova-cert-mqtt.sh` — TODO).
- Porta 8883 aberta na Security List OCI da `cobranca-vm`.
