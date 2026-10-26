# Changelog

Todas as mudanças relevantes do projeto. O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o versionamento segue o [SemVer](https://semver.org/lang/pt-BR/).

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

[1.0.1]: https://github.com/luizfelipe-zip/Eye-Sight/releases/tag/v1.0.1
[1.0.0]: https://github.com/luizfelipe-zip/Eye-Sight/releases/tag/v1.0.0
