# WinPublicIP — план порта на C++

## Стек

**C++17 · Win32 API · GDI+ · WinHTTP · nlohmann/json · CMake + Ninja · MSVC**

---

## Структура проекта

```
WinPublicIP.cpp/
├── CMakeLists.txt
├── .gitignore
├── src/
│   ├── main.cpp                  # WinMain, message loop
│   ├── TrayApp.cpp/.h            # Shell_NotifyIcon, меню, таймер
│   ├── SettingsDialog.cpp/.h     # диалог настроек (Win32 Dialog)
│   ├── Config/
│   │   └── AppSettings.cpp/.h   # JSON load/save, реестр
│   ├── Services/
│   │   ├── IpProvider.cpp/.h    # WinHTTP
│   │   ├── GeoProvider.cpp/.h   # WinHTTP + кэш
│   │   └── VpnDetector.cpp/.h   # GetAdaptersAddresses
│   └── Rendering/
│       ├── FlagLoader.cpp/.h    # RT_RCDATA → GDI+ Bitmap
│       └── TrayRenderer.cpp/.h  # GDI+ compositing
├── resources/
│   ├── resources.rc             # манифест ресурсов
│   ├── app.manifest             # dpiAware, requireAdministrator=false
│   ├── Flags/                   # PNG флагов (те же что в C# проекте)
│   ├── shield_on.png
│   ├── shield_off.png
│   └── offline.png
└── third_party/
    └── nlohmann/
        └── json.hpp             # один файл, скачать с GitHub
```

---

## Этап 1 — Структура проекта и сборка

- Создать файловую структуру выше
- Написать `CMakeLists.txt`:
  - Цель `WIN32` (без консоли)
  - Линковка: `winhttp.lib`, `gdiplus.lib`, `iphlpapi.lib`, `shell32.lib`, `advapi32.lib`
  - Ресурсы через `target_sources`
- Написать `.gitignore` для C++/CMake/MSVC
- Скачать `nlohmann/json.hpp`

**Проверка:**
```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Этап 2 — `main.cpp` (точка входа)

- `WinMain` вместо `Main`
- Защита от второго экземпляра: `CreateMutex` + `ERROR_ALREADY_EXISTS`
- Инициализация GDI+: `GdiplusStartup` / `GdiplusShutdown`
- Создание невидимого окна-хоста для обработки сообщений трея
- Обработка `WM_TASKBARCREATED` — перерегистрация иконки при перезапуске Explorer
- Win32 message loop: `GetMessage` / `TranslateMessage` / `DispatchMessage`

---

## Этап 3 — `AppSettings`

| C# | C++ |
|---|---|
| `System.Text.Json` | `nlohmann::json` |
| `%APPDATA%` | `SHGetKnownFolderPath(FOLDERID_RoamingAppData)` |
| `Registry` | `RegOpenKeyEx` / `RegSetValueEx` / `RegDeleteValue` |

- Загрузка: `std::ifstream` → `nlohmann::json::parse` → поля структуры
- Сохранение: структура → `nlohmann::json::dump(2)` → `std::ofstream`
- Автозапуск: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

---

## Этап 4 — `IpProvider` и `GeoProvider`

Единая вспомогательная функция `HttpGet(url) → std::string` на WinHTTP:

```
WinHttpOpen → WinHttpConnect → WinHttpOpenRequest →
WinHttpSendRequest → WinHttpReceiveResponse →
WinHttpReadData → WinHttpCloseHandle
```

- Таймаут: `WinHttpSetOption(WINHTTP_OPTION_CONNECT_TIMEOUT, 5000)`
- Парсинг ответа: `nlohmann::json`
- Кэш геолокации: `std::unordered_map<std::string, GeoResult>`
- Fallback-цепочка провайдеров IP из настроек

---

## Этап 5 — `VpnDetector`

```cpp
GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, ...)
```

- Итерация по `IP_ADAPTER_ADDRESSES`
- Проверка `OperStatus == IfOperStatusUp`
- Сравнение `FriendlyName` и `Description` (широкие строки) с паттернами через `wcsstr`
- Паттерны берутся из `AppSettings`

---

## Этап 6 — `FlagLoader` и `TrayRenderer`

**Загрузка PNG из ресурсов:**
```cpp
FindResource → LoadResource → LockResource →
CreateStreamOnHGlobal → Gdiplus::Bitmap::FromStream
```

- Кэш: `std::unordered_map<std::string, Gdiplus::Bitmap*>`
- Fallback на `xx.png` при отсутствии флага страны

**Рендер иконки (32×32):**
```cpp
Gdiplus::Bitmap canvas(32, 32, PixelFormat32bppARGB)
Gdiplus::Graphics g(&canvas)
g.DrawImage(flag,   RectF(0,  0,  32, 32))  // флаг на весь холст
g.DrawImage(shield, RectF(16, 16, 16, 16))  // щит в правый нижний угол
canvas.GetHBITMAP → CreateIconIndirect → DestroyIcon(предыдущей)
```

---

## Этап 7 — `TrayApp`

| C# | C++ |
|---|---|
| `NotifyIcon` | `NOTIFYICONDATA` + `Shell_NotifyIcon(NIM_ADD/MODIFY/DELETE)` |
| `ContextMenuStrip` | `CreatePopupMenu` + `AppendMenu` + `TrackPopupMenu` |
| `System.Windows.Forms.Timer` | `SetTimer` / `WM_TIMER` |
| `Clipboard.SetText` | `OpenClipboard` + `SetClipboardData` + `CloseClipboard` |
| `ShowBalloonTip` | `NIIF_INFO` в `NOTIFYICONDATA` |

- HTTP-запросы выполняются в `std::thread` — нельзя блокировать UI-поток
- Результат передаётся обратно через `PostMessage(hwnd, WM_APP_REFRESH_DONE, ...)`

---

## Этап 8 — `SettingsDialog`

- Окно создаётся вручную через `CreateWindowEx` (без `.rc` template)
- Элементы: `STATIC` (лейбл), `EDIT` (поле ввода), `BUTTON` (OK / Отмена)
- `WM_COMMAND` — обработка нажатий OK / Отмена
- Центрирование на экране через `SetWindowPos`
- Валидация: интервал 10–3600 сек

---

## Этап 9 — Ресурсы (`resources.rc`)

```rc
IDI_APP         ICON    "resources/app.ico"
IDB_SHIELD_ON   RCDATA  "resources/shield_on.png"
IDB_SHIELD_OFF  RCDATA  "resources/shield_off.png"
IDB_OFFLINE     RCDATA  "resources/offline.png"
IDB_FLAG_AE     RCDATA  "resources/Flags/ae.png"
IDB_FLAG_RU     RCDATA  "resources/Flags/ru.png"
// ... все флаги
```

Флаги адресуются по строковому имени кода страны (`"flag_ru"`, `"flag_us"` и т.д.) через `FindResourceA`.

---

## Этап 10 — Финальная сборка и тест

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\WinPublicIP.exe
```

