# API do Patogochi

Documentação oficial (back-end PHP): **https://patogochi.codegus.com/docs.html** (spec em `/openapi.yaml`).

## O que o firmware usa hoje

| Quando | Chamada | O que faz com a resposta |
|---|---|---|
| Solta o botão B | `POST /ideia.php` (multipart: `audio` = WAV 16 kHz mono, `origem` = `core2`) com `X-Api-Token` | `tipo` → comida na tela · `estado` → reação depois de comer |
| Fundo, a cada 10 s e depois de cada ideia | `GET /estado.php` | `sprite` → estado base do pato · `hp` → corações no status · `mortes` → lápide |
| Abre a tela de tarefas (A) | `GET /lembretes.php` **(pedido, ainda não existe)** com `X-Api-Token` | lista com checkbox |
| C na tarefa | `POST /marcar-feito.php` `{"lembrete_id": 12}` | marca o checkbox, quack feliz |
| Erro | — | Pato só debocha. Erro (`error`, `409` = morto) vai pro log serial e pro rótulo de debug |

Nada de texto na tela: o pato só reage.

Código: `tarefaEnvio` em `firmware/src/main.cpp`. Token e URL em `firmware/include/config.h`.

## Pedido: `GET /lembretes.php`

Com `X-Api-Token`. Lembretes **pendentes** (não feitos), vencidos primeiro:

```json
{ "lembretes": [
  { "id": 12, "texto": "pagar a energia", "vence_em": "2026-11-15T18:00:00-03:00", "vencido": true }
] }
```

## Reviver (`POST /reviver.php`)

- **Massagem:** JSON `{"metodo":"massagem","batidas":30,"bpm":110.4}` depois de 30 chacoalhadas com média entre 100 e 120 bpm.
- **Desculpa (pedido):** multipart igual ao `/ideia.php` + campo `metodo=desculpa` + `audio` (WAV). Hoje a API só aceita `texto`; o firmware já manda áudio.
- Resposta `{"ok":true,...}` → pato revive empolgado. `{"ok":false}` → pato debocha e continua morto.
