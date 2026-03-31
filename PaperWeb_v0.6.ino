#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "ArduinoUZlib.h"

class PaperEngine {
public:
    int bytesDownloaded = 0;
    int totalBytes = 0;
    bool isDownloading = false;
    
    String currentLine;
    int lineCount = 0;
    
    void (*onLineReady)(String line, int lineNum) = nullptr;
    
    bool inTag = false;
    bool isSkipping = false;
    String currentTag = "";
    bool lastAddedNewline = false;
    
    void reset() {
        currentLine = "";
        lineCount = 0;
        inTag = false;
        isSkipping = false;
        currentTag = "";
        lastAddedNewline = false;
    }
    
    void flushLine() {
        if (currentLine.length() == 0) return;
        if (onLineReady) {
            onLineReady(currentLine, lineCount);
        }
        lineCount++;
        currentLine = "";
    }
    
    void addChar(char c) {
        if (c == ' ' && currentLine.endsWith(" ")) return;
        currentLine += c;
        
        if (currentLine.length() > 35) {
            int lastSpace = currentLine.lastIndexOf(' ');
            if (lastSpace > 0) {
                String linePart = currentLine.substring(0, lastSpace);
                String rest = currentLine.substring(lastSpace + 1);
                if (onLineReady) onLineReady(linePart, lineCount);
                lineCount++;
                currentLine = rest;
            } else {
                flushLine();
            }
        }
    }

bool fetch(String url, int timeoutMs = 60000, void (*progressCallback)(int, int) = nullptr) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(timeoutMs / 1000);
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(timeoutMs);

    if (!http.begin(client, url)) return false;

    http.addHeader("Accept-Encoding", "gzip, deflate");
    http.addHeader("User-Agent", "PaperWeb/0.6");
    
    int httpCode = http.GET();
    if (httpCode != 200) {
        if (httpCode == 301 || httpCode == 302) {
            String newLocation = http.header("Location");
            http.end();
            if (newLocation != "") {
                if (!newLocation.startsWith("http")) {
                    int lastSlash = url.lastIndexOf('/');
                    if (lastSlash > 7) {
                        String baseUrl = url.substring(0, lastSlash + 1);
                        newLocation = baseUrl + newLocation;
                    }
                }
                return fetch(newLocation, timeoutMs, progressCallback);
            }
        }
        http.end();
        return false;
    }
    
    String contentEncoding = http.header("Content-Encoding");
    bool isGzipped = (contentEncoding.indexOf("gzip") != -1);
    
    totalBytes = http.getSize();
    if (totalBytes <= 0) totalBytes = 1;
    
    isDownloading = true;
    reset();
    
    WiFiClient* stream = http.getStreamPtr();
    bytesDownloaded = 0;
    
    uint32_t startTime = millis();
    
