# Tarkov Loot Prices

External native Windows overlay prototype for **Escape from Tarkov official PvE**.

> **v0.1: overlay demo only.** No screenshot capture, item recognition, real PvE prices, or item-specific labels yet. All demo prices are fictional. Not endorsed or approved by Battlestate Games; external overlays may still violate game rules.

## Current features

- Small click-through Win32 overlay using only Windows system libraries.
- Hides itself when Tarkov is not foreground (unless started with --demo).
- Ctrl+Shift+P toggles visibility; Ctrl+Shift+Q exits.
- Testable C++ functions comparing hypothetical flea and trader values per inventory slot.
- GitHub Actions builds MSVC x64 Release, runs unit tests, uploads TarkovLootPrices.exe.

## Build

Requires Windows 10/11 x64, Visual Studio 2022 Build Tools and CMake 3.24+.

    cmake -S . -B build -A x64
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --output-on-failure

Executable: build/Release/TarkovLootPrices.exe

For a standalone preview of illustrative prices (without Tarkov):

    .\build\Release\TarkovLootPrices.exe --demo

Default mode shows the status panel only when a foreground window title contains "Tarkov". No tray icon or installer yet. If global hotkeys are already used by another app, you can exit in Windows Task Manager.

## Next milestones

1. Screen-only capture of inventory regions, frame-diff scheduling, profiling.
2. Offline screenshot test dataset and item sprite matching with confidence scores.
3. tarkov.dev PvE market price API, local cache, expiration and trader comparison.
4. Price labels anchored to recognized visible items in inventory and loot containers.
5. Game compatibility tests, CPU/GPU measurements, anti-cheat/game-rule review.

Never inject DLLs, read game process memory or circumvent anti-cheat.
