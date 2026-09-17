# Слой совместимости с RetroArch / Emustation — исследование

Ветка: `kiosk-retroarch` (база `kiosk`, 528edcb)
Дата: 2026-09-12

**Целевая платформа: Linux/aarch64 — игровая приставка (TV-box/STB) под
управлением EmulationStation.** На устройстве нет клавиатуры и мыши у
пользователя: единственный инструмент управления — геймпад. Отсюда две темы
исследования: слой интеграции с фронтендом/RetroArch и слой управления (HID).

## 1. Цель

Оценить, как интегрировать kiosk-сборку Fallout 1: Community Edition с
ретроиграми-фронтендами: **RetroArch** (движок/libretro) и **Emustation/
EmulationStation/ES-DE** (оболочки-лаунчеры) на **Linux/aarch64**, и предложить
архитектуру слоя совместимости.

Два направления интеграции:

- **Фронтенд → Fallout**: Fallout запускается фронтендом как обычная «игра»
  (по аналогии с эмулятором), и по выходу возвращает управление фронтенду.
- **Fallout → RetroArch**: изнутри Fallout (пункт меню, хоткей, пост-игровая
  точка) запускается RetroArch с конкретным ядром/контентом.

## 2. Что такое RetroArch и Emustation

### RetroArch (libretro)

- Открытый «тонкий» фронтенд, сам не эмулирует — грузит **libretro-ядра**
  (cores): `-L /path/to/core.so <игра>`.
- CLI-контракт (docs.libretro.com/guides/cli-intro):
  - `retroarch -L <core> game.rom` — запуск ядра с контентом;
  - `retroarch --config custom.cfg`, `--appendconfig` — переопределение конфигов;
  - `retroarch --menu` — открыть меню без контента (иначе закроется сразу);
  - `--verbose` — диагностика.
- Часть ядер не требует контента (`supports_no_game = "true"`, напр. ScummVM);
  для них путь к контенту в CLI можно не передавать.
- При выходе из загруженного контента (EXIT в меню / прерывание) процесс
  завершается с обычным кодом возврата → фронтенд получает управление обратно.

### Emustation = EmulationStation, ES-DE

**Emustation** — в нашем контексте это **EmulationStation** (RetroPie) и его
наследник **ES-DE (EmulationStation Desktop Edition)** — основной современный
фронтенд для Windows/Linux/Android. (Слово Emustation также носит клон
EmulationStation поверх XBMC для оригинальной Xbox — Xbox-клон к этому
репозиторию прямого отношения не имеет; в дальнейшем документе
«emustation»/«EmulationStation» означает сочетание RetroPie/ES-DE.)

EmulationStation — не эмулятор, а лаунчер: список «систем», у каждой — папка с
ромами и **команда запуска эмулятора**.
- Модель запуска (FAQ EmulationStation):
  `defaultemulaunch="retroarch --lib pocketsnes.so \"$1\""` — т.е. фронтенд
  выполняет shell-команду, подставляя путь к игре как `$1`; когда подпроцесс
  завершился, фронтенд снова показывает меню.
- Recalbox/ES поддерживают скрипты на события фронтенда (start/pre-start/
  write), MQTT — это точки, куда можно встроить любые команды, в т.ч. переключение
  окружения.

**Вывод по терминологии:** универсальный контракт у всех фронтендов семейства
EmulationStation/ES-DE один: **лаунчер выполняет команду, и по штатному
завершению возвращается к меню.**

## 3. Текущее состояние kiosk-ветки (факты из кода)

### Конфиги

- `kiosk.cfg` — `src/game/gkioskconf.cc`, глобальный `kiosk_config`.
  `gkioskconf_init()` вызывается **безусловно** из `game_init()` (game.cc:146).
  Отдельной детекции «это kiosk-сборка» нет — always-on; без файла работают
  дефолты (все `gconfig_*` = 0).
  Секции `[game]` (exp_start, disable_saveload, continues_play, game_exit и т.д.)
  и `[overrides]` (difficulty, language_filter — перезаписывают fallout.cfg).
