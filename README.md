# Artemis OS 3DS

A tiny Windows 95-style desktop shell running as 3DS homebrew.

![platform](https://img.shields.io/badge/platform-3DS-blue)

[![Ko-fi](https://img.shields.io/badge/support_me_on_ko--fi-F16061?style=for-the-badge&logo=kofi&logoColor=f5f5f5)](https://ko-fi.com/snowystorm)

## Features

- Draggable windows with title bars, close buttons, z-order and focus
- Taskbar with a Start menu, live clock, and battery indicator
- On-screen QWERTY keyboard and a trackpad on the bottom touch screen
- Classic-style synthesized sound effects
- Apps:
  - **Notes** - text editor, saves to the SD card
  - **Paint** - 192x128 indexed-color canvas with pencil, brush, eraser, fill, and line tools. Draw with the stylus directly on the bottom screen in touch-draw mode.
  - **Calculator**
  - **Minesweeper** - 9x9 classic board
  - **File Explorer** - browses `sdmc:/3ds/artemis/`
  - **About**
  - **BASIC** - GW-BASIC style interpreter with a Windows-95 terminal

## New 3DS / New 2DS support

Artemis detects the console model at startup via `cfgu` (`CFGU_GetSystemModel`) and exposes `is_new3ds()` (`source/core.c`). On New 3DS / New 2DS models (which have more RAM and a faster CPU) the following is enabled:

- **About** - window shows "Model: New 3DS / New 2DS" instead of "Original 3DS / 2DS"
- **Paint** - larger canvas (256x144 vs 192x128) and a wider window (300x200 vs 228x200)
- **Notes** - bigger text buffer (4096 vs 2048 bytes)
- **Audio** - higher SFX sample rate (48000 Hz vs 32000 Hz)

Original 3DS / 2DS models keep the smaller defaults.

## Controls

| Input | Action |
| --- | --- |
| Stylus on trackpad | Move cursor, tap to click |
| Stylus on keyboard | Type into focused field |
| A or L | Click at cursor |
| R | Right-click (flags Minesweeper cells) |
| B | Close focused window |
| D-pad / Circle pad | Nudge cursor |
| START | Toggle Start menu |
| SELECT | Quit |

## BASIC

Artemis includes a GW-BASIC style interpreter (`source/app_basic.c`). Launch **BASIC** from the desktop or Start menu.

It is a line-numbered interpreter. Type a line starting with a number to store a program line, or a bare command to run it immediately.

```
10 PRINT "hello, world"
20 FOR I = 1 TO 10
30 PRINT I
40 NEXT I
RUN
```

### Statements
`REM`, `LET`/assignment (`X = 5`, `A$ = "hi"`), `PRINT`, `INPUT "[prompt];" var`, `CLS`, `COLOR fg,bg`, `LOCATE row,col`, `IF expr THEN ...`, `GOTO line`, `GOSUB line` / `RETURN`, `FOR var = x TO y [STEP z]` / `NEXT var`, `END`, `STOP`.

### Direct commands
`RUN` - start the program · `LIST` - show program · `NEW` - clear program · `CLS` - clear screen · `BYE`/`EXIT` - hint to close the window.

### Functions
`RND`, `INT`, `ABS`, `SGN`, `SQR`, `LEN`, `STR$`, `VAL`, `ASC`, `CHR$`.

Variables are `A`–`Z` (numeric) and `A$`–`Z$` (string). Programs live only while the window is open and are not saved to disk.

## Building

Requires [devkitPro](https://devkitpro.org) with `devkitARM` and `libctru` installed.

```
make
```

The resulting `artemis-os-3ds.3dsx` can be copied to `sdmc:/3ds/` on your 3DS and launched from the Homebrew Launcher, or opened directly in [Azahar](https://github.com/azahar-emu/azahar) / Citra.

## Audio

Audio needs the DSP firmware (`dspfirm.cdc`) which Nintendo's copyright prevents shipping. Run [DSP1](https://github.com/zoogie/DSP1/releases) once to dump it from your console, then Artemis OS (and all other homebrew) will play sound. The About window will tell you if audio isn't initialized.

## File layout

Saves and bundled data live under `sdmc:/3ds/artemis/`:

- `*.txt` - Notes documents
- `*.apt` - Paint images (custom 4bpp format)

## License

MIT.
