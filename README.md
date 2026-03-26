# 🌐 PaperWeb v0.4

**PaperWeb** is a fast, lightweight, and open-source text-based web browser designed specifically for the **M5Stack Cardputer**. Experience the web in its purest form—no ads, no distractions, just information.

---

## 📸 What's New in v0.4

| Before (v0.3) | After (v0.4) |
|:-------------:|:------------:|
| ![](before-fotor-202603269139.png) | ![](after-fotor-202603269215.png) |

**Heavy sites like Wikipedia now load perfectly — up to 126KB of clean text!**

---

## ✨ Key Features

### v0.4 Highlights
- **Real Progress Bar** — Visual download feedback with percentage and KB counter
- **Cyrillic & UTF-8 Support** — Full HTML entity decoding (including numeric entities like `&#1076;` → `д`)
- **Timeout Protection** — No more hanging on slow sites (15-second timeout)
- **Better HTML Structure** — Proper handling of `h1`, `h2`, `h3`, `li` tags for cleaner formatting
- **Scroll Percentage** — Know exactly where you are in the article
- **Dynamic Content Height** — Smooth zooming with proper scroll limits

### Core Features
- **PaperEngine™** — Custom parsing engine that intelligently filters out garbage. No more seeing `<script>` or CSS code on your screen!
- **True Streaming Technology** — Pages are processed byte-by-byte instead of loading the whole site into RAM. This makes the browser incredibly stable.
- **Memory Guard** — Built-in "Safety Valve" that stops loading if the Cardputer's RAM drops too low, preventing crashes.
- **HTML Entity Support** — Correctly renders symbols like `&nbsp;`, `&quot;`, `&amp;`, and numeric entities for a clean look.
- **Smart Search** — Type a query, and PaperWeb automatically takes you to Google.
- **Deep Buffer** — Handles up to 12,000 characters per page—that's a whole long-read article!

---

## ⌨️ Controls & Shortcuts

| Key | Action |
| :--- | :--- |
| **[ENTER]** | Open URL input / Google Search |
| **[TAB]** | Open WiFi Manager |
| **`;` / `.`** | Smooth Scroll Up / Down |
| **`=` / `-`** | Zoom In / Out |
| **`` ` `` (Backquote)** | Exit Reading Mode / Go Back |

---

## 🛠️ Installation

1. Go to the [Releases](https://github.com/Artem76228/PaperWeb/releases) section.
2. Download the `PaperWeb_v0.4.bin` file.
3. Flash it to your M5Stack Cardputer using **M5Burner** or **ESP32 Download Tool**.

---

## 📝 Changelog

### v0.4 (2026-03-26)
- **Added** real-time download progress with visual bar and percentage
- **Added** timeout protection (15s) — no more hanging on slow sites
- **Added** Cyrillic/UTF-8 support via numeric HTML entity decoding
- **Added** scroll position indicator in percent
- **Added** better HTML structure support (h1, h2, h3, li tags)
- **Improved** memory management (15KB threshold for stability)
- **Changed** default start page to `ru.wikipedia.org` for better demo
- **Changed** User-Agent to `PaperWeb/0.4` for proper identification
- **Fixed** aggressive filtering that broke non-Latin text
- **Fixed** scroll boundary protection

---

*Created by [Artem76228](https://github.com/Artem76228). If you like this project, please leave a ⭐!*
