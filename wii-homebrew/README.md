# Hello Wii — Homebrew Demo

A minimal Nintendo Wii Homebrew application that displays **"Hello from my Wii!"** on screen using the Wii's video output via **devkitPPC** and **libogc**.

---

## Project Structure

```
wii-homebrew/
├── source/
│   └── main.c          # Application source
├── Makefile            # Build system (devkitPPC wii_rules)
├── meta.xml            # Homebrew Channel metadata
└── README.md
```

---

## Prerequisites

Install **devkitPro** with the Wii toolchain:

### Linux / macOS

```bash
# 1. Download and run the devkitPro pacman installer
#    https://github.com/devkitPro/installer/releases

# 2. Install the Wii development package group
sudo dkp-pacman -S wii-dev

# 3. Export the required environment variables (add to ~/.bashrc or ~/.zshrc)
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH=$DEVKITPPC/bin:$PATH
```

### Windows

Use the [devkitPro Windows installer](https://github.com/devkitPro/installer/releases) and select the **Wii** target. After installation, open the **devkitPro MSYS2** shell included with devkitPro.

---

## Building

```bash
# Navigate to the project directory
cd wii-homebrew

# Build — produces boot.elf and boot.dol
make

# Clean build output
make clean
```

A successful build produces:
- **`boot.dol`** — the Wii executable (load this with the Homebrew Channel)
- **`boot.elf`** — used by Dolphin Emulator and debugging tools

---

## Running on Hardware (Homebrew Channel)

1. Copy the entire `wii-homebrew/` folder (renamed to something meaningful, e.g. `hello-wii/`) onto your SD card under `sd:/apps/hello-wii/`.  
   The SD card structure should look like:
   ```
   SD:/
   └── apps/
       └── hello-wii/
           ├── boot.dol
           └── meta.xml
   ```
2. Insert the SD card into your Wii and launch the **Homebrew Channel**.
3. Select **Hello Wii** from the app list.
4. Press the **HOME** button on the Wiimote to exit.

---

## Running in Dolphin Emulator (no hardware needed)

1. Open [Dolphin Emulator](https://dolphin-emu.org/).
2. Go to **File → Open** and select `boot.elf` (or `boot.dol`).
3. The greeting will appear in the emulated Wii's output window.

---

## How It Works

| Step | Code | Purpose |
|------|------|---------|
| 1 | `VIDEO_Init()` | Initialise Wii video subsystem |
| 2 | `WPAD_Init()` | Initialise Wiimote input |
| 3 | `VIDEO_GetPreferredMode()` | Auto-detect TV format (480i/480p/576i) |
| 4 | `SYS_AllocateFramebuffer()` | Allocate XFB memory |
| 5 | `console_init()` | Set up printf → framebuffer mapping |
| 6 | `VIDEO_Configure()` + `VIDEO_Flush()` | Start video output |
| 7 | `printf(...)` | Render text to screen |
| 8 | Main loop with `VIDEO_WaitVSync()` | Keep screen alive; exit on HOME |

---

## Extending This Project

- Add more source files to `source/` — the Makefile picks them up automatically.
- Add header-only or hand-written headers to `include/`.
- Link additional libogc modules by appending to `LIBS` in the Makefile (e.g. `-lwiikeyboard`, `-lasnd`).
- Add a controller input library like **wiiuse** (already linked) for full Wiimote/Nunchuk support.

---

## References

- [devkitPro](https://devkitpro.org) — toolchain homepage
- [libogc documentation](https://libogc.devkitpro.org) — Wii/GCN library reference
- [WiiBrew Wiki](https://wiibrew.org) — Wii homebrew community
- [Dolphin Emulator](https://dolphin-emu.org) — Wii/GCN emulator for testing
