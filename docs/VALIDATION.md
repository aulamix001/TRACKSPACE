# Validacao executada — TRACKSPACE 0.3.0

Windows x64 VST3, JUCE 7.0.12 (`4f43011b96eb0636104cb3e433894cda98243626`),
GCC/MinGW-w64 14-posix Release, runtimes estaticos. Executado em Wine 10/Xvfb
sobre Linux. O Studio One Windows ainda nao foi testado diretamente.

## Resultados

`ctest --test-dir build-win7 --output-on-failure`: **3 de 3 suites passaram**.

| Suite | Evidencia |
|---|---|
| `spectral_audio_tests` | 56 verificacoes: transparencia, Amount zero, seletividade, bandas excluidas, L/R, Mid/Side, Freeze, Release, Bypass, monitoracao, valores extremos, independencia do buffer, sensibilidade ao nivel real, Side-only com auxiliar mono, ausencia de click em automacao e ausencia de alocacoes |
| `host_processor_tests` | 30 verificacoes: buses, auxiliar sem vazamento e somente leitura, zero alocacoes no callback, parametros, estado, mono, janela, formatacao Hz/kHz, atualizacao dos dois cantos por knobs e por automacao, renderizacao de ambos os paineis e tamanhos 360/420/800 |
| `vst3_binary_load_and_audio` | Carregou o VST3 real, instanciou factory/component/processor, confirmou auxiliar kAux/kDefaultActive, negociou buses stereo, processou 400 blocos de 512 amostras e verificou sidechain so em L, so em R e em oposicao de fase |

O motor tambem passou nas 56 verificacoes compilado nativamente em Linux.
Taxas testadas: 44100, 48000, 96000 e 192000 Hz. Foram usados buffers de
1, 127, 257, 512, 1024 e 1025 amostras, alem de uma leitura de burst de 24 amostras.
A recuperacao de silencio do auxiliar foi verificada por comparacao bit a bit
com o sinal seco, incluindo stereo M/S. A ausencia de alocacoes foi medida
interceptando `new/new[]`, comuns e alinhados, no thread do processamento.

## Auditoria e controle negativo

O relatorio [DSP.md](DSP.md) descreve divergencias anteriores, equacoes,
roteamento, suavizacao, comportamento dos limites e limites fisicos dos filtros.

Os novos testes tambem foram executados contra o motor 0.2.0 (commit `c08eda0`).
Ele falhou em seis casos: profundidade proporcional em sinais baixos, resposta
curta de ataque sob a nova lei de ganho, dependencias de nivel, Side-only com
auxiliar mono, automacao combinada sem click em DC e recuperacao bit a bit.
A 0.3.0 passou nos 56 casos. Os testes de parametros antigos continuam presentes;
as expectativas de profundidade foram ajustadas explicitamente para a nova lei
inversa de amplitude, em vez de conservar cortes normalizados profundos em sinais
muito baixos.

As renderizacoes do display foram inspecionadas nos paineis normal e avancado,
com 20 Hz/20,0 kHz, 1,2/5,4 kHz e 2,0/8,0 kHz. A legibilidade foi conferida nas
janelas de 360 e 800 pixels. Nenhuma das imagens originais foi editada; seus hashes
coincidem com os arquivos incorporados no plugin.

## Reproducao da compilacao cruzada

Com um MinGW-w64 POSIX x64 e um `juceaide` executavel no Linux ja instalados:

```sh
export TRACKSPACE_MINGW_ROOT=/caminho/para/prefixo/usr
cmake -S . -B build-win -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x64.cmake \
  -DJUCE_SOURCE_DIR=/caminho/para/JUCE-7.0.12 \
  -DTRACKSPACE_HOST_JUCEAIDE=/caminho/para/juceaide \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-Wa,-mbig-obj
cmake --build build-win --parallel 3
```

O `juceaide` 8.0.6 foi usado apenas como gerador de recursos e imagens binarios
nesta maquina; nao e vinculado ao plugin. Os recursos gerados foram compilados e
validados com o VST3 Windows. A compilacao normal em Windows usa o gerador do
proprio JUCE 7.0.12 e e fornecida em `scripts/Build-Windows.ps1`.

## Ainda nao verificado

- Abertura, roteamento de sidechain, Control Link, automacao e recall diretamente no Studio One Windows.
- Interfaces em todas as escalas de DPI e configuracoes de multiplos monitores do Windows.
- Equivalencia do algoritmo e das curvas ao produto proprietario de referencia.
- Compilacao Visual Studio / execucao do workflow GitHub Actions; estes estao preparados, mas nao foram executados nesta maquina.

Nao ha assinatura Authenticode neste build de desenvolvimento. Nao foram
implementados os sistemas comerciais de licenciamento ou autoatualizacao do produto de referencia.
