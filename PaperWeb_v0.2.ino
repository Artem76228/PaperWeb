#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

// --- ИНФОРМАЦИЯ О ПРОЕКТЕ ---
const String APP_NAME = "PaperWeb";
const String APP_VERSION = "0.2"; // Обновили версию
const String AUTHOR = "Artem76228";
// ----------------------------

Preferences prefs;
String ssids[3], passs[3];
String currentURL = "https://google.com";
String pageContent = ""; 
String sysStatus = "READY"; 
int scrollPos = 0; // Теперь это смещение в строках
float siteZoom = 1.0; 
int loadPercent = 0;
bool isReading = false;

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

void drawUI() {
    canvas.startWrite();
    canvas.fillSprite(BLACK);
    
    if (!isReading) {
        if (WiFi.status() == WL_CONNECTED) pageContent = "WELCOME TO PAPERWEB v0.2. [ENT] - SEARCH | [TAB] - WIFI";
        else pageContent = "WIFI DISCONNECTED! PRESS [TAB] TO CONNECT";
    }

    canvas.setTextColor(WHITE);
    canvas.setTextSize(siteZoom);
    canvas.setCursor(0, 22 - (scrollPos * 2)); 
    canvas.setTextWrap(true); 
    canvas.print(pageContent);

    // Хедер
    uint16_t headerCol = (WiFi.status() != WL_CONNECTED) ? 0x8000 : (loadPercent > 0 ? 0xFBE0 : (isReading ? 0x2104 : 0x0400));
    canvas.fillRect(0, 0, 240, 18, headerCol); 
    
    // --- ЗОНА ЗНАЧКОВ (СПРАВА) ---
    int bat = map(M5Cardputer.Power.getBatteryVoltage(), 3300, 4200, 0, 100);
    bat = constrain(bat, 0, 100);

    drawBatteryUI(220, 5, bat); // Самый край
    canvas.setTextColor(WHITE);
    canvas.setTextSize(1);
    canvas.setCursor(195, 5);
    canvas.printf("%d%%", bat); // Проценты
    drawRSSI(175, 5); // Связь левее процентов

    // --- ТЕКСТ СТАТУСА (СЛЕВА С ЗАЩИТОЙ) ---
    canvas.setClipRect(0, 0, 170, 18); // Текст не зайдет дальше 170 пикселя
    canvas.setCursor(5, 5);
    canvas.printf("%s v%s | %s", APP_NAME.c_str(), APP_VERSION.c_str(), sysStatus.c_str());
    canvas.clearClipRect(); 

    // Прогресс-бар
    if (loadPercent > 0 && loadPercent < 100) {
        canvas.fillRect(0, 18, map(loadPercent, 0, 100, 0, 240), 3, CYAN);
    }

    // Футер
    canvas.fillRect(0, 118, 240, 17, 0x18E3); 
    canvas.setCursor(5, 122);
    if (isReading) canvas.printf("SCROLL: %d | [`] EXIT | Z:%.1f", scrollPos, siteZoom);
    else canvas.printf("[ENT] URL | [TAB] WiFi");

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

void loadWebPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    isReading = true;
    pageContent = ""; 
    sysStatus = "CONNECTING..."; drawUI();
    
    WiFiClientSecure *client = new WiFiClientSecure;
    if(client) {
        client->setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        if (http.begin(*client, currentURL)) {
            int httpCode = http.GET();
            sysStatus = "GET CODE: " + String(httpCode); drawUI();
            
            if (httpCode == 200) {
                String raw = http.getString();
                String filtered = "";
                filtered.reserve(8000); // Резервируем память для скорости
                bool inTag = false;
                
                for (int i = 0; i < (int)raw.length(); i++) {
                    if (i % 500 == 0) { 
                        loadPercent = map(i, 0, raw.length(), 0, 100); 
                        sysStatus = "PARSING " + String(loadPercent) + "%";
                        drawUI(); 
                    }
                    if (raw[i] == '<') { inTag = true; filtered += " "; }
                    else if (raw[i] == '>') inTag = false;
                    else if (!inTag && (uint8_t)raw[i] >= 32) filtered += raw[i];
                    
                    if (filtered.length() > 8000) break; // Увеличили объем страницы
                }
                pageContent = filtered;
                sysStatus = "DONE";
            } else { pageContent = "HTTP ERROR: " + String(httpCode); sysStatus = "FAILED"; }
            http.end();
        }
        delete client;
    }
    loadPercent = 0; scrollPos = 0;
}

// ... (openWiFiMenu и setup остаются без изменений) ...
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
    canvas.setTextSize(3);
    canvas.drawCenterString(APP_NAME, 120, 30);
    canvas.drawLine(40, 65, 200, 65, CYAN);
    canvas.setTextSize(1);
    canvas.setTextColor(CYAN);
    canvas.drawCenterString("v" + APP_VERSION, 120, 75);
    canvas.setTextColor(0xBDD7);
    canvas.drawCenterString("by " + AUTHOR, 120, 100);
    canvas.pushSprite(0, 0);
    delay(1500); // Ускорили заставку для динамики

    prefs.begin("wifi-config", false);
    for (int i = 0; i < 3; i++) {
        ssids[i] = prefs.getString(("s" + String(i)).c_str(), "");
        passs[i] = prefs.getString(("p" + String(i)).c_str(), "");
    }
    if (ssids[0] != "") WiFi.begin(ssids[0].c_str(), passs[0].c_str());
}

void loop() {
    if (millis() > targetTime) {
        targetTime = millis() + 30; // Уменьшили частоту до ~33 FPS для более вязкого скролла
        M5Cardputer.update();
        drawUI();
        auto status = M5Cardputer.Keyboard.keysState();
        if (isReading) {
            // Медленный скролл по 3 пикселя за раз
            if (M5Cardputer.Keyboard.isKeyPressed(';')) { if (scrollPos > 0) scrollPos -= 3; }
            if (M5Cardputer.Keyboard.isKeyPressed('.')) { scrollPos += 3; }
            
            if (M5Cardputer.Keyboard.isKeyPressed('=')) { siteZoom += 0.1; if (siteZoom > 3.0) siteZoom = 3.0; delay(100); }
            if (M5Cardputer.Keyboard.isKeyPressed('-')) { siteZoom -= 0.1; if (siteZoom < 0.5) siteZoom = 0.5; delay(100); }
            if (M5Cardputer.Keyboard.isKeyPressed('`')) { isReading = false; waitRelease(); }
        } else {
            if (status.tab) openWiFiMenu();
            if (status.enter) {
                if (WiFi.status() != WL_CONNECTED) openWiFiMenu();
                else {
                    String url = inputText("URL:");
                    if (url != "") {
                        // Умный поиск (если нет точки в запросе - идем в Google)
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