FROM php:8.3-fpm-alpine

# ---- base + extensoes PHP + claude CLI ----
RUN apk add --no-cache \
      nginx \
      supervisor \
      postgresql-client \
      postgresql-dev \
      curl \
      bash \
      ffmpeg \
      nodejs \
      npm \
  && docker-php-ext-install pdo_pgsql \
  && npm install -g @anthropic-ai/claude-code \
  && apk del postgresql-dev \
  && ln -sf /usr/local/bin/claude /usr/bin/claude || true

# ---- whisper.cpp (compila + baixa modelo base pt-BR) ----
RUN apk add --no-cache --virtual .whisper-build \
      git build-base cmake \
  && git clone --depth 1 --branch v1.7.4 https://github.com/ggerganov/whisper.cpp /tmp/whisper.cpp \
  && cd /tmp/whisper.cpp \
  && cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DGGML_OPENBLAS=OFF \
        -DWHISPER_BUILD_SERVER=ON \
  && cmake --build build -j --config Release --target whisper-cli whisper-server \
  && cp build/bin/whisper-cli    /usr/local/bin/whisper-cli \
  && cp build/bin/whisper-server /usr/local/bin/whisper-server \
  && mkdir -p /opt/whisper/models \
  && curl -L -o /opt/whisper/models/ggml-base-q5_1.bin \
       https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin \
  && rm -rf /tmp/whisper.cpp \
  && apk del .whisper-build

COPY nginx.conf /etc/nginx/nginx.conf
COPY supervisord.conf /etc/supervisord.conf

# Upload maior pra receber WAV de até 15s (~480KB bruto, fator x2 pela base64 em multipart)
RUN { \
    echo 'upload_max_filesize = 10M'; \
    echo 'post_max_size = 10M'; \
    echo 'memory_limit = 128M'; \
    echo 'max_execution_time = 120'; \
  } > /usr/local/etc/php/conf.d/patogochi-upload.ini

WORKDIR /var/www/html
COPY public /var/www/html/public
COPY src    /var/www/html/src
COPY config /var/www/html/config

# php-fpm worker roda como www-data; claude CLI procura sessao em $HOME/.claude.
# Setamos HOME=/var/www pra sessao ficar em /var/www/.claude (volume montado).
RUN chown -R www-data:www-data /var/www/html \
 && mkdir -p /var/www/.claude \
 && ln -sf /var/www/.claude/.claude.json.managed /var/www/.claude.json \
 && chown -R www-data:www-data /var/www \
 && echo 'env[HOME] = /var/www' >> /usr/local/etc/php-fpm.d/zz-docker.conf

EXPOSE 8080
CMD ["supervisord", "-c", "/etc/supervisord.conf", "-n"]