**Ожидаемый результат:** один `.exe` без зависимостей, ~500 КБ–2 МБ.

---

## Таблица соответствия Win32 API

| Назначение | C# | C++ API | Библиотека |
|---|---|---|---|
| Иконка в трее | `NotifyIcon` | `Shell_NotifyIcon` | shell32 |
| Контекстное меню | `ContextMenuStrip` | `CreatePopupMenu`, `TrackPopupMenu` | user32 |
| Таймер | `Forms.Timer` | `SetTimer`, `WM_TIMER` | user32 |
| Буфер обмена | `Clipboard.SetText` | `OpenClipboard`, `SetClipboardData` | user32 |
| HTTP | `HttpClient` | `WinHttpOpen`, `WinHttpSendRequest` | winhttp |
| Сетевые интерфейсы | `NetworkInterface` | `GetAdaptersAddresses` | iphlpapi |
| GDI рендер | `Graphics.DrawImage` | `Gdiplus::Graphics::DrawImage` | gdiplus |
| PNG из ресурсов | `EmbeddedResource` | `FindResource`, `LockResource` | kernel32 |
| JSON | `System.Text.Json` | `nlohmann::json` | (header-only) |
| Реестр | `Registry` | `RegOpenKeyEx`, `RegSetValueEx` | advapi32 |
| Путь %APPDATA% | `Environment.SpecialFolder` | `SHGetKnownFolderPath` | shell32 |

---

## Чеклист реализации

### Подготовка
- [ ] Создать структуру папок
- [ ] Написать `CMakeLists.txt`
- [ ] Написать `.gitignore`
- [ ] Скачать `third_party/nlohmann/json.hpp`
- [ ] Скопировать PNG-ресурсы из C# проекта
- [ ] Написать `resources.rc`

### Реализация
- [ ] `main.cpp` — WinMain, GDI+ init, mutex, message loop
- [ ] `Config/AppSettings` — JSON config, реестр
- [ ] `Services/IpProvider` — WinHTTP, fallback chain
- [ ] `Services/GeoProvider` — WinHTTP, кэш
- [ ] `Services/VpnDetector` — GetAdaptersAddresses
- [ ] `Rendering/FlagLoader` — PNG из RCDATA, кэш
- [ ] `Rendering/TrayRenderer` — GDI+ compositing
- [ ] `TrayApp` — Shell_NotifyIcon, меню, таймер, фоновый поток
- [ ] `SettingsDialog` — Win32 диалог

### Проверка
- [ ] `cmake --build` — 0 ошибок
- [ ] Запуск: иконка появляется в трее
- [ ] Tooltip показывает IP и страну
- [ ] Флаг страны отображается корректно
- [ ] VPN-статус определяется правильно
- [ ] Меню работает (обновить, копировать IP, настройки, выход)
- [ ] Диалог настроек открывается и сохраняет интервал
