<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/regras.php';
require_once __DIR__ . '/../src/mqtt.php';

require_method('POST');
require_api_token();

$body = read_json_body();
$id = (int) ($body['lembrete_id'] ?? 0);
if ($id <= 0) json_err('lembrete_id inválido', 400);

$stmt = db()->prepare('
    update lembretes
       set feito_em = now(), virou_coco = false
     where id = :id and feito_em is null
     returning id
');
$stmt->execute(['id' => $id]);
$ok = (bool) $stmt->fetchColumn();

if (!$ok) json_err('lembrete não encontrado ou já feito', 404);

// Estado completo (igual /estado.php) + metadados do que foi marcado
$estado = estado_json();
mqtt_publish('patogochi/coco', ['id' => 0, 'total' => $estado['coco'], 'removido' => $id]);

json_out($estado + ['ok' => true, 'lembrete_marcado' => $id]);
