<?php
declare(strict_types=1);

require_once __DIR__ . '/../src/http.php';
require_once __DIR__ . '/../src/regras.php';

require_method('GET');

json_out(estado_json());
