# 🌐 PaperWeb v0.5

**PaperWeb** is a fast, lightweight, and open-source text-based web browser designed specifically for the **M5Stack Cardputer**. Experience the web in its purest form—no ads, no distractions, just information.

---

## ✨ Key Features

### v0.5 Highlights
- **Streaming Renderer** — Pages are processed byte-by-byte and displayed line-by-line. No more waiting for the whole page to load!
- **No Framebuffer** — Uses only ~100KB of RAM for display buffer. The browser never crashes on large pages.
- **Real-Time Loading** — Text appears instantly as it's downloaded, line by line.
- **Cyrillic Support** — Full UTF-8 to CP1251 conversion. Russian Wikipedia displays perfectly.
- **Progress Bar** — Visual download feedback with percentage and KB counter.
- **Timeout Protection** — No hanging on slow sites (30-second timeout).
- **Scroll Percentage** — Know exactly where you are in the article.
- **Smart Search** — Type a query, and PaperWeb automatically takes you to Google.

### Core Features
- **PaperEngine™** — Custom parsing engine that intelligently filters out garbage. No more seeing `<script>` or CSS code on your screen!
- **True Streaming Technology** — Pages are processed byte-by-byte instead of loading the whole site into RAM. This makes the browser incredibly stable.
- **Memory Guard** — Built-in "Safety Valve" that stops loading if the Cardputer's RAM drops too low, preventing crashes.
- **HTML Entity Support** — Correctly renders symbols like `&nbsp;`, `&quot;`, `&amp;`, and numeric entities for a cleaner look.
- **WiFi Manager** — Save up to 3 networks, switch seamlessly.

---

## ⌨️ Controls & Shortcuts

| Key | Action |
| :--- | :--- |
| **[ENTER]** | Open URL input / Google Search |
| **[TAB]** | Open WiFi Manager |
| **`;` / `.`** | Scroll Up / Down |
| **`=` / `-`** | Zoom In / Out |
| **`` ` `` (Backquote)** | Exit Reading Mode / Go Back |

---

## 🛠️ Installation

1. Go to the [Releases](https://github.com/Artem76228/PaperWeb/releases) section.
2. Download the `PaperWeb_v0.5.bin` file.
3. Flash it to your M5Stack Cardputer using **M5Burner** or **ESP32 Download Tool**.

---

## 📝 Changelog

### v0.5 (2026-03-28)
- **Added** streaming renderer — text appears line-by-line as it downloads
- **Added** full Cyrillic support (UTF-8 → CP1251 conversion)
- **Added** real-time progress with percentage
- **Added** timeout protection (30s) — no more hanging on slow sites
- **Added** scroll percentage indicator
- **Improved** memory usage — no framebuffer, only 100KB RAM for display
- **Improved** HTML structure support (h1, h2, h3, li tags)
- **Changed** default start page to `ru.wikipedia.org`
- **Changed** User-Agent to `PaperWeb/0.5`
- **Fixed** aggressive filtering that broke non-Latin text
- **Fixed** scroll boundary protection
- **Fixed** text overlapping issues

### v0.4 (2026-03-26)
- Added real-time download progress with visual bar and percentage
- Added timeout protection (15s)
- Added numeric HTML entity decoding
- Added scroll position indicator
- Improved memory management
- Fixed non-Latin text support

---

*Created by [Artem76228](https://github.com/Artem76228). If you like this project, please leave a ⭐!*
