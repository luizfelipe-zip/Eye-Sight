<p align="center"><img src="docs/icon.png" width="96" alt="Ícone do Eye Sight"></p>

<h1 align="center">Eye Sight</h1>

<p align="center">Textura de papel (grain) e luz quente sobre a tela do Windows, para ler com mais conforto.<br>
Fica só na bandeja, como o Twinkle Tray: sem janela na barra de tarefas, sem aparecer no Alt+Tab.</p>

<p align="center"><img src="docs/painel.png" width="360" alt="Painel do Eye Sight com o slider da textura, os cinco estilos e o slider da Luz quente"></p>

---

Inspirado no "Modo de leitura" dos celulares Poco/Xiaomi: uma camada de grão bem fino sobre a tela que quebra o brilho "plástico" do monitor e deixa a leitura parecida com papel, e uma luz quente opcional para a noite.

- **Só na bandeja.** Um clique no ícone abre um painel com a intensidade da textura, os estilos e a Luz quente.
- **Cobre tudo**, inclusive o menu Iniciar, a busca, a barra de tarefas e o cursor do mouse.
- **Luz quente** de 6500 K (cor normal) a 2000 K (âmbar), que substitui a Luz noturna do Windows sem fazer o grão piscar.
- **Invisível para o resto do sistema.** Não aparece na barra de tarefas, no Alt+Tab nem no Win+Tab, e cliques passam direto pela textura.
- **Cinco estilos finos**, do mais fino ao mais marcado.
- **Inicia com o Windows** direto na bandeja e lembra a última configuração.
- **Leve.** Um .exe de ~100 KB em C/Win32 puro, sem dependências. A textura é estática e composta pelo próprio Windows (DWM), então o custo de CPU/GPU é praticamente zero.
- Multi-monitor, DPI por monitor, tema claro/escuro e cor de destaque do Windows.

## Estilos

| | Estilo | Descrição |
|---|---|---|
| 1 | **Pó** | O mais fino de todos, quase imperceptível |
| 2 | **Névoa** | Finíssimo e suave |
| 3 | **Véu** | Muito fino, sem aglomerados |
| 4 | **Cetim** | Fino e uniforme |
| 5 | **Seda** (padrão) | O mais marcado, ótimo para ler por horas |

<p align="center"><img src="docs/textures.png" width="640" alt="Os cinco estilos em fundo claro e escuro, na intensidade máxima"></p>

Todos usam grão de 1 pixel físico. Os quatro mais finos usam ruído filtrado ("ruído azul", sem aglomerados), que o olho percebe como mais miúdo.

## Recomendado

> [!IMPORTANT]
> **Não use junto com a Luz noturna do Windows.** Deixe a Luz noturna desligada (Configurações → Sistema → Tela) e use a **Luz quente** do Eye Sight no lugar. A Luz noturna altera a tabela de cores da placa de vídeo, que passa a pontilhar os tons: num grão de 1 pixel isso aparece como piscadas e como mudanças de intensidade alguns segundos depois de trocar de janela.

> [!TIP]
> **Se a textura mudar de força entre páginas claras e escuras, desligue o DCR do monitor** (contraste dinâmico, às vezes chamado de *Dynamic Contrast*, *ACM* ou *Smart Contrast*, no menu do próprio monitor). Ele ajusta o brilho conforme o conteúdo e pode fazer o grão parecer mais forte ou mais fraco. Isso depende de cada monitor: em muitos não faz diferença.

## Instalação

Baixe na página de [Releases](../../releases):

- **`EyeSight-Instalador.zip`**: extraia e dê dois cliques em `Instalar.cmd` (pede administrador). O Eye Sight vai para `C:\Program Files\Eye Sight`, abre na bandeja e passa a iniciar com o Windows. A textura cobre tudo, inclusive o menu Iniciar, a busca e a barra de tarefas.
- **`EyeSight-Desinstalador.zip`**: para remover. Extraia e dê dois cliques em `Desinstalar.cmd` (pede administrador). Remove o programa, a assinatura local e a inicialização automática. O mesmo `Desinstalar.cmd` também vem dentro do instalador.

Na primeira execução o SmartScreen pode avisar que o arquivo não é reconhecido, porque ele não é assinado por uma autoridade comercial. Clique em **Mais informações → Executar assim mesmo**.

Requer Windows 10 ou 11 (64 bits). No Windows 11 o painel tem cantos arredondados e fundo acrílico.

## Uso

- **Clique** no ícone da bandeja: abre o painel. Se o ícone estiver escondido, arraste-o da setinha `^` para a barra.
- **Slider de cima**: intensidade da textura. Também funciona com a roda do mouse, as setas, Page Up/Down e Home/End.
- **Amostras** abaixo do slider (ou teclas **1** a **5**): troca o estilo.
- **Luz quente** (slider laranja): de 0 (cor normal) a 100 (âmbar); 50 fica perto de 3000 K. Também com a roda do mouse sobre ele ou **Shift + setas**.
- **Botão de energia** (ou **Espaço**): liga/desliga a textura lembrando a intensidade.
- **Engrenagem** ou **clique direito** no ícone: menu com *Estilo da textura*, *Iniciar com o Windows*, *Textura no cursor do mouse*, *Esconder a textura em prints* e *Sair*.
- **Esc** ou clicar fora: fecha o painel.