    if (isGzipped) {
        String compressedData;
        int noDataCount = 0;
        while (http.connected() && millis() - startTime < timeoutMs) {
            if (ESP.getFreeHeap() < 15000) break;
            
            if (stream->available()) {
                noDataCount = 0;
                compressedData += (char)stream->read();
                bytesDownloaded++;
                if (progressCallback && bytesDownloaded % 500 == 0) {
                    progressCallback(bytesDownloaded, totalBytes);
                }
            } else {
                noDataCount++;
                if (noDataCount > 500) {
                    break;
                }
                delay(1);
            }
        }
        
        uint8_t* outbuf = NULL;
        uint32_t outsize = 0;
        int result = ArduinoUZlib::decompress(
            (uint8_t*)compressedData.c_str(), 
            compressedData.length(), 
            outbuf, 
            outsize
        );
        
        if (result == 0 && outbuf != NULL) {
            for (uint32_t i = 0; i < outsize; i++) {
                char c = (char)outbuf[i];
                
                if (c == '<') { 
                    inTag = true; 
                    currentTag = ""; 
                    continue; 
                }
                if (c == '>') { 
                    inTag = false;
                    currentTag.toLowerCase();
                    
                    if (currentTag == "script" || currentTag == "style" || currentTag == "head" || currentTag == "noscript") {
                        isSkipping = true;
                    }
                    if (currentTag == "/script" || currentTag == "/style" || currentTag == "/head" || currentTag == "/noscript") {
                        isSkipping = false;
                    }
                    
                    if (!isSkipping && (currentTag == "p" || currentTag == "div" || currentTag == "br" || 
                        currentTag == "h1" || currentTag == "h2" || currentTag == "h3" || currentTag == "li")) {
                        if (!lastAddedNewline) {
                            flushLine();
                            lastAddedNewline = true;
                        }
                    }
                    continue;
                }

                if (!inTag && !isSkipping) {
                    if (c == '{' || c == '}' || c == '[' || c == ']' || c == ';' || c == '=') continue;
                    
                    if ((uint8_t)c >= 32 || c == '\n' || c == '\t') {
                        addChar(c);
                        lastAddedNewline = false;
                    }
                } else if (inTag) {
                    if (c != ' ' && currentTag.indexOf(' ') == -1) currentTag += c;
                }
            }
            free(outbuf);
        } else {
            flushLine();
            http.end();
            return false;
        }
    } else {
        int noDataCount = 0;
        while (http.connected() && millis() - startTime < timeoutMs) {
            if (ESP.getFreeHeap() < 15000) break;
            
            if (stream->available()) {
                noDataCount = 0;
                char c = stream->read();
                bytesDownloaded++;
                
                if (progressCallback && bytesDownloaded % 500 == 0) {
                    progressCallback(bytesDownloaded, totalBytes);
                }

                if (c == '<') { 
                    inTag = true; 
                    currentTag = ""; 
                    continue; 
                }
                if (c == '>') { 
                    inTag = false;
                    currentTag.toLowerCase();
                    
                    if (currentTag == "script" || currentTag == "style" || currentTag == "head" || currentTag == "noscript") {
                        isSkipping = true;
                    }
                    if (currentTag == "/script" || currentTag == "/style" || currentTag == "/head" || currentTag == "/noscript") {
                        isSkipping = false;
                    }
                    
                    if (!isSkipping && (currentTag == "p" || currentTag == "div" || currentTag == "br" || 
                        currentTag == "h1" || currentTag == "h2" || currentTag == "h3" || currentTag == "li")) {
                        if (!lastAddedNewline) {
                            flushLine();
                            lastAddedNewline = true;
                        }
                    }
                    continue;
                }

                if (!inTag && !isSkipping) {
                    if (c == '{' || c == '}' || c == '[' || c == ']' || c == ';' || c == '=') continue;
                    
                    if ((uint8_t)c >= 32 || c == '\n' || c == '\t') {
                        addChar(c);
                        lastAddedNewline = false;
                    }
                } else if (inTag) {
                    if (c != ' ' && currentTag.indexOf(' ') == -1) currentTag += c;
                }
            } else {
                noDataCount++;
                if (noDataCount > 500) { // 500 мс без данных — загрузка завершена
                    break;
                }
                delay(1);
            }
        }
    }
    
    flushLine();
    http.end();
    
    return lineCount > 0;
}
};

PaperEngine engine;

const String APP_NAME = "PaperWeb";
const String APP_VERSION = "0.6";
const String AUTHOR = "Artem76228";

Preferences prefs;
String ssids[3], passs[3];
String currentURL = "https://ru.wikipedia.org";
String sysStatus = "READY"; 
int scrollPos = 0; 
float siteZoom = 1.0; 
int loadPercent = 0;
bool isReading = false;
int maxScrollPos = 0;
String loadStage = "";

String displayLines[100];
int displayLineCount = 0;

LGFX_Sprite canvas(&M5Cardputer.Display);
uint32_t targetTime = 0;

String utf8ToCp1251(String utf8) {
    String result = "";
    for (int i = 0; i < utf8.length(); i++) {
        uint8_t c = utf8[i];
        if (c < 128) {
            result += (char)c;
        }
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < utf8.length()) {
                uint8_t c2 = utf8[i + 1];
                uint16_t code = ((c & 0x1F) << 6) | (c2 & 0x3F);
                if (code >= 0x410 && code <= 0x42F) {
                    result += (char)(0xC0 + (code - 0x410));
                } else if (code >= 0x430 && code <= 0x44F) {
                    result += (char)(0xE0 + (code - 0x430));
                } else if (code == 0x401) {
                    result += (char)0xA8;
                } else if (code == 0x451) {
                    result += (char)0xB8;
                } else {
                    result += '?';
                }
                i++;
            }
        } else {
            result += '?';
        }
    }
    return result;
}

void onLineReady(String line, int lineNum) {
    String converted = utf8ToCp1251(line);
    if (lineNum < 100) {
        displayLines[lineNum] = converted;
        displayLineCount = lineNum + 1;
        int lines = displayLineCount;
        int lineHeight = siteZoom * 8;
        maxScrollPos = (lines + 5) * lineHeight / 2;
    }
}

void waitRelease() {
    M5Cardputer.update();
    while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(5); }
}

void drawBatteryUI(int x, int y, int pct) {
    canvas.drawRect(x, y, 18, 9, WHITE);
    canvas.fillRect(x + 18, y + 2, 2, 5, WHITE);
    int w = map(constrain(pct, 0, 100), 0, 100, 0, 16);
    canvas.fillRect(x + 1, y + 1, w, 7, (pct > 20) ? 0x07E0 : 0xF800);
}

