# WerewolfHotkey (SKSE, Skyrim AE 1.6.1170)

Нажми K (настраивается) - превратиться в оборотня. Повторное нажатие - вернуться (если задан RevertSpell).

## Требования в игре
- SKSE64 2.2.6+
- Address Library for SKSE Plugins (All in one, AE)

## Сборка через GitHub (ничего не нужно ставить)
1. Создай пустой репозиторий на github.com и залей туда содержимое этой папки.
2. Вкладка **Actions** -> workflow **Build** -> дождись зелёной галочки (или Run workflow).
3. В запуске внизу, в разделе **Artifacts**, скачай `WerewolfHotkey`.
4. Внутри: `Data/SKSE/Plugins/WerewolfHotkey.dll` и `.ini` - скопируй папку Data в игру (или установи как мод в MO2/Vortex).

## Сборка локально
1. Visual Studio 2022 (или Build Tools) с компонентом "Desktop development with C++" + xmake (https://xmake.io).
2. В папке проекта:
   xmake f -m release -y
   xmake
3. DLL: build/windows/x64/release/WerewolfHotkey.dll -> Data/SKSE/Plugins/

## Настройка
Data/SKSE/Plugins/WerewolfHotkey.ini - клавиша, FormID'ы, переключение обратно.
Лог: Documents/My Games/Skyrim Special Edition/SKSE/WerewolfHotkey.log

## ВАЖНО: проверь FormID в SSEEdit
TransformSpell, WerewolfRace и RevertSpell прописаны по памяти и не проверены. Открой Skyrim.esm
в SSEEdit, найди нужные записи (Spell -> WerewolfChange / Beast Form, Race -> WerewolfBeastRace),
скопируй FormID без первых двух цифр (load order) в ini.
