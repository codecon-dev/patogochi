<?php
declare(strict_types=1);

require_once __DIR__ . '/../../src/admin.php';
require_once __DIR__ . '/../../src/regras.php';
require_once __DIR__ . '/../../src/mqtt.php';

require_admin();
require_method('POST');

$body = read_json_body();
$acao = (string)($body['acao'] ?? '');

$pdo = db();

switch ($acao) {
    case 'matar':
        $pdo->exec("
            update pato
               set estado='morto', fome=0, estado_ate=null,
                   morto_desde=now() - interval '1 hour',
                   invulneravel_ate=null,
                   atualizado_em=now() - interval '2 hours',
                   versao=versao+1,
                   mortes=mortes+1
             where id=1
        ");
        $p = $pdo->query('select * from pato where id=1')->fetch();
        mqtt_pub_estado($p);
        json_out(['ok' => true, 'acao' => 'matar', 'pato' => $p]);

    case 'reviver':
        $hp = max(1, min(100, (int)($body['hp'] ?? 100)));
        $pdo->prepare("
            update pato
               set estado='parado', fome=:hp,
                   estado_ate=null, morto_desde=null,
                   invulneravel_ate=now() + make_interval(secs => :inv),
                   versao=versao+1, atualizado_em=now()
             where id=1
        ")->execute([
            'hp'  => $hp,
            'inv' => config_int('invulneravel_apos_reviver_s'),
        ]);
        $pdo->exec('update mortes set reviveu_em=now() where reviveu_em is null');
        $p = $pdo->query('select * from pato where id=1')->fetch();
        mqtt_pub_estado($p);
        json_out(['ok' => true, 'acao' => 'reviver', 'pato' => $p]);

    case 'setar_hp':
        $hp = max(0, min(100, (int)($body['hp'] ?? 100)));
        $pdo->prepare('update pato set fome=:hp, versao=versao+1, atualizado_em=now() where id=1')
            ->execute(['hp' => $hp]);
        $p = $pdo->query('select * from pato where id=1')->fetch();
        mqtt_pub_estado($p);
        json_out(['ok' => true, 'acao' => 'setar_hp', 'pato' => $p]);

    case 'zerar_cocos':
        $n = $pdo->exec('update lembretes set feito_em=now(), virou_coco=false where feito_em is null and virou_coco=true');
        mqtt_publish('patogochi/coco', ['id' => 0, 'total' => 0, 'limpeza' => true]);
        json_out(['ok' => true, 'acao' => 'zerar_cocos', 'cocos_limpos' => $n]);

    case 'delete_ideia':
        $id = (int)($body['ideia_id'] ?? 0);
        if ($id <= 0) json_err('ideia_id invalido', 400);
        $n = $pdo->prepare('delete from ideias where id=?')->execute([$id]);
        json_out(['ok' => true, 'acao' => 'delete_ideia', 'ideia_id' => $id]);

    case 'marcar_lembrete':
        $id = (int)($body['lembrete_id'] ?? 0);
        if ($id <= 0) json_err('lembrete_id invalido', 400);
        $pdo->prepare('update lembretes set feito_em=now(), virou_coco=false where id=?')
            ->execute([$id]);
        $total = (int) $pdo->query('select count(*) from lembretes where virou_coco=true and feito_em is null')->fetchColumn();
        mqtt_publish('patogochi/coco', ['id' => 0, 'total' => $total, 'removido' => $id]);
        json_out(['ok' => true, 'acao' => 'marcar_lembrete', 'lembrete_id' => $id]);

    default:
        json_err('acao invalida (matar|reviver|setar_hp|zerar_cocos|delete_ideia|marcar_lembrete)', 400);
}
