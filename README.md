# PaperWeb v1.2.0

![](paper_156961.bmp)  ![](paper_145852.bmp)

A text-based web browser for the **M5Stack Cardputer**.  
Now with offline reading and a built-in file manager.

## Why?

The Cardputer is an ESP32 with a screen and a keyboard. It needed a browser. So I made one.

PaperWeb streams and parses HTML on the fly, pulls out text and links, and renders it all on a 240×135 display. No JavaScript, no CSS, no bloat. Just text and links.

**v1.2.0 turns PaperWeb into a tiny offline document hub — save pages, browse your SD card, read saved files.**

## Features

- **gzip/deflate** — modern sites are compressed, this handles it
- **Cyrillic support** — Russian and Ukrainian text renders fine
- **Link navigation** — `,` and `/` to jump, `ENTER` to follow, highlighted in cyan
- **Browsing history** — `DEL` goes back (up to 10 steps)
- **Bookmarks** — `B` saves current page, `L` opens menu (5 slots)
- **Save page to TXT** — `D` saves the current page as a `.txt` file on the SD card
- **File manager** — `OPT` opens a menu with:
  - File browser (navigate SD card, open `.txt` files)
  - Quick access to `/saved_links` folder
- **Offline reading** — open any saved `.txt` file and read it like a web page
- **Screenshots** — `S` saves screen as BMP to SD card
- **WiFi Manager** — store up to 3 networks
- **Session cookies** — stays logged in on websites
- **Smart search** — type words without dots → Google search

## Controls

| Key | Action |
|-----|--------|
| `ENTER` | Open URL / Search |
| `TAB` | WiFi Manager |
| `,` / `/` | Previous / Next link |
| `;` / `.` | Scroll up / down |
| `=` / `-` | Zoom in / out |
| `` ` `` | Exit menu / Back |
| `DEL` | History back |
| `B` | Save bookmark |
| `L` | Open bookmarks menu |
| `D` | Save current page to SD as `.txt` |
| `OPT` | Open main menu (file browser, saved links) |
| `S` | Screenshot to SD |

## Installation

1. Download `PaperWeb_v1.2.0.bin` from [Releases](../../releases)
2. Flash using **M5Burner** or **ESP32 Download Tool**
3. Insert microSD card (required for saving pages, screenshots, and file manager)
4. Open WiFi Manager (`TAB`), connect to network
5. Browse the web like it's 1995 — or save pages and read them offline.

## What's New in v1.2.0

- **Save page to TXT (`D`)** – stores the current page content (without special characters) to `/saved_links/page_<timestamp>.txt`
- **File manager (`OPT`)** – browse any folder on the SD card, open `.txt` files, view them as formatted text
- **Saved links shortcut** – directly open the `/saved_links` folder from the menu
- **Optimized redraw rate** – now updates at ~10 FPS for smoother reading and better performance
- **Menu stability** – fixed exit logic and improved navigation in file browser
- **Better UTF-8 handling** – works seamlessly with Russian/English file names

## Known Issues

- Some heavy sites load slowly. It's an ESP32, not a MacBook.
- Bookmark menu sometimes lags — working on it.
- File browser shows only first 50 files per folder.

## Build It Yourself

Open the `.ino` file in Arduino IDE. Required libraries:
- `M5Cardputer`
- `M5Unified`
- `efont` (for Cyrillic)
- `SD` (built-in)
- `zlib` (for gzip)

Board: **ESP32-S3 Dev Module**  
Flash size: **8MB**

## License

MIT — do whatever you want with it.

---

Made by [Artem76228](https://github.com/Artem76228)  
⭐ Star this repo if it worked on your Cardputer
