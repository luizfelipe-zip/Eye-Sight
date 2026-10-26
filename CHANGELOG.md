# Changelog

Todas as mudanças relevantes do projeto. O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o versionamento segue o [SemVer](https://semver.org/lang/pt-BR/).

## [1.2.0] - 2026-10-26

### Adicionado
- Textura no cursor do mouse: os cursores do sistema (seta, texto, mão, redimensionar e outros) ganham o mesmo grão e a mesma intensidade da tela, já que o Windows sempre desenha o cursor por cima de qualquer janela. Dá para desligar no menu, em *Textura no cursor do mouse*. Os cursores originais voltam ao sair do programa.

### Alterado
- Instalar e desinstalar agora pedem para o Eye Sight fechar sozinho antes de removê-lo, para ele devolver os cursores e as cores normais. O desinstalador também restaura os cursores, caso o programa tenha sido encerrado à força.
- A descrição de cada release no GitHub vem do changelog.

## [1.1.0] - 2026-10-26

### Adicionado
- Luz quente: segundo slider no painel que esquenta as cores da tela (de 6500 K a 2000 K). Usa o filtro de cor de tela cheia do Windows em vez da tabela de gama da placa de vídeo, então não faz o grão piscar como a Luz noturna. Shift + setas ou a roda do mouse sobre o slider também ajustam.

### Alterado
- A release traz só a versão completa (`EyeSight-Instalador.zip`, que cobre também o menu Iniciar e a barra) e um desinstalador separado (`EyeSight-Desinstalador.zip`). A versão portátil foi retirada.

### Corrigido
- O instalador grava a inicialização com o Windows; em alguns PCs o próprio programa instalado não conseguia gravá-la.
- Uma cópia solta do programa não toma mais o lugar da versão instalada na inicialização com o Windows.
- Instalar e desinstalar também fecham cópias de teste que estejam abertas.
- Abrir o Eye Sight de novo com ele já rodando volta a abrir o painel na versão instalada.

## [1.0.1] - 2026-10-26

### Corrigido
- Piscadas na textura: a camada só é reposicionada no topo quando outra janela realmente passa por cima (antes era a cada 2 segundos) e nunca na versão instalada.
- A textura não é mais recriada em avisos de mudança de tela que não alteram os monitores (taxa de atualização, HDR, alguns drivers).

## [1.0.0] - 2026-10-26

### Adicionado
- Ícone na bandeja com painel de intensidade (slider, roda do mouse e teclado).
- Cinco estilos de textura fina: Pó, Névoa, Véu, Cetim e Seda.
- Camada de textura por monitor, fora da barra de tarefas e do Alt+Tab, com cliques passando direto.
- Inicialização com o Windows e memória da última configuração.
- Opção de esconder a textura em prints e gravações.
- Compatibilidade com a barra de tarefas com ocultação automática.
- Instalador opcional (uiAccess com assinatura local) para a textura cobrir o menu Iniciar e a barra.
- Build automático e publicação de releases pelo GitHub Actions.

[1.2.0]: https://github.com/luizfelipe-zip/Eye-Sight/releases/tag/v1.2.0
[1.1.0]: https://github.com/luizfelipe-zip/Eye-Sight/releases/tag/v1.1.0
[1.0.1]: https://github.com/luizfelipe-zip/Eye-Sight/releases/tag/v1.0.1
[1.0.0]: https://github.com/luizfelipe-zip/Eye-Sight/releases/tag/v1.0.0
