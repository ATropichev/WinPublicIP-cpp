# WinPublicIP — C++/Win32

[English](#english) | [Русский](#russian)

---

<a id="english"></a>

## English

A lightweight Windows system tray application written in **C++17 / Win32 API** that displays your current public IP address, country flag, and VPN status — inspired by the [Public IP Address widget for KDE 6](https://github.com/Davide-sd/ip_address).

No .NET, no Electron, no runtime dependencies. Single native `.exe`, ~477 KB.

### Features

- **Country flag** — 60+ countries, embedded as resources
- **Public IP** — displayed in tooltip; copied to clipboard in one click
- **VPN indicator** — green shield (on) / grey (off)
- **Auto-detection of VPN interfaces** — TAP, tun, WireGuard, OpenVPN, Tailscale, ZeroTier, PPP, IKEv2
- **Configurable refresh interval** — Settings dialog (10–3600 sec)
- **Balloon notification** on IP change
- **Start with Windows** — optional autorun via registry
- **Single `.exe`, no dependencies** — no runtime installation required

### Requirements

- Windows 10 / 11 / Server 2019+, x64

### Installation

Download `WinPublicIP.exe` from [Releases](../../releases) and run it. The icon appears in the system tray immediately.

Optionally enable **Start with Windows** in the context menu.

### Build from source

**Requirements:** Visual Studio Build Tools 2022 (MSVC + Windows 11 SDK), CMake 3.20+, Ninja

```powershell
git clone https://github.com/ATropichev/WinPublicIP-cpp.git
cd WinPublicIP-cpp

# Configure (from VS Developer Command Prompt, or with vcvars64.bat)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Output: build\WinPublicIP.exe (~477 KB)
```

### Configuration

Settings are stored in `%APPDATA%\WinPublicIP\config.json`:

```json
{
  "refreshIntervalSeconds": 60,
  "vpnInterfacePatterns": ["TAP", "tun", "WireGuard", "OpenVPN", "Tailscale", "ZeroTier", "PPP", "WAN Miniport (IKEv2)"],
  "notifyOnIpChange": true,
  "startWithWindows": false
}
```

### VPN detection

The app scans active network interfaces by name and description. Add your VPN adapter name to `vpnInterfacePatterns` in `config.json` if it is not detected automatically.

### Services used

| Purpose | Service | Notes |
|---------|---------|-------|
| External IP | [ipify.org](https://www.ipify.org) | Primary; fallback: ifconfig.me, icanhazip.com |
| Geolocation | [ip-api.com](http://ip-api.com) | Free, 45 req/min |
| Flag images | [flagcdn.com](https://flagcdn.com) | MIT, based on flag-icons |

### Tech stack

| Component | Technology |
|-----------|-----------|
| Language | C++17 |
| UI / Tray | Win32 API (`Shell_NotifyIcon`) |
| Rendering | GDI+ |
| HTTP | WinHTTP |
| Network interfaces | `GetAdaptersAddresses` (iphlpapi) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) (header-only, MIT) |
| Build | CMake + Ninja + MSVC |

### Also available

A [C# / .NET 8 version](https://github.com/ATropichev/WinPublicIP) of the same application is also available.

### License

MIT — see [LICENSE](LICENSE).

---

<a id="russian"></a>

## Русский

Лёгкое приложение для системного трея Windows на **C++17 / Win32 API**: отображает публичный IP-адрес, флаг страны и статус VPN — по образцу плазмоида [Public IP Address widget for KDE 6](https://github.com/Davide-sd/ip_address).

Без .NET, без Electron, без зависимостей рантайма. Один нативный `.exe` ~477 КБ.

### Возможности

- **Флаг страны** — 60+ стран, встроены в исполняемый файл
- **Внешний IP** — в tooltip; копируется в буфер обмена одним кликом
- **Индикатор VPN** — зелёный щит (включён) / серый (выключен)
- **Авто-определение VPN-интерфейсов** — TAP, tun, WireGuard, OpenVPN, Tailscale, ZeroTier, PPP, IKEv2
- **Настраиваемый интервал обновления** — диалог настроек (10–3600 сек)
- **Уведомление** при смене IP-адреса
- **Автозапуск с Windows** — через реестр, включается в меню
- **Один `.exe` без зависимостей** — установка среды выполнения не требуется

### Системные требования

- Windows 10 / 11 / Server 2019+, x64

### Установка

Скачать `WinPublicIP.exe` из раздела [Releases](../../releases) и запустить. Иконка сразу появится в трее.

Опционально включить **Запускать с Windows** в контекстном меню.

### Сборка из исходников

**Требования:** Visual Studio Build Tools 2022 (MSVC + Windows 11 SDK), CMake 3.20+, Ninja

```powershell
git clone https://github.com/ATropichev/WinPublicIP-cpp.git
cd WinPublicIP-cpp

# Конфигурация (из Developer Command Prompt или с vcvars64.bat)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Сборка
cmake --build build

# Результат: build\WinPublicIP.exe (~477 КБ)
```

### Конфигурация

Настройки хранятся в `%APPDATA%\WinPublicIP\config.json`:

```json
{
  "refreshIntervalSeconds": 60,
  "vpnInterfacePatterns": ["TAP", "tun", "WireGuard", "OpenVPN", "Tailscale", "ZeroTier", "PPP", "WAN Miniport (IKEv2)"],
  "notifyOnIpChange": true,
  "startWithWindows": false
}
```

### Определение VPN

Приложение сканирует активные сетевые интерфейсы по имени и описанию. Если ваш VPN-адаптер не определяется — добавьте его имя в `vpnInterfacePatterns` в `config.json`.

### Используемые сервисы

| Назначение | Сервис | Примечания |
|-----------|--------|------------|
| Внешний IP | [ipify.org](https://www.ipify.org) | Основной; fallback: ifconfig.me, icanhazip.com |
| Геолокация | [ip-api.com](http://ip-api.com) | Бесплатно, 45 запросов/мин |
| Иконки флагов | [flagcdn.com](https://flagcdn.com) | MIT, основан на flag-icons |

### Стек технологий

| Компонент | Технология |
|-----------|-----------|
| Язык | C++17 |
| UI / Трей | Win32 API (`Shell_NotifyIcon`) |
| Рендеринг | GDI+ |
| HTTP | WinHTTP |
| Сетевые интерфейсы | `GetAdaptersAddresses` (iphlpapi) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) (header-only, MIT) |
| Сборка | CMake + Ninja + MSVC |

### Также доступна

[C# / .NET 8 версия](https://github.com/ATropichev/WinPublicIP) того же приложения.

### Лицензия

MIT — см. [LICENSE](LICENSE).
