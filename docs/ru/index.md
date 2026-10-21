# Zenith Gaming Daemon - Документация (Русский)

**Zenith** - это высокопроизводительный, безопасный системный демон на C++20 для ультранизкой задержки и устранения микрофризов в играх под Linux (Arch Linux, CachyOS, SteamOS, Fedora, Ubuntu/Debian).

---

## 🎯 Назначение и Архитектура

Современный гейминг на Linux через Proton, Wine и DXVK обеспечивает высокий средний FPS, однако часто страдает от **неравномерности времени кадра (frametime jitter) и просадок 0.1% и 1% редких событий**:
1. **Split-Lock Throttling**: По умолчанию ядро Linux замедляет процессы с невыровненными атомарными инструкциями (характерно для DirectX 9-11 и движков Unreal Engine в Wine) на 10-100 миллисекунд.
2. **Скачки межъядерной задержки (L3 Cache Bouncing)**: На многочиплетных процессорах (AMD Ryzen 7900X3D / 7950X3D) или гетерогенных архитектурах (Intel 12-14 поколений и Core Ultra) планировщик перебрасывает игровые потоки между CCD или на медленные E-ядра, сбрасывая кэш L3.
3. **Фоновые помехи**: Браузеры, Discord, индексация и клиенты загрузки перехватывают вычислительные кванты у потоков рендера.
4. **Сброс частот GPU**: Драйверы графики снижают частоты памяти (VRAM) в меню или статичных сценах, вызывая задержку при возобновлении динамичной игры.

Zenith динамически и безопасно устраняет эти узкие места **без вмешательства в адресное пространство процессов игры**.

---

## ⚡ Что делает / Чего НЕ делает

### Что делает:
* **Событийный детект через Netlink proc connector**: Мгновенно (<50 мс) распознает запуск игр без необходимости правки параметров запуска Steam (`%command%`).
* **Привязка к 3D V-Cache (CCD0) и Intel P-ядрам**: Направляет игровые потоки на кэш 96 МБ или производительные ядра; фоновые процессы изолирует в срез `cgroups v2` на ядре 0.
* **Снятие задержек ядра**: Отключает split-lock mitigation во время игры, переводит THP в безопасный режим `madvise`, останавливает фоновую дефрагментацию памяти (`compaction_proactiveness=0`).
* **Фиксация частот GPU**: Предотвращает задержки переключения DPM / PowerMizer.
* **Звук реального времени**: Настраивает квант PipeWire на 64 семпла (1.33 мс) с приоритетом `SCHED_FIFO`.
* **Гарантия полного отката (Zero-Drift)**: Все параметры записываются в журнал Write-Ahead Log (`/run/zenith/journal.json`). При выходе из игры или крахе системы параметры восстанавливаются на 100%.

### Чего НЕ делает:
* Не удваивает средний FPS в играх, где видеокарта уже загружена на 100%.
* Не изменяет неизменяемые системные файлы; не оставляет «мусора» в sysctl после завершения игры.
* Не требует отключения безопасности ядра (SELinux/AppArmor) и не отключает `mitigations=off`.

---

## 🛡️ Безопасность и Античиты

**Zenith на 100% безопасен для любых античитов (Easy Anti-Cheat, BattlEye, Ricochet, VAC, Vanguard):**
* Zenith **никогда не внедряется** в память игрового процесса (никаких DLL/SO инъекций).
* Zenith **не перехватывает системные вызовы** (`ptrace`, LD_PRELOAD не используются).
* Взаимодействие происходит исключительно через стандартные интерфейсы ядра Linux (`sched_setaffinity`, `cgroups v2`, `setpriority`). Для античита Zenith неотличим от системного планировщика OS.

---

## 🚀 Быстрая установка

### Arch Linux / CachyOS (AUR)
```bash
paru -S zenith-gaming-daemon
# или
yay -S zenith-gaming-daemon
```

### Debian / Ubuntu (`.deb`)
```bash
wget https://github.com/NEGG0/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.2.0-Linux.deb
sudo dpkg -i zenith-gaming-daemon-1.2.0-Linux.deb
```

### Fedora / RHEL / Bazzite (`.rpm`)
```bash
sudo dnf install https://github.com/NEGG0/zenith-gaming-daemon/releases/download/v1.1.0/zenith-gaming-daemon-1.2.0-Linux.rpm
```

### NixOS (`flake.nix`)
```nix
services.zenith-gaming-daemon.enable = true;
```

---

## 🎮 Управление через `zenithctl`

```bash
# Запустить интерактивную живую TUI панель состояния:
zenithctl status --watch

# Проверить здоровье системы и обнаружить конфликтующие сервисы:
zenithctl doctor

# Переключить профиль (safe / balanced / aggressive):
zenithctl profile set aggressive

# Аварийный откат всех настроек ядра к заводским значениям:
zenithctl rollback

# Сформировать анонимизированный отчет для issue на GitHub:
zenithctl report --redact > report.txt
```
