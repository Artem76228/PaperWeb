#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

class PaperEngine {
public:
    String filteredText;
    int bytesDownloaded = 0;
    int totalBytes = 0;
    bool isDownloading = false;

    bool fetch(String url, int maxChars = 8000, int timeoutMs = 15000, void (*progressCallback)(int, int) = nullptr) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(timeoutMs / 1000);
        
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(timeoutMs);

        if (!http.begin(client, url)) return false;

        http.addHeader("Accept-Encoding", "identity");
        http.addHeader("User-Agent", "PaperWeb/0.4");
        
        int httpCode = http.GET();
        if (httpCode != 200) {
            http.end();
            return false;
        }
        
        totalBytes = http.getSize();
        if (totalBytes <= 0) totalBytes = 1;
        
        isDownloading = true;
        
        WiFiClient* stream = http.getStreamPtr();
        filteredText = "";
        filteredText.reserve(maxChars);
        bytesDownloaded = 0;

        bool inTag = false;
        bool isSkipping = false;
        String currentTag = "";
        bool lastAddedNewline = false;
        
        uint32_t startTime = millis();

        while (http.connected() && filteredText.length() < maxChars && millis() - startTime < timeoutMs) {
            if (ESP.getFreeHeap() < 15000) break;
            
            while (stream->available() && filteredText.length() < maxChars) {
                char c = stream->read();
                bytesDownloaded++;
                
                if (progressCallback && bytesDownloaded % 500 == 0) {
                    progressCallback(bytesDownloaded, totalBytes);
                }

                if (c == '<') { inTag = true; currentTag = ""; continue; }
                if (c == '>') { 
                    inTag = false;
                    currentTag.toLowerCase();
                    if (currentTag == "script" || currentTag == "style" || currentTag == "head" || currentTag == "noscript") isSkipping = true;
                    if (currentTag == "/script" || currentTag == "/style" || currentTag == "/head" || currentTag == "/noscript") isSkipping = false;
                    
                    // Добавляем перенос только если не было недавно
                    if (!isSkipping && (currentTag == "p" || currentTag == "div" || currentTag == "br" || 
                        currentTag == "h1" || currentTag == "h2" || currentTag == "h3" || currentTag == "li")) {
                        if (!lastAddedNewline && !filteredText.endsWith("\n")) {
                            filteredText += "\n";
                            lastAddedNewline = true;
                        }
                    }
                    continue;
                }

                if (!inTag && !isSkipping) {
                    // Убираем только явно мусорные символы
                    if (c == '{' || c == '}' || c == '[' || c == ']' || c == ';' || c == '=') continue;
                    
                    // UTF-8: сохраняем все байты (русские буквы в т.ч.)
                    if ((uint8_t)c >= 32 || c == '\n' || c == '\t') {
                        if (c == ' ' && filteredText.endsWith(" ")) continue;
                        filteredText += c;
                        if (c != ' ') lastAddedNewline = false;
                    }
                } else if (inTag) {
                    if (c != ' ' && currentTag.indexOf(' ') == -1) currentTag += c;
                }
            }
            delay(1);
        }
        
        isDownloading = false;
        cleanEntities();
        http.end();
        
        // Убираем множественные пустые строки
        while (filteredText.indexOf("\n\n\n") != -1) {
            filteredText.replace("\n\n\n", "\n\n");
        }
        
        return filteredText.length() > 0;
    }

private:
    void cleanEntities() {
        filteredText.replace("&nbsp;", " ");
        filteredText.replace("&quot;", "\"");
        filteredText.replace("&amp;", "&");
        filteredText.replace("&lt;", "<");
        filteredText.replace("&gt;", ">");
        filteredText.replace("&#39;", "'");
        filteredText.replace("&apos;", "'");
        filteredText.replace("&mdash;", "-");
        filteredText.replace("&ndash;", "-");
        
        // Числовые сущности (для кириллицы)
        int pos = 0;
        while ((pos = filteredText.indexOf("&#", pos)) != -1) {
            int end = filteredText.indexOf(';', pos);
            if (end != -1) {
                String numStr = filteredText.substring(pos + 2, end);
                char ch = (char)numStr.toInt();
                filteredText.replace(filteredText.substring(pos, end + 1), String(ch));
            }
            pos++;
        }
    }
};

