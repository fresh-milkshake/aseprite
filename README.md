<p align="left"><img src="data/icons/ase128.png" width="72" alt="Aseprite"></p>

# Aseprite Java Edition

Форк Aseprite для Windows с инструментами для текстур Minecraft Java Edition. На главной — миниатюры недавних файлов с закреплением и масштабированием. В редакторе — тёмная тема и встроенный русский интерфейс.

![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=for-the-badge)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Lua](https://img.shields.io/badge/Lua-2C2D72?style=for-the-badge&logo=lua&logoColor=white)

[Скачать для Windows](https://github.com/fresh-milkshake/aseprite/releases/latest)

![Recent Files](docs/screenshots/recent-files.png)

| Sprite Editor | Minecraft Menu |
|:---:|:---:|
| <img src="docs/screenshots/sprite-editor.png" width="460" alt="Sprite editor with the palette, layers, and timeline"> | <img src="docs/screenshots/minecraft-menu.png" width="460" alt="Minecraft menu showing export, preview, and texture commands"> |

## Запуск

Распакуйте ZIP и запустите `aseprite.exe` из распакованной папки. Русский язык включается в **Edit → Preferences → General → Language**. В меню **Minecraft** доступны быстрый экспорт PNG, работа с листами и предпросмотр текстур.

## Сборка

Нужны Visual Studio 2022 с инструментами C++, Git for Windows, CMake и Ninja.

```cmd
git clone --recursive https://github.com/fresh-milkshake/aseprite.git
cd aseprite
build.cmd --auto --norun
```

[Конфигурация сборки и проверок](.github/workflows/windows-release.yml)

## Автор и лицензия

Форк поддерживает [fresh-milkshake](https://github.com/fresh-milkshake). Aseprite создал David Capello, редактор поддерживают Igara Studio и контрибьюторы.

[EULA](EULA.txt) · [Лицензии компонентов и авторы перевода](docs/LICENSES.md)
