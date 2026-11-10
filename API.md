# API do Patogochi

Documentação oficial (back-end PHP): **https://patogochi.codegus.com/docs.html** (spec em `/openapi.yaml`).

## O que o firmware usa hoje

| Quando | Chamada | O que faz com a resposta |
|---|---|---|
| Solta o botão B | `POST /ideia.php` (multipart: `audio` = WAV 16 kHz mono, `origem` = `core2`) com `X-Api-Token` | `tipo` → comida na tela · `estado` → reação depois de comer |
| Erro | — | Pato só debocha. Erro (`error`, `409` = morto) vai pro log serial e pro rótulo de debug |

Nada de texto na tela: o pato só reage.

Código: `tarefaEnvio` em `firmware/src/main.cpp`. Token e URL em `firmware/include/config.h`.

## Ainda não usado

- `GET /estado.php` em polling (~2 min) pra fome, cocôs, mortes e sprite base
- `POST /marcar-feito.php` (tirar cocô)
- `POST /reviver.php` (desculpa e massagem cardíaca)