Configurações ficam em `HKCU\Software\EyeSight`; a inicialização automática, em `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

## Como funciona

Cada monitor recebe uma janela com:

- `WS_EX_TOOLWINDOW`: fica fora da barra de tarefas, do Alt+Tab e do Win+Tab;
- `WS_EX_LAYERED` com alpha por pixel: o DWM mistura a textura com a tela; o slider muda só o alpha global (`UpdateLayeredWindow`), então arrastar é instantâneo;
- `WS_EX_TRANSPARENT` + `WS_EX_NOACTIVATE`: cliques e teclado atravessam, nunca recebe foco;
- a propriedade `NonRudeHWND` e 1 px a menos de altura, para o Explorer não tratá-la como app em tela cheia (senão a barra de tarefas com ocultação automática deixa de subir);
- `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` (opcional), para a textura não sair em prints e gravações.

O menu Iniciar e a barra de tarefas ficam numa faixa de z-order acima de qualquer janela "sempre no topo". Só processos com `uiAccess="true"` (assinados por um certificado confiável e instalados em uma pasta protegida, como Arquivos de Programas) podem criar janelas na faixa `ZBID_UIACCESS`, acima deles. É isso que o instalador faz:

1. copia `EyeSight-uiaccess.exe` (manifesto com `uiAccess="true"`) para `C:\Program Files\Eye Sight\EyeSight.exe`;
2. cria um certificado autoassinado *Eye Sight (assinatura local)*, exclusivo daquele computador, e o adiciona como confiável;
3. assina o .exe e **apaga a chave privada** logo em seguida, para que o certificado não possa assinar mais nada.

`Desinstalar.cmd` remove o programa, o certificado e a inicialização automática.

### Cursor do mouse

O Windows desenha o cursor por cima de todas as janelas, até das que ficam acima do menu Iniciar, então nenhuma camada consegue cobri-lo. Por isso o Eye Sight troca os cursores do sistema (seta, texto, mão, redimensionar etc.) por cópias com o mesmo grão e a mesma intensidade da tela (`SetSystemCursor`), e refaz as cópias quando você muda o estilo, a intensidade ou o esquema/tamanho do ponteiro nas Configurações. Ao sair, desligar a textura ou desmarcar *Textura no cursor do mouse*, os cursores originais voltam (`SPI_SETCURSORS`). Os cursores animados (ocupado) e os que invertem a cor do fundo ficam como estão, assim como cursores próprios de alguns programas e jogos.

### Luz quente

A Luz quente usa o filtro de cor de tela cheia da Magnification API (`MagSetFullscreenColorEffect`, o mesmo recurso dos "Filtros de cor" de acessibilidade do Windows). É uma matriz que multiplica o vermelho, o verde e o azul na composição do DWM, com as cores de um corpo negro entre 6500 K e 2000 K (passos iguais em mireds). Ela não mexe na tabela de gama da placa de vídeo, como a Luz noturna faz, então o grão continua estável. Ao sair do Eye Sight as cores voltam ao normal. Por usar o mesmo recurso, não funciona junto com a Lupa ou os Filtros de cor do Windows.

## Compilar

Não há dependências além do compilador.

**Linux/macOS** (mingw-w64):

```sh
sudo apt install gcc-mingw-w64-x86-64   # Debian/Ubuntu
sh build.sh
```

**Windows** ([MSYS2](https://www.msys2.org/) com MinGW-w64, `gcc` e `windres` no PATH):

```bat
build.bat
```

A saída fica em `build/`: `EyeSight-Instalador/` e `EyeSight-Desinstalador/`. O GitHub Actions compila a cada push. Para publicar uma release, crie uma tag `v*` ou rode o workflow **Build** manualmente (aba Actions → Run workflow) informando a versão.

### Estrutura

```
src/
  main.c            bandeja, painel, camadas da textura, luz quente
  grain.h           geração das texturas e lista de estilos
  app-uiaccess.rc, app-uiaccess.manifest, app.ico
installer/          Instalar.cmd / install.ps1, Desinstalar.cmd / uninstall.ps1
tools/
  preview.c         gera prévia dos estilos (PPM) sem precisar do Windows
  make_icon.py      gera src/app.ico a partir dos pontos do ícone em main.c
```

Para criar um estilo novo, adicione uma linha em `PRESETS` em `src/grain.h`. O painel se ajusta sozinho ao número de estilos, e o resultado pode ser conferido com `tools/preview.c`.

## Licença

[MIT](LICENSE)
