# PaperWeb v1.3.0

![](paper_156961.bmp)  ![](paper_145852.bmp)

A web browser for your M5Cardputer. Because why not.

## What it does

Reads HTML on the fly, shows text and links. No JS, no CSS, no nonsense. Just pages.

**Now with offline mode** — save pages to SD, browse files, read saved stuff anywhere.

## Controls (short version)

| Key | What it does |
|-----|---------------|
| `ENTER` | Go to URL / Search |
| `TAB` | WiFi settings |
| `,` / `/` | Choose link |
| `;` / `.` | Scroll |
| `` ` `` | Back / Exit |
| `D` | Save page to SD (.txt) |
| `OPT` | File manager |
| `S` | Screenshot |
| `B` / `L` | Bookmarks |

## Install

1. Get the `.bin` file from [Releases](../../releases)
2. Flash with M5Burner
3. Put an SD card in (for saving pages)
4. Connect to WiFi (`TAB`) and go.

## What's new in v1.3.0

- Heavy pages like Wikipedia load and scroll a bit faster. Cleaned up some memory junk and now only draws what's actually on screen.
- Search is still Google — other engines block ESP32 requests.
- Everything else is the same. It's still just a text browser.

## Build

Arduino IDE, board ESP32-S3 Dev Module, 8MB flash. Libraries: M5Cardputer, M5Unified, efont, SD.

## License

MIT — do whatever.

— [Artem76228](https://github.com/Artem76228)  
Star if this actually worked for you.
