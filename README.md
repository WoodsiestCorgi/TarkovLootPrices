# Tarkov Loot Prices — experimental PvE overlay

Native external Windows x64 overlay for official **Escape from Tarkov PvE**. The application never opens the game's process, modifies game files, injects DLLs, or sends simulated game input. **Third-party tool, not approved by BSG.** External overlays can still violate game rules. Use at your own discretion.

## What is implemented

- Win32 layered click-through overlay (no browser, no Electron, no Python interpreter at runtime).
- PvE item catalogue and flea/trader market values, refreshed every 5 minutes over HTTPS with a disk cache.
- Low-frequency screen capture with GDI only while Tarkov is foreground; scans manually selected inventory / loot-container grids.
- Small icon descriptor index built once from tarkov.dev sprite images, bundled with the Windows artifact.
- Confidence filtering: when sprite recognition is ambiguous or uncertain, **no price** is shown rather than inventing a match.
- Search window for any item, including when its icon cannot be recognized.
- Local BMP diagnostic capture for offline testing and eventual algorithm tuning.
- CMake/MSVC, unit tests and a portable ZIP packaged automatically in GitHub Actions.

## Installation

1. Download the successful workflow **TarkovLootPrices-windows-x64** artifact from the GitHub Actions tab.
2. Unzip the GitHub artifact, then unzip the packaged TarkovLootPrices-windows-x64.zip inside.
3. Keep TarkovLootPrices.exe and the data/ folder together. Run the EXE.
4. Use **Borderless Window** at **1920x1080** with game UI at **100%** for initial calibration. The default grid pitch is **63 pixels**, but can be changed in settings.
5. On first run, press **Ctrl+Shift+F** to open the PvE search window; this lets you verify that network pricing works before configuring scanning.
6. To calibrate the personal inventory grid, point at its exact top-left grid border and press **Ctrl+Alt+1**. Point at the bottom-right border of the whole grid and press **Ctrl+Alt+2**.
7. Optionally repeat for the open loot container using **Ctrl+Alt+3** and **Ctrl+Alt+4**. Regions are saved to %LOCALAPPDATA%/TarkovLootPrices/settings.ini. For nonstandard UI scales, add a `[capture]` section with `cell_pitch=63` (change 63 to measured cell size in pixels), then restart the program.

Grid crops must include **whole cells**, each exactly the configured pixel pitch; a few pixels of offset can substantially reduce recognition quality. On-screen recognized labels use F for flea value (last low or 24-hour fallback) and T for best trader value when flea data is unavailable. Costs do not include flea fees. Stacks, durability, modded equipment, found-in-raid restrictions and duplicate-looking icons are not fully accounted for; treat values as guides, not exact sale proceeds.

## Hotkeys

| Shortcut | Action |
|---|---|
| Ctrl+Shift+F | Open/close the PvE item price search window |
| Ctrl+Shift+P | Toggle automatic price labels |
| Ctrl+Shift+R | Refresh the cached PvE catalogue |
| Ctrl+Shift+V | Cycle between market price (F), best value per slot (S) and best sale offer (B) |
| Ctrl+Shift+S | Save an offline full-screen BMP screenshot to %LOCALAPPDATA%/TarkovLootPrices/screen_capture.bmp |
| Ctrl+Shift+Q | Exit the program |
| Ctrl+Alt+1 / 2 | Set upper-left / lower-right personal inventory grid boundaries |
| Ctrl+Alt+3 / 4 | Set upper-left / lower-right open-container grid boundaries |

Run with `--demo` outside the game to show the application status widget and catalogue/index counts.

## Limitations and testing

The sprite matcher is a **best-effort experimental recognizer**, designed around native 1080p/100% pixel alignment. It has automated synthetic tests, **not an established recognition accuracy measured inside the official PvE game**. The app is not a trusted, anti-cheat-approved tool. Before using it with your Tarkov account, check the game's rules and decide whether you accept the risk.

Automatic recognition only covers visually available grids in the two configured regions, and does not access hidden slots or unopened containers. Do not share screenshot files if they contain private content.

## Build

Requires Visual Studio 2022 Build Tools, CMake 3.24+, network connection for CMake FetchContent (nlohmann/json):

    cmake -S . -B build -A x64
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --output-on-failure

For the sprite index (only during **build**, not at runtime), Python 3.12 + Pillow:

    python -m pip install pillow
    python tools/build_index.py --output data/icons.json

Build artifacts bundle data/icons.json. Prices are from the community-funded API at https://json.tarkov.dev (PvE mode); screenshots never leave the PC.

## Performance

The app runs a low-frequency grid scan every 1.5 seconds only with Tarkov foreground, and skips expensive pattern matching when sampled pixels have not changed. It should be measured on actual hardware: CPU/GPU/RAM overhead and FPS impact are **not benchmarked yet**. No claim of guaranteed zero FPS impact is made.

Tarkov is a trademark of Battlestate Games. Unofficial project; no affiliation or endorsement.