PaperEngine engine;

const String APP_NAME = "PaperWeb";
const String APP_VERSION = "0.4";
const String AUTHOR = "Artem76228";

Preferences prefs;
String ssids[3], passs[3];
String currentURL = "https://ru.wikipedia.org";
String pageContent = ""; 
String sysStatus = "READY"; 
int scrollPos = 0; 
float siteZoom = 1.0; 
int loadPercent = 0;
bool isReading = false;
int maxScrollPos = 0;
String loadStage = "";

LGFX_Sprite canvas(&M5Cardputer.Display);
uint32_t targetTime = 0;

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

int getTextHeight() {
    canvas.setTextSize(siteZoom);
    int lineHeight = siteZoom * 8;
    int lines = 0;
    for (int i = 0; i < (int)pageContent.length(); i++) {
        if (pageContent[i] == '\n') lines++;
    }
    return (lines + 5) * lineHeight;
}

void drawUI() {
    canvas.startWrite();
    canvas.fillSprite(BLACK);
    
    if (!isReading) {
        if (WiFi.status() == WL_CONNECTED) pageContent = "WELCOME TO PAPERWEB v0.4\n[ENT] - SEARCH | [TAB] - WIFI";
        else pageContent = "WIFI DISCONNECTED!\nPRESS [TAB] TO CONNECT";
    }

    canvas.setTextColor(WHITE);
    canvas.setTextSize(siteZoom);
    canvas.setCursor(0, 22 - (scrollPos * 2)); 
    canvas.setTextWrap(true); 
    canvas.print(pageContent);

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
    
    loadStage = "CONNECTING";
    sysStatus = loadStage;
    loadPercent = 0;
    drawUI();
    delay(100);
    
    loadStage = "DOWNLOADING";
    sysStatus = loadStage;
    drawUI();
    
    bool ok = engine.fetch(currentURL, 8000, 15000, updateProgress);
    
    if (ok && engine.filteredText.length() > 0) {
        pageContent = engine.filteredText;
        int kbSize = engine.bytesDownloaded / 1024;
        sysStatus = "DONE " + String(kbSize) + "KB";
        loadPercent = 100;
        
        maxScrollPos = getTextHeight() / 2;
        if (maxScrollPos < 0) maxScrollPos = 0;
        
    } else {
        sysStatus = "FAIL";
        pageContent = "FAILED\n\n" + currentURL + "\n\nBytes: " + String(engine.bytesDownloaded);
        maxScrollPos = 0;
        loadPercent = 0;
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
    canvas.drawCenterString("powered by PaperEngine (TM)", 120, 115); 

    canvas.pushSprite(0, 0);
    delay(1500); 
    
    prefs.begin("wifi-config", false);
    for (int i = 0; i < 3; i++) {
        ssids[i] = prefs.getString(("s" + String(i)).c_str(), "");
        passs[i] = prefs.getString(("p" + String(i)).c_str(), "");
    }
    if (ssids[0] != "") WiFi.begin(ssids[0].c_str(), passs[0].c_str());
    
    // Устанавливаем русскую Википедию по умолчанию
    currentURL = "https://ru.wikipedia.org";
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
                maxScrollPos = getTextHeight() / 2;
                delay(100); 
            }
            if (M5Cardputer.Keyboard.isKeyPressed('-')) { 
                siteZoom -= 0.1; 
                if (siteZoom < 0.5) siteZoom = 0.5;
                maxScrollPos = getTextHeight() / 2;
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