- `kiosk_exec.cfg` — `src/plib/gnw/system_exec.cc`. Секция `[exec]`, ключи `0..7`
  (EXEC_MAX_LINES=8). Значения — произвольные shell-команды, документированы в
  README.md:243 (`0=touch /tmp/approach-apocalipse`).

### Механизм запуска внешних команд

- `system_exec(line_nums[])` (system_exec.cc:61) → для ключей, совпавших с
  line_nums, вызывает `compat_exec(cmd)`.
- `compat_exec` (src/platform_compat.cc:386) → `std::system(cmd)` в отделённом
  `std::thread` (thread-detach), всегда возвращает 0. Это **единственный**
  shell-out в кодовой базе.
- **На Android `compat_exec` эквивалентен голому `system()`**: кода для интентов
  и JNI нет (единственный `__ANDROID__` блок — winmain.cc:55, `chdir` на внешнее
  хранилище). Java-сторона (`os/android/app/.../MainActivity.java`) умеет
  запускать другую Activity (→ ImportActivity), но джусть паттерн GUI-интентов
  из native отсутствует.

### Точки, где сейчас вызывается system_exec

- `char_dump()` (src/game/chardump.cc:60-63) — **при каждом дампе персонажа**
  (смерть и явный выход). `char_dump_kiosk()` (chardump.cc:263) вызывается при
  смерти (continues_play) и при выходе (disable_saveload && !continues_play).

### Точки выхода из игры / меню

- Главное меню: `src/game/mainmenu.cc`, enum `MainMenuOption` (mainmenu.h:6-17),
  кнопки INTRO/NEW_GAME/LOAD_GAME/CREDITS/EXIT.
