#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <zlib.h>   
#include <SD.h>    
#include <SPI.h> 

#include <efont.h>
#include <efontEnableAll.h>
#define SD_CS   12
#define SD_MOSI 14
#define SD_MISO 39
#define SD_SCK  40

enum ParseState { STATE_TEXT, STATE_TAG, STATE_SCRIPT, STATE_STYLE, STATE_ENTITY };

class PaperEngine {
public:
    int bytesDownloaded = 0;
    int totalBytes = 0;
    bool isDownloading = false;
    
    String currentLine;
    int lineCount = 0;
    
    String linkURLs[100]; 
    int linkCount = 0;
    String sessionCookie = "";
    
    void (*onLineReady)(String line, int lineNum) = nullptr;
    
    ParseState currentState = STATE_TEXT;
    String tagBuffer = "";
    bool lastWasSpace = false;
    
    String endTagBuffer = "";
    String entityBuffer = "";

    void reset() {
        currentLine = "";
        lineCount = 0;
        linkCount = 0; 
        currentState = STATE_TEXT;
        tagBuffer = "";
        lastWasSpace = false;
        endTagBuffer = "";
        entityBuffer = "";
    }
    
    void flushLine() {
        if (currentLine.length() == 0) return;
        if (lineCount >= 99) return; 
        
        if (onLineReady) {
            onLineReady(currentLine, lineCount);
        }
        lineCount++;
        currentLine = "";
        lastWasSpace = true; 
    }
    