void drawRSSI(int x, int y) {
    int rssi = WiFi.RSSI();
    int bars = (WiFi.status() != WL_CONNECTED) ? 0 : (rssi > -55 ? 4 : (rssi > -70 ? 3 : (rssi > -85 ? 2 : 1)));
    for (int i = 0; i < 4; i++) {
        uint16_t color = (i < bars) ? WHITE : 0x3186; 
        canvas.fillRect(x + (i * 4), y + (6 - (i * 2)), 3, (i * 2) + 2, color);
    }
}

void drawUI() {
    canvas.startWrite();
    canvas.fillSprite(BLACK);
    
    String textToShow = "";
    for (int i = 0; i < displayLineCount; i++) {
        textToShow += displayLines[i];
        if (i < displayLineCount - 1) textToShow += "\n";
    }
    
    canvas.setTextColor(WHITE);
    canvas.setTextSize(siteZoom);
    canvas.setCursor(0, 22 - (scrollPos * 2));
    canvas.setTextWrap(true);
    canvas.print(textToShow);
    
    uint16_t headerCol = (WiFi.status() != WL_CONNECTED) ? 0x8000 : (loadPercent > 0 ? 0xFBE0 : (isReading ? 0x2104 : 0x0400));
    canvas.fillRect(0, 0, 240, 18, headerCol); 
    
    int bat = map(M5Cardputer.Power.getBatteryVoltage(), 3300, 4200, 0, 100);
    bat = constrain(bat, 0, 100);

    drawBatteryUI(220, 5, bat);
    canvas.setTextColor(WHITE);
    canvas.setTextSize(1);
    canvas.setCursor(195, 5);
    canvas.printf("%d%%", bat);
    drawRSSI(175, 5);

    canvas.setClipRect(0, 0, 170, 18);
    canvas.setCursor(5, 5);
    canvas.printf("%s v%s | %s", APP_NAME.c_str(), APP_VERSION.c_str(), sysStatus.c_str());
    canvas.clearClipRect(); 

    if (loadPercent > 0 && loadPercent < 100) {
        canvas.fillRect(0, 18, map(loadPercent, 0, 100, 0, 240), 3, CYAN);
    }

    canvas.fillRect(0, 118, 240, 17, 0x18E3); 
    canvas.setCursor(5, 122);
    if (isReading) {
        int percent = (maxScrollPos > 0) ? (scrollPos * 100 / maxScrollPos) : 0;
        canvas.printf("SCROLL: %d%% | [`] EXIT | Z:%.1f", percent, siteZoom);
    } else {
        canvas.printf("[ENT] URL | [TAB] WiFi");
    }

    canvas.pushSprite(0, 0);
    canvas.endWrite();
}

String inputText(String prompt) {
    waitRelease();
    String text = "";
    uint32_t keyTimer = 0;
    while (true) {
        M5Cardputer.update();
        canvas.startWrite();
        canvas.fillSprite(BLACK);
        canvas.drawRect(5, 40, 230, 45, CYAN);
        canvas.setCursor(10, 20); canvas.setTextColor(YELLOW); canvas.print(prompt);
        canvas.setCursor(15, 58); canvas.setTextColor(WHITE); canvas.print(text + "_");
        canvas.pushSprite(0, 0);
        canvas.endWrite();
        auto status = M5Cardputer.Keyboard.keysState();
        if (status.word.size() > 0 && millis() - keyTimer > 180) {
            for (auto c : status.word) text += c;
            keyTimer = millis();
        }
        if (status.del && text.length() > 0 && millis() - keyTimer > 120) {
            text.remove(text.length() - 1);
            keyTimer = millis();
        }
        if (status.enter) { waitRelease(); return text; }
        if (M5Cardputer.Keyboard.isKeyPressed('`')) { waitRelease(); return ""; }
        delay(1); 
    }
}

void updateProgress(int downloaded, int total) {
    loadPercent = (downloaded * 100) / total;
    if (loadPercent > 100) loadPercent = 100;
    sysStatus = loadStage + " " + String(loadPercent) + "%";
    drawUI();
}

void loadWebPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    isReading = true;
    scrollPos = 0;
    
    for (int i = 0; i < 100; i++) displayLines[i] = "";
    displayLineCount = 0;
    
    loadStage = "CONNECTING";
    sysStatus = loadStage;
    loadPercent = 0;
    drawUI();
    delay(100);
    
    loadStage = "DOWNLOADING";
    sysStatus = loadStage;
    drawUI();
    
    engine.onLineReady = onLineReady;
    bool ok = engine.fetch(currentURL, 60000, updateProgress);
    
    if (ok && displayLineCount > 0) {
        sysStatus = "DONE " + String(engine.bytesDownloaded / 1024) + "KB";
        loadPercent = 100;
        int lineHeight = siteZoom * 8;
        maxScrollPos = (displayLineCount + 5) * lineHeight / 2;
    } else {
        sysStatus = "FAIL";
        displayLines[0] = "FAILED\n\n" + currentURL;
        displayLineCount = 1;
        maxScrollPos = 0;
    }
    
    drawUI();
}

