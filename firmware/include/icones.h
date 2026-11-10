// Ícones 12x12 pretos da barra de baixo (estilo tela de Tamagotchi).
// 'k' = preto, '.' = transparente.
#pragma once

static constexpr int ICONE_TAM = 12;

// checkbox preto com o check vazado
static const char* ICONE_TAREFAS[ICONE_TAM] = {
  "............",
  ".kkkkkkkkkk.",
  ".kkkkkkkkkk.",
  ".kkkkkkkk.k.",
  ".kkkkkkk..k.",
  ".k.kkkk..kk.",
  ".k..kk..kkk.",
  ".kk....kkkk.",
  ".kkk..kkkkk.",
  ".kkkkkkkkkk.",
  ".kkkkkkkkkk.",
  "............",
};

static const char* ICONE_CORACAO[ICONE_TAM] = {
  "............",
  ".kkk...kkk..",
  "kkkkk.kkkkk.",
  "kkkkkkkkkkk.",
  "kkkkkkkkkkk.",
  ".kkkkkkkkk..",
  "..kkkkkkk...",
  "...kkkkk....",
  "....kkk.....",
  ".....k......",
  "............",
  "............",
};

static const char* ICONE_MICROFONE[ICONE_TAM] = {
  "....kkkk....",
  "...kkkkkk...",
  "...kkkkkk...",
  "...kkkkkk...",
  "...kkkkkk...",
  ".k.kkkkkk.k.",
  ".k..kkkk..k.",
  "..k......k..",
  "...kkkkkk...",
  ".....kk.....",
  ".....kk.....",
  "...kkkkkk...",
};

// "<" de voltar
static const char* ICONE_VOLTAR[ICONE_TAM] = {
  "............",
  ".......kk...",
  "......kk....",
  ".....kk.....",
  "....kk......",
  "...kk.......",
  "...kk.......",
  "....kk......",
  ".....kk.....",
  "......kk....",
  ".......kk...",
  "............",
};
