# TRACKSPACE

Efeito VST3 x64 para Windows, com reducao espectral por sidechain em 32 bandas,
automacao de parametros, processamento L/R e M/S, freeze e interface redimensionavel.
Usa as duas imagens aprovadas, preservadas em `Assets/normal.png` e `Assets/advanced.png`.

**Uso no Studio One:** [guia de instalacao e roteamento](docs/Studio-One.md).

## Compilar no Windows

Requisitos: Windows 10/11 x64, Git, CMake 3.22+, Visual Studio 2022 com
**Desktop development with C++** e Windows SDK. O CMake baixa JUCE 7.0.12 no commit
`4f43011b96eb0636104cb3e433894cda98243626`.

No PowerShell, a partir do repositorio:

```powershell
./scripts/Build-Windows.ps1
```

O script compila, executa testes e gera `dist/TRACKSPACE-Windows-x64.zip`.
Tambem existe um workflow GitHub Actions para Windows; ele so executa depois
que o codigo for enviado ao GitHub ou o workflow for acionado.

## Testes do motor sem dependencias de GUI

```sh
cmake -S . -B build-tests -DTRACKSPACE_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel 4
ctest --test-dir build-tests --output-on-failure
```

Os testes verificam audio de verdade: ausencia de sidechain, Amount zero, selecao
espectral, filtros, stereo, M/S, freeze, release, monitoracao, bypass, valores extremos,
taxas de 44,1–192 kHz e independencia do tamanho do buffer. O teste do processador
JUCE verifica buses auxiliares, automacao e restauracao do estado da sessao.

## Arquitetura

- `Source/SpectralEngine.*`: DSP C++17 independente, sem alocacao ou mutex no callback.
- `Source/PluginProcessor.*`: buses VST3, parametros estaveis e estado da sessao.
- `Source/PluginEditor.*`: imagens incorporadas, botoes funcionais e redimensionamento.
- `Tests/`: sinais sinteticos e integracao com o processador.

Processamento em fase minima sem lookahead; latencia declarada de zero amostras.
O detector possui tempos de resposta e os filtros possuem deslocamento de fase.
O algoritmo e uma implementacao independente, sem promessa de equivalencia exata ao original.

JUCE 7 e distribuido sob GPLv3 ou licenca comercial; seus termos se aplicam ao uso e
distribuicao do framework. O SDK VST3 incluido no JUCE tem seus proprios avisos.
Veja as licencas das dependencias antes de distribuir um produto.
