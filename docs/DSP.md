# Cadeia espectral de 32 bandas — revisao 0.3.0

## Divergencias encontradas na 0.2.0

- A profundidade dependia principalmente do espectro normalizado. Dois sinais
  com o mesmo espectro e niveis diferentes podiam gerar cortes quase iguais.
- No modo M/S, o auxiliar tambem era dividido em Mid e Side. Uma voz mono no
  sidechain nao tinha componente Side, impedindo Side-only quando essa era a
  escolha de processamento do audio principal.
- Low-cut/High-cut e Pan podiam mudar os alvos de ganho de uma vez. Os coeficientes
  dos equalizadores eram substituidos a cada quatro amostras, sem interpolacao.
- A volta a 0 dB dependia somente do estado dos biquads e da matriz M/S; faltava
  um caminho seco exato ao final da recuperacao.
- Os tracos do display eram escalados junto com a imagem e ficavam com menos de
  um pixel na janela compacta. Faltavam valores de frequencia e sombreamento dos limites.

## Caminho de audio atual

`getBusBuffer(buffer, true, 1)` fornece o auxiliar mono/stereo e o JUCE resolve
o deslocamento correto dos canais conforme o layout. Somente
`getBusBuffer(buffer, false, 0)` recebe os resultados do processamento.
O auxiliar nao e somado ao principal; a unica excecao intencional e o modo Listen.

O detector recebe HPF/LPF controlados por Low-cut/High-cut e dois bancos de 32
passa-faixas de fase minima. As bandas possuem bordas logaritmicas entre 20 Hz e
20 kHz, com centros geometricos entre essas bordas. Centros proximos/acima de
Nyquist sao desativados conforme a taxa de amostragem.

Para cada banda, `max(abs(filteredL), abs(filteredR))` alimenta um unico envelope
Peak com Attack/Release. A extracao do valor absoluto ocorre depois do passa-faixa,
para preservar o espectro original do auxiliar durante a analise.

A lei de atenuacao base e `gain = 1 / (1 + 40 * envelope)`, limitada a 36 dB por
banda. Amount escala essa profundidade em dB de 0% a 100%. Um seletor relativo
suprime o vazamento distante dos passa-faixas, mas nao normaliza a profundidade.
Um auxiliar baixo gera menos reducao que um auxiliar alto com o mesmo espectro.
Bandas com centros fora de Low-cut/High-cut recebem alvo de 0 dB.

O audio principal passa por 32 equalizadores de pico, com atenuacao somente nas
bandas ativadas pelo detector. As transicoes de faixa e Pan sao suavizadas em
4 ms; o ganho recebe suavizacao adicional de 0,5 ms. Os coeficientes sao
interpolados amostra a amostra entre atualizacoes de controle de oito amostras.
Os coeficientes do HPF/LPF tambem possuem rampas quando suas frequencias mudam.

## Stereo e M/S

O espectro vinculado do auxiliar controla ambas as matrizes do audio principal:

`M = (L + R) / 2`, `S = (L - R) / 2`; reconstrucao: `L = M + S`, `R = M - S`.

Pan negativo favorece L no modo L/R e Mid no modo M/S; Pan positivo favorece R
ou Side. No centro, ambos recebem a mesma curva. Nos extremos, o componente
oposto recebe alvo de 0 dB. Um auxiliar mono pode controlar qualquer um deles.
As duas matrizes permanecem aquecidas e a alternancia e feita por crossfade.

## Transparencia e limites reais

Sem auxiliar, ou depois da recuperacao do envelope quando ele fica silencioso,
o processamento volta por crossfade a um caminho seco exato. Amount=0 tambem
retorna ao sinal seco depois das rampas. Freeze conserva intencionalmente o
envelope aprendido, mesmo quando o auxiliar para de tocar.

Fase minima tem resposta de fase durante a atenuacao; esta implementacao nao
promete fase zero. Filtros de banda finita possuem saias e sobreposicao: o alvo
de uma banda excluida e 0 dB, mas bandas vizinhas podem influenciar a regiao de
transicao. Nao ha corte de frequencia ideal/brickwall nem equivalencia comprovada
ao algoritmo proprietario do Trackspacer. A curva do visor mostra os alvos por
banda, nao a soma exata das respostas de todos os biquads em cascata.

O DSP nao usa FFT com janela, lookahead, alocacoes, mutex, arquivos ou rede no
callback. A latencia declarada e zero; Attack/Release, filtros e rampas possuem
tempos de resposta. Os testes interceptam alocacoes comuns e alinhadas no thread
de audio, tanto no motor quanto no processBlock com auxiliar.

## Display

Os valores Low-cut/High-cut sao lidos diretamente dos parametros atomicos e
atualizados pelo repaint de 30 Hz. Abaixo de 1000 Hz usa-se Hz; a partir de 1000 Hz,
kHz com uma casa decimal. Os mesmos parametros posicionam as areas translucidas
fora da faixa ativa. Os tracos mantem 2 e 2,5 pixels logicos, com juntas curvas e
pontas arredondadas, inclusive ao redimensionar. Os arquivos de imagem originais
permanecem inalterados.
