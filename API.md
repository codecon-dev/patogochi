# API do Patogochi

Contrato entre o Core2 (firmware) e o back-end PHP.
Tem um servidor de mentira que segue esse contrato em `tools/mock-servidor/`, pra testar o Core2 antes do back-end real ficar pronto.

## Autenticação

Toda requisição manda o header `X-Pato-Token: <token>`. Token errado → `401`.

---

## `POST /ideia`

Recebe uma ideia, devolve o que o pato faz com ela.

### Entrada: áudio (Core2)

```
Content-Type: audio/wav
Body: WAV PCM 16 bits, mono, 16 kHz (até ~10 s, ~320 KB)
```

### Entrada: texto (Alexa, testes)

```
Content-Type: application/json
{ "texto": "ideia de vídeo sobre um pato que julga minhas ideias" }
```

### Resposta `200`

```json
{
  "texto": "ideia de vídeo sobre um pato que julga minhas ideias",
  "comida": "pipoca",
  "reacao": "empolgado",
  "opiniao": "Isso aí eu assistiria. Comi feliz.",
  "lembrete": null
}
```

| Campo | Tipo | Valores |
|---|---|---|
| `texto` | string | Transcrição do áudio (ou o texto recebido) |
| `comida` | string | `pipoca` (conteúdo/vídeo) · `moeda_chocolate` (negócio) · `minhoca` (técnica/código) · `biscoito_sorte` (vida/futuro) · `comida_estragada` (ideia ruim) |
| `reacao` | string | `empolgado` · `debochando` · `passando_mal` (sempre que `comida_estragada`) |
| `opiniao` | string | Opinião sincera do pato, **máximo ~90 caracteres** (cabe em 3 linhas na tela) |
| `lembrete` | objeto ou `null` | `{ "texto": "gravar a publi", "vence_em": "2026-11-14T18:00:00-03:00" }` quando a ideia tem prazo |

Se a fala não foi uma ideia (silêncio, ruído, "testando"), responder `comida: null`, `reacao: "debochando"` e uma opinião zoando.

### Erros

`4xx`/`5xx` com `{ "erro": "mensagem curta" }`. O Core2 mostra a mensagem num balão.

---

## `GET /estado` (etapa 4+)

Estado do pato, que mora no servidor. O Core2 consulta de tempos em tempos.

```json
{
  "fome": 72,
  "cocos": 1,
  "morto": false,
  "mortes": 0,
  "lembretes_vencidos": [{ "id": 3, "texto": "gravar a publi" }]
}
```

## `POST /lembrete/{id}/feito` (etapa 4)

Marca o lembrete como feito (tira o cocô). Resposta: o mesmo JSON de `GET /estado`.
