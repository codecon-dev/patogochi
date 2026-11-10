# Patogochi — server PHP

Servidor que guarda o cérebro do pato tamagotchi. Roda na `cobranca-vm` (OCI),
atrás do Caddy compartilhado em `patogochi.codegus.com`.

## Stack

- PHP 8.3-fpm + nginx + supervisord num container
- PDO pgsql → Supabase Postgres `patogochi` (sa-east-1, ref `yrzaovsbhdpcqwrdpuxe`)
- Claude via CLI `@anthropic-ai/claude-code` (sessão do usuário ubuntu, montada `/home/ubuntu/.claude:/root/.claude:ro`)
- Rede Docker `codegus-cobranca_cobranca-net` (Caddy pega pelo nome `patogochi:8080`)

## Endpoints

| Método | Path | Descrição |
|---|---|---|
| GET  | `/estado.php`       | retorna sprite + hp + coco + versão (aplica decaimento) |
| POST | `/ideia.php`        | body `{texto, origem}` → Claude classifica + alimenta pato |
| POST | `/marcar-feito.php` | body `{lembrete_id}` → limpa cocô |
| POST | `/reviver.php`      | body `{metodo: desculpa\|massagem, ...}` revive morto |
| POST | `/cron.php`         | header `X-Cron-Token` → vence lembretes, mata se fome=0 há 12h |

## Rodando local

```bash
cp config/.env.example config/.env
# preenche DB_PASSWORD (Supabase) e CRON_TOKEN
docker compose up --build
```

## Deploy na cobranca-vm

```bash
rsync -av --exclude '.git' server/ ubuntu@144.22.209.97:/opt/patogochi/
ssh ubuntu@144.22.209.97 'cd /opt/patogochi && docker compose up -d --build'
```

## Cron (host)

```
*/5 * * * * curl -sS -H "X-Cron-Token: <token>" https://patogochi.codegus.com/cron.php > /dev/null
```

## MQTT (push pro Core2)

Eventos em tempo real: estado, LED, cocô, alerta.
Ver [`MQTT.md`](MQTT.md) pra tópicos, payloads e snippet ESP32.
