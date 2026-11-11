<?php
declare(strict_types=1);

require_once __DIR__ . '/env.php';

/**
 * Classifica uma ideia usando o CLI `claude -p`.
 *
 * Retorna ['tipo' => ..., 'prazo' => ISO8601|null, 'qualidade' => 'boa'|'meh'].
 *
 * Tipos (cada um vira um sprite de comida 16x16 no Core2):
 *   pipoca            ideia de video / conteudo
 *   moeda_chocolate   ideia de negocio / dinheiro
 *   biscoito_sorte    lembrete/tarefa (com ou sem prazo explicito)
 *   minhoca           ideia sem pe nem cabeca (bizarra, non-sense, solta)
 *   comida_estragada  ideia ruim (cliche, obvia, preguicosa) -> pato passa mal
 *
 * Qualidade (so se aplica a pipoca e moeda_chocolate):
 *   boa  -> pato fica empolgado (fome +30)
 *   meh  -> pato fica debochando (fome +15, meio termo)
 */
function claude_classifica_ideia(string $texto): array {
    $tz = new DateTimeZone('America/Sao_Paulo');
    $hoje = (new DateTimeImmutable('now', $tz))->format('c');
    $fim_do_dia = (new DateTimeImmutable('today 23:59:59', $tz))->format('c');

    $prompt = <<<PROMPT
Voce classifica ideias de um dev em UMA das 5 categorias abaixo.
Responda APENAS com JSON puro, sem markdown, sem cercas, sem explicacao:
{"tipo":"...","prazo":null,"qualidade":"boa","titulo":null}

Categorias (tipo):
- pipoca           ideia de video/conteudo (gravar algo, publicar, roteiro, youtube, tiktok, live)
- moeda_chocolate  ideia de negocio (produto, startup, vender, cobrar, empresa)
- biscoito_sorte   LEMBRETE/TAREFA - o dev se auto-lembrando de fazer algo
- minhoca          ideia bizarra, sem pe nem cabeca, non-sense, solta (sem categoria clara)
- comida_estragada SO em casos obvios de cliche conhecido (ex: "uber de X", "ifood de Y", "tinder de Z", "airbnb de W")

Prioridade (ordem importa):
1. Tem PALAVRA-CHAVE de lembrete/tarefa? ("me lembra", "me lembre", "lembrar de", "lembrete", "anota", "anotar", "tenho que", "preciso", "nao esquecer", "nao posso esquecer", "tarefa", "pra fazer", "tenho uma tarefa", "adiciona uma tarefa") -> biscoito_sorte
2. Dev mencionou EXPLICITAMENTE que eh video/conteudo (palavras: "video", "youtube", "tiktok", "roteiro", "gravar", "publicar", "live")? -> pipoca
3. Dev mencionou EXPLICITAMENTE que eh negocio (palavras: "startup", "empresa", "produto", "vender", "cobrar", "monetizar")? -> moeda_chocolate
4. Eh cliche conhecido (lista acima)? -> comida_estragada
5. Bizarra/non-sense sem categoria clara? -> minhoca + qualidade "meh"
6. Senao, decida entre pipoca/moeda_chocolate + julgue qualidade:
   - "boa": ideia com potencial real, especifica
   - "meh": generica mas nao e cliche gritante

REGRA DO PRAZO (preencha o campo "prazo" ISO8601):
- Se biscoito_sorte COM prazo explicito (ex "sexta", "amanha 18h", "dia 20", "ate quarta"): calcule o prazo real a partir de hoje ($hoje)
- Se biscoito_sorte SEM prazo explicito: use "$fim_do_dia" (fim do dia atual)
- Outros tipos: prazo null

REGRA DO TITULO (so preencha quando for biscoito_sorte):
- Extraia a ESSENCIA da tarefa, limpo e curto (max 50 chars), imperativo
- Remova palavras de preenchimento: "me lembra de", "tenho que", "é", "hum", "pra eu", "o lembrete de", etc
- Remova referencias de tempo que ja estao no prazo
- Remova pontos finais

Exemplos:
  "me lembra de comprar pão amanha" -> {"titulo":"Comprar pão","prazo":"<amanha>"}
  "lembrete: enviar relatório pra Logiteque" -> {"titulo":"Enviar relatório pra Logiteque","prazo":"<fim do dia>"}
  "tenho que ligar pro cliente sexta-feira as 9h" -> {"titulo":"Ligar pro cliente","prazo":"<sexta 9h>"}
  "é a nota do lembrete para eu buscar ali mais tarde na quinta" -> {"titulo":"Buscar ali","prazo":"<quinta>"}

Outros tipos (pipoca, moeda_chocolate, minhoca, comida_estragada): titulo null (nao usamos).

REGRA DE OURO: Respeite a intencao do dev. "me lembra" / "lembrar" / "tarefa" / "tenho que" sao SEMPRE biscoito_sorte, mesmo se o conteudo for de video ou negocio.

Hoje eh $hoje.

IDEIA: $texto
PROMPT;

    $bin   = env('CLAUDE_BIN', 'claude');
    $model = env('CLAUDE_MODEL', 'claude-haiku-4-5');

    $cmd = sprintf(
        '%s -p %s --model %s --output-format text 2>&1',
        escapeshellcmd($bin),
        escapeshellarg($prompt),
        escapeshellarg($model)
    );

    $raw = trim(shell_exec($cmd) ?? '');

    if (preg_match('/\{.*\}/s', $raw, $m)) {
        $json = json_decode($m[0], true);
        if (is_array($json) && isset($json['tipo'])) {
            $tipo = $json['tipo'];
            $prazo = $json['prazo'] ?? null;
            // Safety net: se classificou como biscoito_sorte mas Claude esqueceu o prazo,
            // preenche com fim do dia pra garantir que vira lembrete
            if ($tipo === 'biscoito_sorte' && empty($prazo)) {
                $prazo = $fim_do_dia;
            }
            // minhoca sempre meh; comida_estragada e biscoito_sorte: qualidade nao se aplica
            $qualidade = match (true) {
                $tipo === 'minhoca'           => 'meh',
                $tipo === 'comida_estragada'  => 'boa',
                $tipo === 'biscoito_sorte'    => 'boa',
                default                        => ($json['qualidade'] ?? 'boa'),
            };
            return [
                'tipo'      => $tipo,
                'prazo'     => $prazo,
                'qualidade' => $qualidade === 'meh' ? 'meh' : 'boa',
                'titulo'    => isset($json['titulo']) && is_string($json['titulo']) && $json['titulo'] !== ''
                               ? trim($json['titulo'])
                               : null,
            ];
        }
    }

    // Fallback: trata como minhoca
    return ['tipo' => 'minhoca', 'prazo' => null, 'qualidade' => 'meh'];
}