void openWiFiMenu() {
    waitRelease();
    int sel = 0;
    while (true) {
        M5Cardputer.update();
        canvas.startWrite();
        canvas.fillSprite(BLACK);
        canvas.setCursor(10, 10); canvas.setTextColor(CYAN); canvas.println("NET MANAGER");
        for (int i = 0; i < 3; i++) {
            canvas.setCursor(10, 35 + (i * 20));
            if (sel == i) canvas.setTextColor(BLACK, WHITE); else canvas.setTextColor(WHITE);
            canvas.printf("Slot %d: %s", i + 1, ssids[i] == "" ? "Empty" : ssids[i].c_str());
        }
        canvas.pushSprite(0, 0);
        canvas.endWrite();
        if (M5Cardputer.Keyboard.isKeyPressed(';')) { sel = (sel > 0) ? sel - 1 : 2; delay(150); }
        if (M5Cardputer.Keyboard.isKeyPressed('.')) { sel = (sel < 2) ? sel + 1 : 0; delay(150); }
        auto status = M5Cardputer.Keyboard.keysState();
        if (status.enter) {
            String s = inputText("SSID:");
            if (s != "") {
                String p = inputText("PASS:");
                ssids[sel] = s; passs[sel] = p;
                prefs.putString(("s" + String(sel)).c_str(), s);
                prefs.putString(("p" + String(sel)).c_str(), p);
                WiFi.disconnect(); WiFi.begin(s.c_str(), p.c_str());
            }
            waitRelease(); break;
        }
        if (status.tab || M5Cardputer.Keyboard.isKeyPressed('`')) { waitRelease(); break; }
        delay(1);
    }
}

void setup() {
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Power.begin();
    canvas.setColorDepth(8);
    canvas.createSprite(240, 135);

    canvas.fillSprite(BLACK);
    canvas.setTextColor(WHITE);
    canvas.setTextSize(2);
    canvas.drawCenterString(APP_NAME, 120, 30);
    canvas.drawLine(40, 65, 200, 65, CYAN);
    
    canvas.setTextSize(1);
    canvas.setTextColor(CYAN);
    canvas.drawCenterString("v" + APP_VERSION, 120, 75);
    
    canvas.setTextColor(0xBDD7);
    canvas.drawCenterString("by " + AUTHOR, 120, 95);
    
    canvas.setTextColor(ORANGE);
    canvas.drawCenterString("FASTER LOADING", 120, 115); 

    canvas.pushSprite(0, 0);
    delay(1500); 
    
    prefs.begin("wifi-config", false);
    for (int i = 0; i < 3; i++) {
        ssids[i] = prefs.getString(("s" + String(i)).c_str(), "");
        passs[i] = prefs.getString(("p" + String(i)).c_str(), "");
    }
    if (ssids[0] != "") WiFi.begin(ssids[0].c_str(), passs[0].c_str());
    
    currentURL = "https://ru.wikipedia.org";
    engine.onLineReady = onLineReady;
}

void loop() {
    if (millis() > targetTime) {
        targetTime = millis() + 30; 
        M5Cardputer.update();
        drawUI();
        auto status = M5Cardputer.Keyboard.keysState();
        if (isReading) {
            if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
                if (scrollPos > 0) scrollPos -= 4;
                if (scrollPos < 0) scrollPos = 0;
            }
            if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
                scrollPos += 4;
                if (scrollPos > maxScrollPos) scrollPos = maxScrollPos;
            }
            
            if (M5Cardputer.Keyboard.isKeyPressed('=')) { 
                siteZoom += 0.1; 
                if (siteZoom > 3.0) siteZoom = 3.0;
                maxScrollPos = displayLineCount * 12 * siteZoom;
                delay(100); 
            }
            if (M5Cardputer.Keyboard.isKeyPressed('-')) { 
                siteZoom -= 0.1; 
                if (siteZoom < 0.5) siteZoom = 0.5;
                maxScrollPos = displayLineCount * 12 * siteZoom;
                delay(100); 
            }
            if (M5Cardputer.Keyboard.isKeyPressed('`')) { 
                isReading = false; 
                waitRelease(); 
            }
        } else {
            if (status.tab) openWiFiMenu();
            if (status.enter) {
                if (WiFi.status() != WL_CONNECTED) openWiFiMenu();
                else {
                    String url = inputText("URL:");
                    if (url != "") {
                        if (url.indexOf('.') == -1) url = "https://www.google.com/search?q=" + url;
                        else if (!url.startsWith("http")) url = "https://" + url;
                        currentURL = url;
                        loadWebPage();
                    }
                }
            }
        }
    }
}