#!/usr/bin/env bash
# Copia os sprites do repo pra public/sprites/ (servido pelo nginx).
# Rodar antes de `docker compose build` ou no deploy.
set -euo pipefail

cd "$(dirname "$0")"
mkdir -p public/sprites/comidas public/sprites/pato

# 5 comidas 16x16
cp ../assets/sprites/comidas/*.png public/sprites/comidas/

# frame do pato pro header da galeria publica
cp ../assets/sprites/empolgado/01.png public/sprites/pato/empolgado.png
cp ../assets/sprites/parado/01.png    public/sprites/pato/parado.png

echo "sprites sincronizados"
