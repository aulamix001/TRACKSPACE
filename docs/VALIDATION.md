# Validacao executada

Build: TRACKSPACE 0.1.0, Windows x64 VST3, JUCE 7.0.12
(`4f43011b96eb0636104cb3e433894cda98243626`).
Compilacao cruzada com GCC/MinGW-w64 14-posix, Release, runtimes estaticos.
Os testes Windows foram executados em Wine 10 / Xvfb sobre Linux, nao no Studio One.

## Resultados

`ctest --test-dir build-win7 --output-on-failure`: **3 de 3 suites passaram**.

| Suite | Evidencia |
|---|---|
| `spectral_audio_tests` | 36 verificacoes: ausencia de sidechain, Amount zero, frequencias coincidentes/distantes, regiao dos filtros, L/R, M/S, Freeze, Release, Bypass, escuta do sidechain, faixa invertida, valores nao finitos e independencia do tamanho do buffer |
| `host_processor_tests` | 16 verificacoes: buses auxiliares, processamento via entrada auxiliar, 11 parametros publicados, estado de automacao, tamanho e painel salvos, mono, estado invalido, renderizacao e interacao com o controle Amount e alternancia de painel |
| `vst3_binary_load_and_audio` | Abriu o arquivo VST3 Windows com LoadLibrary, chamou InitDll/GetPluginFactory, instanciou IComponent/IAudioProcessor, negociou buses stereo, processou 200 blocos de 512 amostras, verificou saidas finitas, reducao com sidechain, latencia declarada e encerramento |

O motor tambem passou nas 36 verificacoes compilado nativamente em Linux.
Uma compilacao auxiliar do processador com JUCE 8.0.6 em Linux passou nas 16
verificacoes e gerou imagens da interface. O artefato Windows entregue utiliza JUCE 7.0.12.

As quatro taxas testadas no motor foram 44100, 48000, 96000 e 192000 Hz.
Buffers de 1, 127, 257, 512 e 1024 amostras foram usados conforme o teste.

Os hashes SHA-256 dos dois arquivos de imagem originais coincidem com os
respectivos arquivos incorporados no plugin. Nenhum original foi editado.

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
