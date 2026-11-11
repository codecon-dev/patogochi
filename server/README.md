# Patogochi — server PHP

Servidor que guarda o cérebro do pato tamagotchi. Roda na `cobranca-vm` (OCI),
atrás do Caddy compartilhado em `patogochi.codegus.com`.

## Stack

- **PHP 8.3-fpm + nginx + supervisord** num container
- **Postgres** via Supabase (Postgres 17, sa-east-1, ref `yrzaovsbhdpcqwrdpuxe`)
- **Claude** via CLI `@anthropic-ai/claude-code` (session no volume `patogochi_claude_home`)
- **whisper.cpp server** (modelo `ggml-base-q5_1`, ~4-5s por áudio de 8s)
- **Mosquitto MQTT** TLS 8883 pra push server→Core2 (`patogochi/estado`, `patogochi/led`, `patogochi/coco`, `patogochi/alerta`)
- **Voice Monkey** v3 pra Alexa anunciar alertas (passing_mal, morte com marcha fúnebre, votos na galeria)
- Rede Docker `codegus-cobranca_cobranca-net` (Caddy pega pelo nome `patogochi:8080`)

## Regras do pato

- Fome 0-100. Decai **~33/h de dia** (07h-20h) e **~8/h de noite** (21h-06h) → cheio a vazio em ~3h de dia.
- Fome=0 por **30 min** → `morto`. Cron detecta e gera obituário.
- **Ideia alimenta** o pato (pipoca/moeda_chocolate +30, minhoca +15, comida_estragada +5).
- **Lembrete (biscoito_sorte) NÃO alimenta** — só cria tarefa. Vencida, vira cocô na tela.
- Reviver dá +50 HP e **escudo de 1h** (não morre nesse período).

## Endpoints

| Método | Path | Protegido? | Descrição |
|---|---|---|---|
| GET  | `/estado.php`       | — | Sprite + HP + cocôs + versão (aplica decaimento) |
| POST | `/ideia.php`        | ✓ | Multipart (`audio`) ou JSON (`texto`) → Whisper + Claude classifica |
| GET  | `/lembretes.php`    | ✓ | Lista pendentes (vencidos primeiro) |
| POST | `/marcar-feito.php` | ✓ | Body `{lembrete_id}` → limpa cocô, devolve estado completo |
| POST | `/reviver.php`      | ✓ | Áudio OU JSON (`metodo: desculpa\|massagem`) → revive pato morto |
| POST | `/cron.php`         | token separado | Vence lembretes → cocô, mata se fome=0 há 30 min, gera obituário |
| POST | `/alexa.php`        | — (Amazon) | Webhook da Skill Alexa (Status, Alimentar, Cocos, Reviver intents) |
| GET  | `/feed.php`         | — | Feed público ranqueado por score (sem lembretes) |
| GET  | `/scores.php`       | — | Só scores das ideias pedidas (polling leve) |
| POST | `/voto.php`         | — | Body `{ideia_id, voto:-1\|0\|1, username}`. Alexa narra cada voto (rate limit 10s). |
| GET  | `/ideias`           | — | **Galeria pública** pixel art com votação |
| GET  | `/docs`             | — | **Swagger UI** |

Protegido = header `X-Api-Token: <API_TOKEN do .env>`.

## Classificação de ideia pelo Claude

Palavras-chave de lembrete → sempre `biscoito_sorte`: `me lembra`, `lembrar de`,
`anota`, `anotar`, `tenho que`, `preciso`, `nao esquecer`, `tarefa`, `pra fazer`.

Lembrete sem prazo explícito → prazo = fim do dia atual. Claude extrai um
**título limpo** da tarefa (sem "me lembra de", filler, etc).

## Rodando local

```bash
# 1. Sincronizar os sprites do repo pra pasta servida pelo nginx
./tools-sync-sprites.sh

# 2. Configurar env
cp config/.env.example config/.env
# preenche DB_PASSWORD (Supabase), API_TOKEN, CRON_TOKEN, MQTT_PASS, VOICEMONKEY_TOKEN/DEVICE

# 3. Gerar passwd do Mosquitto
docker run --rm -i eclipse-mosquitto:2 sh -c \
  "echo 'patogochi:SENHA' > /tmp/pw && mosquitto_passwd -U /tmp/pw && cat /tmp/pw" \
  | grep '^patogochi:' > mqtt/passwd

# 4. Subir
docker compose up --build
```

## Deploy na cobranca-vm

```bash
./tools-sync-sprites.sh
rsync -av --exclude '.git' --exclude 'config/.env' ./ ubuntu@144.22.209.97:/opt/patogochi/
ssh ubuntu@144.22.209.97 'cd /opt/patogochi && docker compose up -d --build'
```

## Cron (host)

```
*/5 * * * * curl -sS -H "X-Cron-Token: <token>" https://patogochi.codegus.com/cron.php > /dev/null
```

## MQTT

Push server→Core2 em tempo real (estado, LED, cocô, alerta).
Ver [`MQTT.md`](MQTT.md) pra tópicos, payloads e snippet ESP32.

## Alexa

Skill custom em [`alexa-interaction-model.json`](alexa-interaction-model.json).
Buildar no portal developer.amazon.com, apontar endpoint pra
`https://patogochi.codegus.com/alexa.php`.
