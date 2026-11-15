# TRACKSPACE — Studio One / Windows 64 bits

## Instalar

1. Feche o Studio One.
2. Extraia o ZIP e copie a pasta inteira `TRACKSPACE.vst3` para
   `C:\Program Files\Common Files\VST3\`. Preserve `Contents\x86_64-win` dentro dela.
3. Abra o Studio One e atualize a lista de plugins. Procure **TRACKSPACE** em efeitos.
4. Se o Studio One tiver bloqueado uma versao anterior, remova-a da lista de bloqueados e refaca a verificacao.

## Conectar o sidechain

1. Insira TRACKSPACE na pista que deve ceder espaco, por exemplo, o instrumental.
2. Ative o botao **Sidechain** no cabecalho do efeito do Studio One.
3. Na pista que deve ter prioridade, por exemplo, a voz, crie um envio para
   **Sidechain → TRACKSPACE** da pista de destino. As palavras podem variar com a versao/idioma da DAW.
4. Reproduza ambas as pistas e ajuste **Amount**. O grafico azul mostra a deteccao e a reducao.
5. Limite a regiao com **Low-cut** e **High-cut**. Para voz, experimente iniciar em
   150 Hz–8 kHz, Amount 20–35%, Attack 10 ms e Release 100 ms; ajuste ouvindo.

Os parametros ficam expostos automaticamente ao sistema de automacao VST3.
O plugin declara a entrada auxiliar para a DAW, mas o envio de uma pista para outra
precisa ser conectado no Studio One: um efeito VST3 nao pode escolher arbitrariamente pistas da sessao.
O envio MIDI para controladores fisicos e gerenciado pelo Control Link da DAW.

## Controles

| Controle | Funcao |
|---|---|
| Amount | Intensidade de reducao, 0–100%; 0% preserva o sinal |
| Low-cut / High-cut | Regiao de reducao e HPF/LPF de 12 dB/oitava no sinal auxiliar antes do detector; 20 Hz–20 kHz |
| Freeze | Mantem o espectro aprendido; desligue para acompanhar novamente |
| Pequeno circulo azul do visor | Alterna entre painel normal e avancado |
| Pan | Balanco de processamento: esquerda/direita ou mid/side |
| L/R / M/S | Stereo com detector vinculado ou processamento Mid/Side |
| Attack / Release | Resposta do detector: 0,001–100 ms / 0,01–1000 ms; ataque padrao 2 ms |
| Sidechain no painel avancado | Escuta do sinal auxiliar dentro da faixa selecionada |
| Logo inferior | Menu de tamanhos, bypass e escuta Delta (sinal removido) |
| Canto inferior direito | Arraste para redimensionar de 360 a 800 pixels logicos |

A janela inicia em **500 × 500**, preserva a proporcao quadrada e salva o tamanho
e o painel no projeto. O Windows pode ampliar pixels fisicos conforme o DPI da tela.
Arraste os botoes, use a roda do mouse ou de duplo clique para restaurar o padrao.
A DAW pode mostrar/editar os valores numericos pela lista de parametros.

Sem sidechain conectado, o audio principal passa sem reducao. Sidechain conectado mas
sem sinal faz a reducao desaparecer conforme Release. Freeze pode manter uma curva
mesmo sem sinal. Low-cut maior que High-cut deixa a faixa de processamento vazia.
Os filtros acima do limite de Nyquist da taxa de amostragem ficam desativados.

Na versao 0.3.0, o barramento auxiliar e habilitado por padrao. O detector
usa `max(abs(L), abs(R))` por banda antes de suavizar o envelope. Esse mesmo
espectro controla o processamento L/R ou M/S do principal: um auxiliar mono
pode controlar Mid-only ou Side-only. Pan -100% seleciona L/Mid e +100% R/Side.

O HPF/LPF processa somente o auxiliar. Nos extremos 20 Hz/20 kHz, o filtro
auxiliar correspondente e ignorado. O nivel real de cada envelope determina a
profundidade, escalada por Amount, com teto de 36 dB por banda. Bandas fora da
faixa recebem alvo de 0 dB; os filtros possuem transicoes e saias naturais.
Ganhos, limites e Pan sao suavizados, com interpolacao dos coeficientes entre
atualizacoes a cada oito amostras. Ao terminar o Release, o plugin retorna ao
caminho seco exato, exceto quando Freeze mantem intencionalmente a curva.

Os cantos superiores do visor mostram os valores Low-cut/High-cut. Abaixo de
1000 Hz usa-se Hz; a partir de 1000 Hz, kHz com uma casa decimal. As areas
translucidas representam a regiao excluida pelos mesmos parametros. Os tracos
possuem juntas/pontas arredondadas e 2/2,5 pixels de espessura, mesmo na janela
compacta. Os valores respondem a knobs e automacao da DAW.

Attack/Release, passa-faixas e rampas possuem tempos de resposta. Fase minima
nao e fase zero durante a atenuacao. Esta revisao produz profundidades diferentes
da 0.2.0; ajuste Amount ouvindo ao abrir sessoes anteriores.

## Estado desta implementacao

Este e um processador independente: 32 bandas logaritmicas com detectores passa-faixa
e equalizadores de pico dinamicos em fase minima. Nao ha acesso ao codigo ou ao
algoritmo proprietario do Trackspacer para equivalencia de audio bit a bit.
As imagens fornecidas foram mantidas sem edicao; os controles e curvas sao desenhados
por cima das copias incorporadas. Esta compilacao se identifica na DAW como TRACKSPACE.
Ativacao, serial, atualizador e links comerciais do produto de referencia nao fazem
parte do processamento implementado.

O pacote Windows deve ser validado no Studio One antes de uso em sessoes importantes.
Consulte `docs/VALIDATION.md` no repositorio para os testes realmente executados.