    void addChar(char c) {
        if (lineCount >= 99) return; 
        
        currentLine += c;
        if (currentLine.length() > 40) {
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

    void decodeEntity(String ent) {
        if (ent.startsWith("#")) {
            int code;
            if (ent.startsWith("#x")) { 
                code = strtol(ent.substring(2).c_str(), NULL, 16);
            } else { 
                code = ent.substring(1).toInt();
            }

            if (code >= 1024 && code <= 1105) { 
                if (code >= 1040 && code <= 1087) { 
                    addChar((char)0xD0); 
                    addChar((char)(0x90 + (code - 1040)));
                } else if (code >= 1088 && code <= 1103) {
                    addChar((char)0xD1); 
                    addChar((char)(0x80 + (code - 1088)));
                } else if (code == 1025) { 
                    addChar((char)0xD0); addChar((char)0x81);
                } else if (code == 1105) { 
                    addChar((char)0xD1); addChar((char)0x91);
                }
            } else if (code > 31 && code < 127) {
                addChar((char)code);
            }
        } else {
            if (ent == "nbsp") addChar(' ');
            else if (ent == "lt") addChar('<');
            else if (ent == "gt") addChar('>');
            else if (ent == "amp") addChar('&');
            else if (ent == "quot") addChar('"');
        }
    }

    void processHTMLChar(char c) {
        switch (currentState) {
            case STATE_TEXT:
                if (c == '<') {
                    currentState = STATE_TAG;
                    tagBuffer = "";
                } else if (c == '&') {
                    currentState = STATE_ENTITY;
                    entityBuffer = "";
                } else {
                    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        if (!lastWasSpace && currentLine.length() > 0) {
                            addChar(' ');
                            lastWasSpace = true;
                        }
                    } else {
                        addChar(c);
                        lastWasSpace = false;
                    }
                }
                break;

            case STATE_TAG:
                if (c == '>') {
                    String baseTag = tagBuffer;
                    int spaceIdx = baseTag.indexOf(' ');
                    if (spaceIdx != -1) baseTag = baseTag.substring(0, spaceIdx);
                    baseTag.toLowerCase();

                    if (baseTag == "script") {
                        currentState = STATE_SCRIPT;
                        endTagBuffer = "";
                    } else if (baseTag == "style" || baseTag == "svg" || baseTag == "noscript" || baseTag == "head") {
                        currentState = STATE_STYLE;
                        endTagBuffer = "";
                    } else {
                        if (baseTag == "a") {
                            int hIdx = tagBuffer.indexOf("href=");
                            if (hIdx != -1) {
                                int qStart = tagBuffer.indexOf('"', hIdx);
                                if (qStart == -1) qStart = tagBuffer.indexOf('\'', hIdx);
                                if (qStart != -1) {
                                    int qEnd = tagBuffer.indexOf(tagBuffer[qStart], qStart + 1);
                                    if (qEnd != -1 && linkCount < 100) {
                                        linkURLs[linkCount] = tagBuffer.substring(qStart + 1, qEnd);
                                        addChar('\x01'); 
                                        addChar((char)(linkCount + 32)); 
                                        linkCount++;
                                    }
                                }
                            }
                        } else if (baseTag == "/a") {
                            addChar('\x02'); 
                        } else if (baseTag == "br" || baseTag == "p" || baseTag == "div" || baseTag == "tr") {
                            flushLine();
                        } else if (baseTag == "li") {
                            flushLine();
                            currentLine += " • ";
                        } else if (baseTag == "h1" || baseTag == "h2" || baseTag == "h3") {
                            flushLine();
                            currentLine += "[H] ";
                        }
                        currentState = STATE_TEXT;
                    }
                } else {
                    if (tagBuffer.length() < 180) tagBuffer += c; 
                }
                break;

            case STATE_SCRIPT:
                endTagBuffer += c;
                if (endTagBuffer.length() > 9) endTagBuffer.remove(0, 1);
                if (endTagBuffer.equalsIgnoreCase("</script>")) {
                    endTagBuffer = "";
                    currentState = STATE_TEXT;
                }
                break;

            case STATE_STYLE:
                endTagBuffer += c;
                if (endTagBuffer.length() > 11) endTagBuffer.remove(0, 1);
                if (endTagBuffer.endsWith(">")) {
                    String check = endTagBuffer;
                    check.toLowerCase();
                    if (check.endsWith("</style>") || check.endsWith("</svg>") || 
                        check.endsWith("</noscript>") || check.endsWith("</head>")) {
                        currentState = STATE_TEXT;
                        endTagBuffer = "";
                    }
                }
                break;

            case STATE_ENTITY:
                if (c == ';') {
                    decodeEntity(entityBuffer);
                    currentState = STATE_TEXT;
                } else if (c == ' ' || entityBuffer.length() > 8) {
                    currentState = STATE_TEXT;
                    addChar('&');
                    for(int i = 0; i < (int)entityBuffer.length(); i++) addChar(entityBuffer[i]);
                    addChar(c);
                } else {
                    entityBuffer += c;
                }
                break;
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

        const char* headerKeys[] = {"Set-Cookie", "Location", "Content-Encoding"};
        http.collectHeaders(headerKeys, 3);

        http.addHeader("Accept-Encoding", "gzip, deflate");
        http.addHeader("User-Agent", "Mozilla/5.0 (Linux; Android 10; Mobile) AppleWebKit/537.36 PaperWeb/");
        
        if (sessionCookie != "") {
            http.addHeader("Cookie", sessionCookie);
        }
        
        int httpCode = http.GET();
        
        if (http.hasHeader("Set-Cookie")) {
            sessionCookie = http.header("Set-Cookie");
        }

        if (httpCode != 200) {
            if (httpCode == 301 || httpCode == 302 || httpCode == 307 || httpCode == 308) {
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
            z_stream strm;
            strm.zalloc = Z_NULL; strm.zfree = Z_NULL; strm.opaque = Z_NULL;
            strm.avail_in = 0; strm.next_in = Z_NULL;

            if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
                http.end(); return false;
            }

            uint8_t in_buf[1024]; uint8_t out_buf[2048];
            int noDataCount = 0;

            while (http.connected() && millis() - startTime < timeoutMs) {
                if (ESP.getFreeHeap() < 20000) break; 
                
                size_t avail = stream->available();
                if (avail > 0) {
                    noDataCount = 0;
                    size_t readLen = (avail < sizeof(in_buf)) ? avail : sizeof(in_buf);
                    int bytesRead = stream->read(in_buf, readLen);

                    if (bytesRead > 0) {
                        bytesDownloaded += bytesRead;
                        if (progressCallback && bytesDownloaded % 2048 == 0) progressCallback(bytesDownloaded, totalBytes);

                        strm.avail_in = bytesRead; 
                        strm.next_in = in_buf;

                        bool stopParsing = false;

                        while (strm.avail_in > 0 && !stopParsing) {
                            strm.avail_out = sizeof(out_buf); 
                            strm.next_out = out_buf;
                            int ret = inflate(&strm, Z_NO_FLUSH, 0);
        
                            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) break;

                            size_t have = sizeof(out_buf) - strm.avail_out;
                            for (size_t i = 0; i < have; i++) {
                                processHTMLChar((char)out_buf[i]);
                                if (lineCount >= 99) {
                                    stopParsing = true;
                                    break;
                                }
                            }
                            if (ret == Z_STREAM_END) break;
                        }
                    }
                } else {
                    noDataCount++;
                    if (noDataCount > 1000) break;
                    delay(1);
                }
            }
            inflateEnd(&strm);
        } else {
            uint8_t readBuf[512];
            int noDataCount = 0;
            while (http.connected() && millis() - startTime < timeoutMs) {
                if (ESP.getFreeHeap() < 15000 || lineCount >= 99) break;
                size_t avail = stream->available();
                if (avail > 0) {
                    noDataCount = 0;
                    size_t toRead = (avail < sizeof(readBuf)) ? avail : sizeof(readBuf);
                    int n = stream->read(readBuf, toRead);
                    if (n > 0) {
                        bytesDownloaded += n;
                        if (progressCallback) progressCallback(bytesDownloaded, totalBytes);
                        for (int i = 0; i < n && lineCount < 99; i++) {
                            processHTMLChar((char)readBuf[i]);
                        }
                    }
                } else {
                    noDataCount++;
                    if (noDataCount > 1000) break;
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
const String APP_VERSION = "1.3.0";
#define AUTHOR_NAME "Artem76228"

Preferences prefs;
String ssids[3], passs[3];
String currentURL = "https://ru.wikipedia.org";
String sysStatus = "READY"; 

String urlHistory[10];
int historyCount = 0;
int selectedLink = 0;

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

void waitRelease() {
    M5Cardputer.update();
    while (M5Cardputer.Keyboard.isPressed()) { M5Cardputer.update(); delay(5); }
}

String resolveURL(String base, String rel) {
    if (rel.startsWith("http")) return rel;
    if (rel.startsWith("//")) return "https:" + rel;
    if (rel.startsWith("/")) {
        int idx = base.indexOf("/", 8); 
        if (idx != -1) return base.substring(0, idx) + rel;
        return base + rel;
    }
    int idx = base.lastIndexOf("/");
    if (idx > 7) return base.substring(0, idx + 1) + rel;
    return base + "/" + rel;
}

void onLineReady(String line, int lineNum) {
    if (lineNum < 100) {
        displayLines[lineNum] = line;
        displayLineCount = lineNum + 1;
        int lines = displayLineCount;
        int lineHeight = siteZoom * 12; 
        maxScrollPos = (lines + 5) * lineHeight / 2;
    }
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

    int lineH = (int)(12.0f * siteZoom);
    if (lineH < 1) lineH = 1;
    int yOffset = 22 - (scrollPos * 2); //y pos

    int firstLine = 0;
    if (yOffset < 18) {
        firstLine = (18 - yOffset) / lineH;
        if (firstLine < 0) firstLine = 0;
    }

    canvas.setTextSize(siteZoom);
    canvas.setTextWrap(false); 
    canvas.setTextColor(WHITE, BLACK);

    for (int i = firstLine; i < displayLineCount; i++) {
        int lineY = yOffset + i * lineH;
        if (lineY >= 118) break; //footer 

        canvas.setCursor(0, lineY);
        String& line = displayLines[i]; 

        for (int j = 0; j < (int)line.length(); j++) {
            char c = line[j];
            if (c == '\x01') {
                j++;
                if (j < (int)line.length()) {
                    int lnkId = line[j] - 32;
                    if (lnkId == selectedLink) canvas.setTextColor(BLACK, CYAN);
                    else canvas.setTextColor(CYAN, BLACK);
                }
            } else if (c == '\x02') {
                canvas.setTextColor(WHITE, BLACK);
            } else {
                canvas.print(c);
            }
        }
        canvas.setTextColor(WHITE, BLACK);//reset color
    }

    //Header bar
    uint16_t headerCol = (WiFi.status() != WL_CONNECTED) ? 0x8000 : (loadPercent > 0 ? 0xFBE0 : (isReading ? 0x2104 : 0x0400));
    canvas.fillRect(0, 0, 240, 18, headerCol); 
    
    int bat = map(M5Cardputer.Power.getBatteryVoltage(), 3300, 4200, 0, 100);
    bat = constrain(bat, 0, 100);

    drawBatteryUI(220, 5, bat);
    canvas.setTextColor(WHITE, headerCol); 
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
    canvas.setTextColor(WHITE, 0x18E3);
    canvas.setCursor(5, 122);
    if (isReading) {
        int percent = (maxScrollPos > 0) ? (scrollPos * 100 / maxScrollPos) : 0;
        int lIdx = (engine.linkCount > 0) ? selectedLink + 1 : 0;
        canvas.printf("SCR:%d%% | L:%d/%d | [B]/[L]-Books", percent, lIdx, engine.linkCount);
    } else {
        canvas.printf("[ENT] Search/URL | [L] Books | [OPT] Menu");
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
        canvas.setCursor(10, 20); canvas.setTextColor(YELLOW, BLACK); canvas.print(prompt);
        canvas.setCursor(15, 58); canvas.setTextColor(WHITE, BLACK); canvas.print(text + "_");
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

void loadWebPage();

void saveBookmark(String url) {
    prefs.begin("bookmarks", false);
    for (int i = 0; i < 5; i++) {
        String key = "b" + String(i);
        if (prefs.getString(key.c_str(), "") == "") {
            prefs.putString(key.c_str(), url);
            sysStatus = "SAVED TO B" + String(i+1);
            break;
        }
    }
    prefs.end();
}

void openBookmarksMenu() {
    waitRelease();
    int sel = 0;
    String bks[5];
    prefs.begin("bookmarks", true);
    for (int i = 0; i < 5; i++) bks[i] = prefs.getString(("b" + String(i)).c_str(), "---");
    prefs.end();

    while (true) {
        M5Cardputer.update();
        canvas.fillSprite(BLACK);
        canvas.setCursor(10, 10); canvas.setTextColor(CYAN); canvas.println("BOOKMARKS (Del to clear):");
        
        for (int i = 0; i < 5; i++) {
            canvas.setCursor(10, 30 + (i * 15));
            if (sel == i) canvas.setTextColor(BLACK, WHITE); else canvas.setTextColor(WHITE, BLACK);
            canvas.printf("%d. %s", i + 1, bks[i].substring(0, 30).c_str());
        }
        canvas.pushSprite(0, 0);

        if (M5Cardputer.Keyboard.isKeyPressed(';')) { sel = (sel > 0) ? sel - 1 : 4; delay(150); }
        if (M5Cardputer.Keyboard.isKeyPressed('.')) { sel = (sel < 4) ? sel + 1 : 0; delay(150); }
        
        auto status = M5Cardputer.Keyboard.keysState();
        if (status.enter && bks[sel] != "---") {
            currentURL = bks[sel];
            waitRelease();
            loadWebPage();
            break;
        }
        if (status.del) {
            prefs.begin("bookmarks", false);
            prefs.remove(("b" + String(sel)).c_str());
            prefs.end();
            bks[sel] = "---";
            delay(150);
        }
        if (M5Cardputer.Keyboard.isKeyPressed('`')) { waitRelease(); break; }
        delay(5);
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
    selectedLink = 0; 
    
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
        int lineHeight = siteZoom * 12; 
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
    prefs.begin("wifi-config", true);
    for (int i = 0; i < 3; i++) {
        ssids[i] = prefs.getString(("s" + String(i)).c_str(), "");
        passs[i] = prefs.getString(("p" + String(i)).c_str(), "");
    }
    prefs.end();

    while (true) {
        M5Cardputer.update();
        canvas.startWrite();
        canvas.fillSprite(BLACK);
        canvas.setCursor(10, 10); canvas.setTextColor(CYAN, BLACK); canvas.println("NET MANAGER");
        for (int i = 0; i < 3; i++) {
            canvas.setCursor(10, 35 + (i * 20));
            if (sel == i) canvas.setTextColor(BLACK, WHITE); else canvas.setTextColor(WHITE, BLACK);
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
                prefs.begin("wifi-config", false);
                prefs.putString(("s" + String(sel)).c_str(), s);
                prefs.putString(("p" + String(sel)).c_str(), p);
                prefs.end();
                WiFi.disconnect(); WiFi.begin(s.c_str(), p.c_str());
            }
            waitRelease(); break;
        }
        if (status.tab || M5Cardputer.Keyboard.isKeyPressed('`')) { waitRelease(); break; }
        delay(1);
    }
}

void savePageToTXT() {
    if (SD.cardType() == CARD_NONE) {
        sysStatus = "NO SD CARD";
        drawUI();
        return;
    }
    if (!SD.exists("/saved_links")) {
        SD.mkdir("/saved_links");
    }
    String filename = "/saved_links/page_" + String(millis()) + ".txt";
    File txtFile = SD.open(filename, FILE_WRITE);

    if (!txtFile) {
        sysStatus = "WRITE FAIL";
        drawUI();
        return;
    }
    txtFile.println("URL: " + currentURL);
    txtFile.println("====================");
    for (int i = 0; i < displayLineCount; i++) {
        txtFile.println(displayLines[i]);
    }
    txtFile.close();
    sysStatus = "SAVED TO TXT";
    drawUI();
}

void viewTextFile(String path) {
    File f = SD.open(path);
    if(!f) return;
    
    for(int i=0; i<100; i++) displayLines[i]="";
    displayLineCount = 0;
    
    while(f.available() && displayLineCount < 99) {
        String line = f.readStringUntil('\n');
        line.replace("\r", "");
        displayLines[displayLineCount++] = line;
    }
    f.close();
    
    currentURL = "file://" + path;
    sysStatus = "LOCAL FILE";
    isReading = true;
    scrollPos = 0;
    loadPercent = 100;
    maxScrollPos = (displayLineCount + 5) * (siteZoom * 12) / 2;
    engine.linkCount = 0; 
    drawUI();
}

void viewDirectory(String path) {
    if (SD.cardType() == CARD_NONE) {
        sysStatus = "NO SD"; drawUI(); return;
    }
    if (!SD.exists(path)) SD.mkdir(path);
    
    File root = SD.open(path);
    if (!root || !root.isDirectory()) return;

    String files[50];
    int fileCount = 0;
    File file = root.openNextFile();
    while(file && fileCount < 50){
        files[fileCount++] = String(file.name());
        file = root.openNextFile();
    }
    
    int fileSel = 0;
    int scroll = 0;
    while(true) {
        M5Cardputer.update();
        canvas.fillSprite(BLACK);
        canvas.setCursor(5, 5); canvas.setTextColor(CYAN, BLACK); 
        canvas.printf("DIR: %s", path.c_str());
        
        if (fileCount == 0) {
            canvas.setCursor(5, 25); canvas.setTextColor(WHITE, BLACK);
            canvas.println("Empty directory");
        } else {
            for(int i = 0; i < 5; i++) {
                int idx = scroll + i;
                if(idx < fileCount) {
                    canvas.setCursor(5, 25 + (i * 20));
                    if(fileSel == idx) canvas.setTextColor(BLACK, WHITE); else canvas.setTextColor(WHITE, BLACK);
                    canvas.println(files[idx]);
                }
            }
        }
        canvas.pushSprite(0,0);
        
        if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
            if(fileSel > 0) fileSel--; 
            if(fileSel < scroll) scroll = fileSel;
            delay(150); 
        }
        if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
            if(fileSel < fileCount - 1) fileSel++; 
            if(fileSel >= scroll + 5) scroll = fileSel - 4;
            delay(150); 
        }
        
        auto status = M5Cardputer.Keyboard.keysState();
        if (status.enter && fileCount > 0) {
            String selectedFile = path + (path.endsWith("/") ? "" : "/") + files[fileSel];
            File check = SD.open(selectedFile);
            bool isDir = check.isDirectory();
            check.close();
            
            if(isDir) {
                waitRelease();
                viewDirectory(selectedFile);
            } else if (selectedFile.endsWith(".txt")) {
                waitRelease();
                viewTextFile(selectedFile);
                return; 
            }
        }
        if (M5Cardputer.Keyboard.isKeyPressed('`')) { waitRelease(); break; }
        delay(5);
    }
}

void openMainMenu() {
    waitRelease();
    int sel = 0;
    while(true) {
        M5Cardputer.update();
        canvas.fillSprite(BLACK);
        canvas.setCursor(10, 10); canvas.setTextColor(CYAN, BLACK); canvas.println("MAIN MENU");
        
        canvas.setCursor(10, 40);
        if(sel == 0) canvas.setTextColor(BLACK, WHITE); else canvas.setTextColor(WHITE, BLACK);
        canvas.println("1. File System (/)");
        
        canvas.setCursor(10, 65);
        if(sel == 1) canvas.setTextColor(BLACK, WHITE); else canvas.setTextColor(WHITE, BLACK);
        canvas.println("2. Saved Links (/saved_links)");

        canvas.pushSprite(0,0);
        
        if (M5Cardputer.Keyboard.isKeyPressed(';')) { sel = 0; delay(150); }
        if (M5Cardputer.Keyboard.isKeyPressed('.')) { sel = 1; delay(150); }
        
        auto status = M5Cardputer.Keyboard.keysState();
        if (status.enter) {
            waitRelease();
            if (sel == 0) viewDirectory("/");
            else if (sel == 1) viewDirectory("/saved_links");
            break;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('`')) { waitRelease(); break; }
        delay(5);
    }
}

void setup() {
    Serial.begin(115200); 
    SPIClass* sd_spi = new SPIClass(HSPI);
    sd_spi->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, *sd_spi)) {
        Serial.println("SD Card Mount Failed");
        sysStatus = "NO SD";
    } else {
        Serial.println("SD Card Mount Success");
        sysStatus = "SD OK";
    }
    
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Power.begin();
    canvas.setColorDepth(8);
    canvas.createSprite(240, 135);

    canvas.setFont(&fonts::efontCN_12);

    canvas.fillSprite(BLACK);
    canvas.setTextColor(WHITE, BLACK);
    canvas.setTextSize(2);
    canvas.drawCenterString(APP_NAME, 120, 30);
    canvas.drawLine(40, 65, 200, 65, CYAN);
    
    canvas.setTextSize(1);
    canvas.setTextColor(CYAN, BLACK);
    canvas.drawCenterString("v" + APP_VERSION, 120, 75);
    
    canvas.setTextColor(0xBDD7, BLACK);
    canvas.drawCenterString("by " + String(AUTHOR_NAME), 120, 95);
    
    canvas.setTextColor(ORANGE, BLACK);
    canvas.drawCenterString("SAVING PAGES AND FILE SYSTEM", 120, 115); 

    canvas.pushSprite(0, 0);
    delay(1500); 

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setDNS(IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
    
    prefs.begin("wifi-config", true);
    for (int i = 0; i < 3; i++) {
        ssids[i] = prefs.getString(("s" + String(i)).c_str(), "");
        passs[i] = prefs.getString(("p" + String(i)).c_str(), "");
    }
    prefs.end();

    for (int i = 0; i < 3; i++) {
        if (ssids[i] != "") {
            WiFi.begin(ssids[i].c_str(), passs[i].c_str());
            break;
        }
    }
    
    currentURL = "https://ru.wikipedia.org";
    engine.onLineReady = onLineReady;
}

void saveScreenshot() {
    if (SD.cardType() == CARD_NONE) {
        sysStatus = "NO SD CARD";
        drawUI();
        return;
    }

    if (!SD.exists("/screenshots")) {
        if (!SD.mkdir("/screenshots")) {
            sysStatus = "FOLDER FAIL";
            drawUI();
            return;
        }
    }

    String filename = "/screenshots/paper_" + String(millis()) + ".bmp";
    File bmpFile = SD.open(filename, FILE_WRITE);

    if (!bmpFile) {
        sysStatus = "WRITE FAIL";
        drawUI();
        return;
    }

    int width = 240;
    int height = 135;
    
    uint8_t bmp_header[54] = { 0 };
    bmp_header[0] = 'B'; bmp_header[1] = 'M';
    uint32_t fileSize = 54 + width * height * 3;
    bmp_header[2] = (uint8_t)(fileSize);
    bmp_header[3] = (uint8_t)(fileSize >> 8);
    bmp_header[4] = (uint8_t)(fileSize >> 16);
    bmp_header[5] = (uint8_t)(fileSize >> 24);
    bmp_header[10] = 54;
    bmp_header[14] = 40;
    bmp_header[18] = (uint8_t)(width);
    bmp_header[19] = (uint8_t)(width >> 8);
    bmp_header[22] = (uint8_t)(height);
    bmp_header[23] = (uint8_t)(height >> 8);
    bmp_header[26] = 1;
    bmp_header[28] = 24;

    bmpFile.write(bmp_header, 54);

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            uint16_t color565 = canvas.readPixel(x, y);
            
            uint8_t r = (color565 >> 8) & 0xF8;
            uint8_t g = (color565 >> 3) & 0xFC;
            uint8_t b = (color565 << 3) & 0xF8;
            
            bmpFile.write(b);
            bmpFile.write(g);
            bmpFile.write(r);
        }
    }

    bmpFile.close();
    sysStatus = "SCREENSHOT OK";
    drawUI();
}

void loop() {
    bool isTyping = false;
    if (M5Cardputer.Keyboard.isKeyPressed('s') && !isTyping) {
        saveScreenshot();
        delay(300);
    }
    if (millis() > targetTime) {
        targetTime = millis() + 100;
        M5Cardputer.update();
        drawUI();
        auto status = M5Cardputer.Keyboard.keysState();
        
        if (status.opt) {
            openMainMenu();
        }
        
        if (isReading) {
            if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
                if (scrollPos > 0) scrollPos -= 4;
                if (scrollPos < 0) scrollPos = 0;
            }
            if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
                scrollPos += 4;
                if (scrollPos > maxScrollPos) scrollPos = maxScrollPos;
            }
            
            if (M5Cardputer.Keyboard.isKeyPressed(',')) { 
                if (selectedLink > 0) selectedLink--;
                delay(150); 
            }
            if (M5Cardputer.Keyboard.isKeyPressed('/')) { 
                if (selectedLink < engine.linkCount - 1) selectedLink++;
                delay(150);
            }
            
            if (M5Cardputer.Keyboard.isKeyPressed('b')) { 
                saveBookmark(currentURL); 
                delay(200); 
            }
            if (M5Cardputer.Keyboard.isKeyPressed('l')) { 
                openBookmarksMenu(); 
            }
            
            if (M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D')) {
                savePageToTXT();
                delay(200);
            }

            if (status.enter && engine.linkCount > 0) {
                String nextUrl = engine.linkURLs[selectedLink];
                if (nextUrl != "" && !nextUrl.startsWith("javascript:")) {
                    if (historyCount < 10) urlHistory[historyCount++] = currentURL;
                    else {
                        for(int i = 1; i < 10; i++) urlHistory[i-1] = urlHistory[i];
                        urlHistory[9] = currentURL;
                    }
                    currentURL = resolveURL(currentURL, nextUrl);
                    waitRelease();
                    loadWebPage();
                }
            }
            
            if (status.del) { 
                if (historyCount > 0) {
                    historyCount--;
                    currentURL = urlHistory[historyCount];
                    waitRelease();
                    loadWebPage();
                }
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
            if (M5Cardputer.Keyboard.isKeyPressed('l')) openBookmarksMenu();
            
            if (status.enter) {
                if (WiFi.status() != WL_CONNECTED) openWiFiMenu();
                else {
                    String url = inputText("Search or URL:");
                    if (url != "") {
                        if (url.indexOf('.') == -1 || url.indexOf(' ') != -1) {
                            url.replace(" ", "+");
                            url = "https://www.google.com/search?q=" + url;
                        }
                        else if (!url.startsWith("http")) url = "https://" + url;
                        
                        historyCount = 0; 
                        currentURL = url;
                        loadWebPage();
                    }
                }
            }
        }
    }
}