- Обработка EXIT — `gnw_main()` (main.cc:310-328):
  - `gconfig_game_exit_allowed > 0` → `done=true`, меню разрушается, дальше
    `main_exit_system()` + выход (return 0);
  - иначе — диалог-заглушка из kiosk.msg(#1218), игра остаётся в меню.
- Цикл экранов: `gnw_main()` (main.cc:182-342), switch по `mainMenuRc`
  (214-332) — естественная точка для нового пункта меню «Выйти в лаунчер».
- Игровой цикл: `main_game_loop()` (main.cc:435-476); смерть героя →
  `char_dump_kiosk(true)` (461-465) и выход.
- Явный выход из игры: `game_quit_with_confirm()` (game.cc:1139-1195).

### Сборка и платформы

- **Целевая**: Linux/aarch64 (STB-приставка) — главный и приоритетный сценарий
  для этого исследования. Сборка как обычный Linux-executable под aarch64.
- Desktop: Windows/Linux x86_64 executable (CMakeLists).
- Android: build как SHARED, Gradle `os/android` — для документа вторичен
  (на приставке не используется), упоминается как возможное расширение.
- iOS-порт есть (ios.toolchain.cmake), к теме не относится.

### Слой управления (HID) — статус в кодовой базе

**Геймпада в fallout1-ce сейчас НЕТ.** Факты:

- Пулл событий `GNW95_process_message()` (src/plib/gnw/input.cc:1092-1144)
  обрабатывает только: мышь (SDL_MOUSEMOTION/BUTTON/WHEEL), тач
  (SDL_FINGERDOWN/MOTION/UP → touch.cc) и клавиатуру (SDL_KEYDOWN/UP).
  **Событий SDL_CONTROLLER*/SDL_JOYSTICK* в switch нет вообще.**
- `src/plib/gnw/dxinput.cc` — только мышь (SDL relative mouse) и клавиатура,
  никакого джойстика.
- Java `SDLControllerManager` (os/android/.../org/libsdl/app/) — часть SDL-
  рантайма, только для Android, к Linux-приставке отношения не имеет.
- Т.е. SDL2 уже умеет геймпад (GameController API + gamecontrollerdb), но
  движок его не спрашивает: подключённый контроллер на приставке просто не даст
  ввода в игру.

**Пайплайн ввода сейчас:**
1. SDL scancode → `ascii_table` (src/plib/gnw/kb.cc, физическая раскладка).
2. `GNW95_key_map` (input.cc:1000-1071) — «размаскировка» QWERTY для не-QWERTY
   раскладок (важно для киоск-автоматов с нестандартной клавиатурой).
3. `get_key(current_screen, kb_getch())` (input.cc:232) — **переназначение
   клавиш** по `fallout_keys.cfg`.

**Переназначение клавиш — есть** (src/plib/gnw/input_rebind.cc):
- Файл `fallout_keys.cfg`, секции по экранам: `main`, `game`, `editor`,
  `inventory`, `pip` (BindSectionStrings, input_rebind.cc:21-25).
- Формат: `[секция]` ключ = физический scancode, значение = игровой код.
- `get_key(screen, physical)` → игровой код (input_rebind.cc:93-102);
  `get_physical_key()` — обратное преобразование (например, для help-экранов,
  screen_help.cc:309).
- Документировано в README.md:258. **Ребind работает только для клавиатуры** —
  это сканкоды, а не абстрактные игровые действия.

Ключевой вывод для приставки: управление в игру попадает ТОЛЬКО с клавиатуры;
геймпад нужно добавлять (см. раздел «Слой управления (HID)»).

## 4. Модель интеграции «Фронтенд → Fallout»

Fallout оформляется как «эмулятор» фронтенда:

1. В ES-DE/EmulationStation добавляется система (пример es_systems.cfg / ES-DE
   custom system) с командой запуска, напр.:
   `fallout-ce-kiosk --launcher-frontend=esde "$1"` (или просто путь к игре как
   $1 / игнорировать его).
2. Fallout при старте обрабатывает свой контракт argv/env (детекция фронтенда,
   профиль/сохранка от фронтенда).
3. По завершении (системный выход, `game_exit_allowed=1`) процесс делает
   **чистый return** — фронтенд автоматически получает управление обратно.
   На Linux это уже работает «из коробки».

Чего сейчас не хватает:

- **Контракта запуска**: нет ни обработки argv-флага типа `--launcher`, ни
  env-маркеров фронтенда, ни выбора профиля по «контенту».
- **Эксплицитного кода возврата** для фронтенда (exit code / маршрут
  «вернуться в ES-DE» вместо полного выхода из X-сессии).
- Обёртки `compat_exec` не нужна в этом сценарии (процесс просто стартует/умирает).

## 5. Модель интеграции «Fallout → RetroArch»

Управление уходит из игры во внешний процесс RetroArch:

1. Пользователь выбирает в меню (новый `MainMenuOption` в mainmenu /
   gnw_main, main.cc:214-332) «Игры»;
2. Вызывается новый `compat_launch` (расширение system_exec), который:
   - на **Linux/aarch64**: `fork/exec` (или существующий `std::system` в потоке)
     `retroarch -L <core> <контент>`; RetroArch открывает отдельное окно/экран; по
     закрытии управление вернулось в Fallout-меню;
   - на **Android** (вне целевой платформы, опционально): intent на RetroArch
     (package `com.retroarch`) через JNI, возврат через `onResume`/SDL reinit.
3. Параллельно используются штатные хуки `system_exec` (дамп персонажа,
   конец игры) для «после конца игры → запустить следующий контент» — уже есть
   механизм на десктопе, достаточно прописать строки в `kiosk_exec.cfg`.

## 6. Слой управления (HID)

### 6.1 Проблема

На приставке единственное устройство ввода — геймпад. Игра читает только
клавиатуру/мышь/тач (см. факты в разделе 3), поэтому **без доработки играть в
Fallout на приставке невозможно**. Лаунчер EmulationStation и RetroArch геймпад
понимают «из коробки» (у обоих свои автоконфиги), но сама игра — нет.

### 6.2 Как включить геймпад в fallout1-ce

SDL2 уже лежит в зависимостях движка (svga/input), API GameController доступен:
`SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt")`,
`SDL_GameControllerOpen()`, события `SDL_CONTROLLERDEVICEADDED/REMOVED`,
`SDL_CONTROLLERBUTTONDOWN/UP`, `SDL_CONTROLLERAXISMOTION` (для аналоговых).

Минимальный план:
1. В `GNW95_process_message()` (input.cc:1092) добавить ветки
   SDL_CONTROLLERDEVICEADDED/REMOVED и BUTTONDOWN/UP.
2. Обслуживать события отдельным модулем (например, `src/plib/gnw/gamepad.cc`):
   маппинг контроллеров на **игровые коды** (dpad → стрелки, кнопки
   на Enter/Esc/пробел/Action, Start → Esc меню, центр → KEY_HOME — это уже
   используется D-pad HUD, кейс KEY_HOME известен).
3. Фид события в тот же канал, что и клавиатура (`GNW95_process_key`,
   input.cc:1180), чтобы downstream (rebind, сцены, диалоги) остался без изменений.

### 6.3 Rebind как общий слой

Теперь rebind (fallout_keys.cfg, input_rebind.cc) — клавиатурный: маппинг
«scancode → игровой код» по экранам. Для геймпада нужно решить:
- либо ввести отдельную секцию/файл `fallout_gamepad.cfg` с тем же форматом
  (физический «код кнопки» → игровой код);
- либо поднять уровень абстракции: на вход rebind подавать **виртуальные коды
  действий** (game_ACTION_UP/LEFT/A/START…), а keyboard/gamepad — преобразовывать
  в эти коды каждый своим маппингом. Это позволит единым конфигом раскладки
  управлять и клавиатурой, и геймпадом, и тачем. Учесть: HUD/touch уже отдаёт
  клавиатурные коды (KEY_HOME и др.), поэтому абстрагирование к «действиям»
  аккуратно перекроет и его.
- Средний вариант: геймпад мапится сразу на игровые коды (те же, что клавиатура
  отдаёт через fallout_keys.cfg), без новой абстракции. Проще, но раскладка
  геймпада жёстко зашита в коде (или отдельным конфигом).

### 6.4 Взаимодействие с RetroArch/ES по вводу

- Пока управляет Fallout — ввод с геймпада принимает Fallout (через пункт 6.2).
- Когда внутри запускается RetroArch (сценарий A2) — управление полностью
  переходит RetroArch (у него собственные автоконфиги, конфликтов нет).
- При возврате в Fallout геймпад снова должен быть «взят» — SDL
  GameController держит контекст глобально, повторное подключение отработает.

### 6.5 Геймпад + киоск-экраны

- Меню и диалоги уже управляются клавиатурными кодами; после подачи геймпад-
  событий в `GNW95_process_key` киоск-экраны, скринсейвер, help и т.д. работают
  без изменений (это главный аргумент за фид на уровне `GNW95_process_key`).

## 7. Варианты архитектуры слоя

| Вариант | Направление | Суть | Сложность | Выгода |
|---|---|---|---|---|
| **A1** | Фронтенд→Fallout | Fallout как «эмулятор» фронтенда; argv/env-контракт; чистый return | Низкая (конфиг + argv) | Работает на ES-DE/EmulationStation/RetroPie с любым лаунчером |
| **A2** | Fallout→RetroArch (Linux/aarch64) | Пункт меню «Игры» → spawn RetroArch CLI; возврат по завершении | Средняя (процесс-менеджмент) | Единый «игровой центр» на приставке |
| **A3** | Fallout→RetroArch (Android) | Интенты + JNI, пауза/резюм SDL, возврат через onResume | Высокая (жизненный цикл Android) | Вне целевой платформы — по желанию |
| **H1** | HID-слой | Геймпад: SDL_GameController → `GNW95_process_key`; rebind на игровые коды | Средняя (input.cc + модуль) | Обязательно для приставки, без него A1/A2 бессмысленны |
| **B** | Fallout как libretro-core | Порт собственного движка в ядро libretro | Очень высокая, лицензионно спорно | Нецелесообразно; отбрасывается |

Примечание: H1 — сквозное требование: без геймпада Fallout на приставке
неуправляемый, поэтому H1 входит в любой рабочий сценарий (A1 и/или A2).

## 8. Рекомендуемый план внедрения

Этап 0 (HID-слой, H1):
- Добавить модуль геймпада (`src/plib/gnw/gamepad.cc`): SDL_GameController,
  загрузка gamecontrollerdb, фид DPAD/кнопок в `GNW95_process_key`.
- Решить формат rebind геймпада (отдельный конфиг против абстракции «действий»);
  рекомендуется первый вариант (подход «коды игровых кнопок = игровые коды») —
  минимальная инвазия.
- Проверить ключевые экраны: меню, походовый бой, диалоги, инвентарь — на
  отсутствие необходимости в клавиатуре (Fallout в целом keyboard-centric;
  любая упирающаяся в это экрана — отдельный вопрос для киоск-раскладки).

Этап 1 (фундамент слоя, кроссплатформенно):
- Ввести конфиг-ключи в `kiosk.cfg` (`gkioskconf`): `launcher_enabled`,
  `launcher_name` (например `esde` / `emustation` / `retroarch`),
  `launcher_return_on_exit` (0/1), `retroarch_core`, `retroarch_content`.
- Документировать контракт argv/env запуска Fallout-киоска (новые `main()
  argv` флаги в gnw_main): `--launcher=<name>`.

Этап 2 (приставка, «быстрая победа»):
- Детекция фронтенда при старте + гарантированный чистый return после
  `main_exit_system()` (main.cc:337-341) — управление возвращается в ES-DE.
- Добавить пункт «Игры» в главное меню, ведущий к `compat_launch` с командой
  из `kiosk.cfg` (обёртка над `std::system` в потоке уже есть: system_exec).
- Прогнать «после конца игры» через существующий `kiosk_exec.cfg`.

Этап 3 (Android, опционально):
- Реализовать native-вызов интента RetroArch через SDL/JNI (по образцу
  MainActivity→ImportActivity), пауза SDL перед `startActivity`.
- Настроить `onResume` для возвращения в игру без `System.exit(0)`
  (сейчас onDestroy принудительно закрывает процесс).

## 9. Риски и открытые вопросы

- **Геймпад и раскладка**: клавиши по умолчанию (стрелки/Enter/Esc/пробел)
  покрывают большинство игровых действий, но Fallout keyboard-centric; ряд
  интеракций (ввод имени персонажа, горячие клавиши оружия F1-F8) требует
  решения: экранная клавиатура / модальная раскладка / исключение из киоска.
  Нужен прогон геймплея с геймпада.
- **gamecontrollerdb**: для неизвестных геймпадов приставок зачастую нужен
  актуальный `gamecontrollerdb.txt` рядом с бинарником; автоопределение
  SDL может не сработать.
- **Графический стек**: на приставке (X11/KMS) запуск RetroArch поверх живого
  SDL-окна требует позаботиться о фокусе/перераздаче дисплея; проверить на
  целевой плате.
- **Выбор фронтенда-источника**: ES-DE/EmulationStation на Linux могут отличаться
  моделью запуска (переменные окружения, `~/.emulationstation/es_systems.cfg`),
  тестировать на конкретной сборке приставки.
- **Безопасность**: `kiosk_exec.cfg` выполняет произвольные команды — при
  расширении в слое не снижать барьеры (помечать новые ключи как
  «system executed»).
- **RetroArch как «следующий контент»**: маршрут «конец игры → автозапуск
  следующего рома» удобно держать на уровне `kiosk_exec.cfg`, без хардкода в C++.

## 10. Файлы, которых коснётся реализация

- `src/plib/gnw/input.cc` (+ `gamepad.cc`/`gamepad.h` — новые) — HID: пулл
  событий SDL_CONTROLLER*, фид в `GNW95_process_key`.
- `src/plib/gnw/input_rebind.cc/.h` — rebind для геймпада (секция/файл).
- `src/game/gkioskconf.cc/.h` — новые ключи конфига слоя.
- `src/game/main.cc` (gnw_main, main_game_loop) — argv-контракт, пункт меню,
  точки выхода.
- `src/game/mainmenu.cc/.h` — пункт «Игры»/«Лаунчер» (MainMenuOption).
- `src/plib/gnw/system_exec.cc` / `src/platform_compat.cc` — расширение
  `compat_exec`/новый `compat_launch` (spawn RetroArch CLI на Linux/aarch64).
- `README.md` — документация контракта, конфигов слоя и раскладки геймпада.

## 11. Приложение: Анализ PrBoom libretro core (libretro/libretro-prboom)

Клон: `/tmp/opencode/libretro-prboom`, основной файл: `libretro/libretro.c` (4229 строк).

### 11.1 Архитектура слоя совместимости

PrBoom — canonical reference для портирования полного игрового движка на libretro.
Единственный файл `libretro.c` содержит весь compat layer:

```
libretro/
├── libretro.c              # Основной compat layer (4229 строк)
├── libretro_sound.c        # Аудио-совместимость (1623 строки)
└── libretro_core_options.h # Core options API
```

### 11.2 Lifecycle callbacks

| Callback | Назначение | Ключевые детали |
|----------|------------|-----------------|
| `retro_set_environment` | Регистрация core options | `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT` (RGB565/888), `SET_CONTROLLER_INFO`, `SET_INPUT_DESCRIPTORS`, `SET_SUPPORT_NO_GAME` |
| `retro_init` | Минимальная инициализация | Только логирование, полная загрузка deferred до `retro_load_game` |
| `retro_get_system_info` | Метаданные core | `need_fullpath = true` (WAD файлы), `valid_extensions = "wad\|iwad\|pwad\|lmp\|m3u\|pk3\|ipk3\|zip"` |
| `retro_get_system_av_info` | Геометрия и тайминг | `base_width/height = SCREENWIDTH/SCREENHEIGHT` (320×200), `fps` = TICRATE (35) по умолчанию, динамическое через `movement_smooth` |
| `retro_load_game` | Загрузка контента | `need_fullpath=true`: WAD ищется через `I_FindFile`, `GET_SYSTEM_DIRECTORY` для prboom.wad, `GET_SAVE_DIRECTORY` для .dsg файлов. argv конструируется из core options (complevel, resolution). `screen_buf = calloc(...)` — framebuffer для рендеринга. |
| `retro_run` | Кадровый цикл | `update_variables()` при изменении опций → `D_DoomLoop()` (один тик игры) → `I_UpdateSound()` → `video_cb()` (отправка кадра) → rumble counter management |
| `retro_unload_game` | Освобождение ресурсов | `free(screen_buf)`, `free_load_argv()` |
| `retro_deinit` | Финальная очистка | `log_cb = NULL` |

### 11.3 Видео-путь (framebuffer → frontend)

```
I_StartDisplay()
  └─ screens[0].data = (from GET_CURRENT_SOFTWARE_FRAMEBUFFER или screen_buf)
      └─ Renderer пишет пиксели напрямую в framebuffer (direct rendering)

I_FinishUpdate()
  ├─ IF direct_fb_data != NULL (direct rendering):
  │   ├─ memcpy(screen_buf, direct_fb_data, SCREENPITCH*SCREENHEIGHT)  // snapshot для wipe
  │   └─ video_cb(direct_fb_data, SCREENWIDTH, SCREENHEIGHT, pitch)   // direct path
  └─ ELSE (fallback):
      └─ video_cb(screen_buf, SCREENWIDTH, SCREENHEIGHT, SURFACE_PIXEL_DEPTH*SCREENWIDTH)
```

**Ключевые оптимизации:**
- `GET_CURRENT_SOFTWARE_FRAMEBUFFER` — прямая запись в буфер frontend (без копирования)
- `direct_fb_data` — указатель на live framebuffer, snapshot в `screen_buf` только для wipe transitions
- Pixel format: RGB565 (по умолчанию) или XRGB8888, negotiated через `SET_PIXEL_FORMAT`
- Динамическое изменение разрешения через `SET_SYSTEM_AV_INFO` / `SET_GEOMETRY`

### 11.4 Ввод: gamepad → game keys

**Структура gamepad_layout_t** (строки 232-241):
```c
typedef struct {
    struct retro_input_descriptor desc[MAX_BUTTON_BINDS+4]; // для frontend remapping UI
    action_lut_t action_lut[MAX_BUTTON_BINDS];              // gamekey/menukey маппинг
    unsigned num_buttons;
} gamepad_layout_t;
```

**Два режима:**
- `RETROPAD_CLASSIC` — PS1-style (16 кнопок): D-pad = movement, B=strafe, A=use, X=fire, Y=run
- `RETROPAD_MODERN` — Xbox-style (16 кнопок + analog sticks): L2=run, R2=fire, analog sticks = movement/aim

**Process flow:**
```
process_input()
  ├─ FOR each port:
  │   ├─ input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK)
  │   └─ process_gamepad_buttons(ret, num_buttons, action_lut)
  │       └─ FOR each button: input_state_cb → action_lut[i].gamekey/menukey → D_PostEvent(ev_keydown/up)
  ├─ Analog sticks (Modern mode only):
  │   ├─ process_gamepad_left_analog()  → strafe/movement
  │   └─ process_gamepad_right_analog() → turn/look
  └─ Mouse (if enabled):
      ├─ RETRO_DEVICE_MOUSE → ev_mouse events
      └─ Mouse wheel → weapon cycle
```

**Ключевой паттерн для fallout1-ce:**
- `action_lut[i].gamekey` = указатель на engine key variable (`&key_fire`, `&key_use`)
- `action_lut[i].menukey` = тот же указатель для меню (переиспользует engine keys)
- `D_PostEvent()` — posting synthetic keyboard events (ev_keydown/ev_keyup) с data1 = engine key code

### 11.5 Save states (serialization)

```
retro_serialize_size()
  └─ sizeof(extra_serialize) + dynamic_size(nthinkers, numsectors, numlines)

retro_serialize(data_, size)
  ├─ G_DoSaveGameToBuffer(data_ + sizeof(*extra), size - sizeof(*extra))
  └─ extra struct: gametic, gameaction, turnheld, autorun, gamestate,
                   FinaleStage, FinaleCount, itemOn, whichSkull, currentMenu,
                   set_menu_itemon, menuactive, gamekeydown[], old_input[],
                   WI_Save(), music_state

retro_unserialize(data_, size)
  ├─ G_DoLoadGameFromBuffer(data_ + extra_size, size - extra_size)
  └─ Restore extra struct fields → engine state
```

**Для fallout1-ce:** существующая система `save_game`/`load_game` в `savegame.cc` уже
сериализует game state в буфер. Паттерн аналогичен: G_DoSaveGameToBuffer — это обёртка
над существующей сериализацией, а extra struct хранит дополнительные runtime-поля.

### 11.6 Файловая система и директории

| API call | Назначение | Путь по умолчанию |
|----------|------------|-------------------|
| `GET_SYSTEM_DIRECTORY` | Системные файлы (prboom.wad) | `~/.config/retroarch/system/` |
| `GET_SAVE_DIRECTORY` | Save files (.dsg) | `~/.config/retroarch/saves/` |
| `GET_CORE_ASSETS_DIRECTORY` | Контент (WAD) | `~/.config/retroarch/content/` |
| `GET_SAVE_DIRECTORY` + basename | Per-game saves | `{saves}/{content_name}/` |

**Для fallout1-ce:** перенаправить `savegame_dir` и `critter_dat` через эти API,
сохраняя обратную совместимость с `--config` путём через argv.

### 11.7 Core options (libretro_core_options.h)

PrBoom предоставляет ~20 core options через `libretro_set_core_options()`:
- `prboom-resolution` — внутреннее разрешение (restart-only)
- `prboom-complevel` — compatibility level
- `prboom-render` — software/OpenGL renderer
- `prboom-filter` — фильтрация текстур
- `prboom-mmap_wads` — memory-mapped I/O для WAD

**Паттерн для fallout1-ce:** аналогичные опции для `screen_size`, `gameDifficulty`,
`game_extra_difficulty`, `combat_speed`, `music`/`sound`/`voice` volume.

### 11.8 Аудио (libretro_sound.c)

- `audio_batch_cb` (int16) или `audio_batch_cb_float` (float) — batched audio output
- `I_UpdateSound()` → `audio_batch_cb(samples, frames)` — вызывается из `retro_run`
- Music через `I_MusicRegisterSerialise()` / `I_MusicUnregisterSerialise()` —葆存 music state в save states
- Sound frontend negotiates format через `GET_AUDIO_SAMPLE_BATCH_FLOAT`

**Для fallout1-ce:** существующий `sound_use[]` / `music_use[]` паттерн уже batched —
просто оборачивается в `audio_batch_cb` вызов.

### 11.9 Ключевые паттерны для переноса на fallout1-ce

1. **Single-file compat layer**: `libretro.c` = один файл, все callbacks + engine glue
2. **Direct framebuffer**: `GET_CURRENT_SOFTWARE_FRAMEBUFFER` → прямая запись → `video_cb`
3. **Gamepad via synthetic keyboard**: `input_state_cb` → `D_PostEvent(ev_keydown)` с engine keys
4. **Menu через те же keys**: `action_lut` с `menukey` = тот же pointer что и `gamekey`
5. **Save states**: `G_DoSaveGameToBuffer` + extra struct для runtime state
6. **File redirection**: `GET_SYSTEM_DIRECTORY` / `GET_SAVE_DIRECTORY` для перенаправления I/O
7. **Core options**: через `RETRO_ENVIRONMENT_SET_VARIABLES` + `GET_VARIABLE`

### 11.10 Сравнение с текущей архитектурой fallout1-ce

| Aspekt | PrBoom libretro | fallout1-ce (текущий) |
|--------|-----------------|----------------------|
| Render path | `screens[0].data` → `video_cb` | `gSdlSurface` → `renderPresent` |
| Input | `input_state_cb` → `D_PostEvent` | `SDL_PollEvent` → `GNW95_process_message` |
| Game loop | `D_DoomLoop()` в `retro_run` | `main_game_loop()` → `game_loop` |
| Config | Core options + `D_DoomMainSetup` | `config.cfg` + `fallout.cfg` |
| Save/load | `G_DoSaveGameToBuffer` + extra | `save_game`/`load_game` в `savegame.cc` |
| Main entry | `gnw_main` с argv | `gnw_main` с argc/argv |

**Вывод:** PrBoom показывает, что полный compat layer укладывается в ~4000 строк C.
Ключевые отличия для fallout1-ce:
- Need to handle `main_game_loop` iterative (not single-threaded like Doom)
- 640×480 vs 320×200 — framebuffer size negotiation differs
- Touch/mouse hybrid on Android (PrBoom не имеет touch)
- Existing `kiosk_exec` / `mainmenu` integration (PrBoom не имеет system menu)