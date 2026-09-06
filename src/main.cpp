#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <GxEPD2_BW.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>

#include "qrcode.h"

// 所有需要按实际硬件或部署环境调整的参数集中在这里，避免魔法数字散落在业务逻辑中。
namespace config {
// 同一组凭据用于连接现有 Wi-Fi；连接失败时也用于创建应急 AP 热点。
// WPA2 热点密码不得少于 8 个字符。
constexpr char kWifiSsid[] = "hacker";
constexpr char kWifiPassword[] = "12345678";

// GDEP015OC1 使用 ESP32 默认硬件 SPI。旧硬件没有连接 RST 和 BUSY，故设为 -1。
// 若局刷不稳定或屏幕偶尔无响应，应优先补接这两个信号并在此填写 GPIO。
constexpr uint8_t kDisplayCsPin = SS;
constexpr uint8_t kDisplayDcPin = 17;
constexpr int8_t kDisplayResetPin = -1;
constexpr int8_t kDisplayBusyPin = -1;

constexpr uint16_t kHttpPort = 80;
constexpr uint16_t kWebSocketPort = 81;
constexpr uint32_t kWifiConnectTimeoutMs = 6000;
// 电子墨水屏连续局刷会积累残影；完成指定次数的局刷后执行一次全刷。
constexpr uint8_t kPartialRefreshesBeforeFull = 7;
// 1 bit 表示一个像素，因此 200 × 200 的单色图固定占用 5,000 bytes。
constexpr size_t kBitmapSize =
    GxEPD2_154::WIDTH * GxEPD2_154::HEIGHT / 8;
}  // namespace config

// ESP32 内存足够容纳整个 200 × 200 帧缓冲区，因此 page height 使用屏幕完整高度。
// 构造参数依次为 CS、DC、RST、BUSY。
GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display(GxEPD2_154(
    config::kDisplayCsPin, config::kDisplayDcPin, config::kDisplayResetPin,
    config::kDisplayBusyPin));
QRcode qrcode(display);
AsyncWebServer server(config::kHttpPort);
WebSocketsServer webSocket(config::kWebSocketPort);

uint8_t partialRefreshCount = 0;

// 用局部刷新显示短暂状态信息，避免联网阶段产生不必要的多次全刷。
void showMessage(const String &message) {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 18);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.println(message);
    display.displayWindow(0, 0, display.width(), display.height());
}

void initializeDisplay() {
    // 上电后先执行一次全刷，使控制器中的新旧图像缓冲区处于已知状态。
    display.init();
    display.fillScreen(GxEPD_WHITE);
    display.display();
}

IPAddress connectNetwork() {
    showMessage("Connecting to WiFi...");
    Serial.printf("Connecting to WiFi '%s'\n", config::kWifiSsid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config::kWifiSsid, config::kWifiPassword);
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startedAt < config::kWifiConnectTimeoutMs) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected; IP address: %s\n",
                      WiFi.localIP().toString().c_str());
        return WiFi.localIP();
    }

    // STA 超时后切换到 AP，使没有路由器时仍能直接连接设备使用画板。
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(config::kWifiSsid, config::kWifiPassword)) {
        Serial.println("Failed to start access point");
        showMessage("WiFi setup failed");
        while (true) delay(1000);
    }

    Serial.printf("Access point started; SSID: %s; IP address: %s\n",
                  config::kWifiSsid, WiFi.softAPIP().toString().c_str());
    return WiFi.softAPIP();
}

void showAddressQrCode(const IPAddress &address) {
    // 必须包含协议头，否则部分扫码软件只会显示文本，不会打开浏览器。
    qrcode.create("http://" + address.toString());
    display.displayWindow(0, 0, display.width(), display.height());
}

void renderBitmap(const uint8_t *bitmap) {
    // 浏览器采用 MSB-first 行优先格式：每字节的 bit 7 对应最左侧像素。
    // drawBitmap() 中值为 1 的位使用 GxEPD_BLACK 绘制，背景预先填白。
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, bitmap, display.width(), display.height(),
                       GxEPD_BLACK);

    if (++partialRefreshCount <= config::kPartialRefreshesBeforeFull) {
        display.displayWindow(0, 0, display.width(), display.height());
        return;
    }

    // 第 8 张图进行全刷，并从零开始下一轮局刷计数，以抑制残影累积。
    partialRefreshCount = 0;
    display.display();
}

void handleWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload,
                          size_t length) {
    // 当前协议只接受一条完整的二进制消息，不处理文本命令或分片消息。
    // 浏览器端一次 send() 发送 5,000 bytes，正常情况下会触发 WStype_BIN。
    switch (type) {
        case WStype_CONNECTED: {
            const IPAddress remoteIp = webSocket.remoteIP(clientId);
            Serial.printf("WebSocket client %u connected from %s\n", clientId,
                          remoteIp.toString().c_str());
            break;
        }
        case WStype_DISCONNECTED:
            Serial.printf("WebSocket client %u disconnected\n", clientId);
            break;
        case WStype_BIN:
            if (length != config::kBitmapSize) {
                Serial.printf(
                    "Client %u sent %u bitmap bytes; expected %u; ignored\n",
                    clientId, static_cast<unsigned>(length),
                    static_cast<unsigned>(config::kBitmapSize));
                break;
            }
            renderBitmap(payload);
            break;
        case WStype_TEXT:
            Serial.printf("Client %u sent an unsupported text message\n",
                          clientId);
            break;
        case WStype_ERROR:
            Serial.printf("WebSocket error for client %u\n", clientId);
            break;
        case WStype_PING:
        case WStype_PONG:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

void startWebServices() {
    webSocket.onEvent(handleWebSocketEvent);
    webSocket.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        // 显式指定入口，避免 serveStatic() 的默认文件行为随库版本变化。
        request->send(SPIFFS, "/www/index.html", "text/html");
    });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/www/img/avatar.jpg", "image/jpeg");
    });
    server.serveStatic("/", SPIFFS, "/www/");
    server.begin();

    Serial.printf("HTTP server listening on port %u; WebSocket on port %u\n",
                  config::kHttpPort, config::kWebSocketPort);
}

void setup() {
    Serial.begin(115200);

    if (!SPIFFS.begin()) {
        // 不自动格式化：格式化会掩盖网页镜像未烧录的问题，并删除已有文件。
        Serial.println("SPIFFS initialization failed");
        while (true) delay(1000);
    }

    initializeDisplay();
    const IPAddress address = connectNetwork();
    showAddressQrCode(address);
    startWebServices();
}

// arduinoWebSockets 需要在主循环中持续轮询；HTTP 服务自身由异步任务处理。
void loop() { webSocket.loop(); }
