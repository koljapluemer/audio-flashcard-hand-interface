/*
 * 008_esp_s3_zero_ble_buttons
 * ---------------------------
 * Three-button BLE remote (based on 004) for the Waveshare ESP32-S3-Zero,
 * running from a LiPo through a 5 V battery shield.
 *
 * Wiring:
 *   LiPo +/-               -> battery shield BAT+/BAT-
 *   shield OUT+ (5 V)      -> S3-Zero 5V
 *   shield OUT- / GND      -> S3-Zero GND
 *   Button 1  GPIO1 <-> GND
 *   Button 2  GPIO2 <-> GND
 *   Button 3  GPIO3 <-> GND
 *   (internal pull-ups, no resistors; a pressed button reads LOW)
 *
 * BLE contract (must match code/basic_cordo_android_app/www/index.html):
 *   Device name    : "SentenceRemote" (the app doesn't check it)
 *   Service        : 4fafc201-1fb5-459e-8fcc-c5c9c331914b  (the app scans for this)
 *   Characteristic : beb5483e-36e1-4688-b7f5-ea07361b26a8  notify, device -> app
 *   Each confirmed press notifies the plain string "Button N Pressed"
 *   (N = 1..3, no newline). The app currently reacts to buttons 1 and 2 and
 *   ignores 3.
 *
 * Battery-related behaviour:
 *   - CPU at 80 MHz (plenty for BLE + 3 GPIOs).
 *   - Onboard RGB LED (WS2812, GPIO21) shows status with short, dim blinks
 *     instead of being lit all the time:
 *       blue blink every 2 s .... advertising, nobody connected
 *       green blink every 5 s ... connected
 *       white flash ............. button press
 *   - If nobody connects and no button is pressed for IDLE_SLEEP_MS, the chip
 *     goes into deep sleep. Pressing any button wakes it (it reboots and
 *     starts advertising again; that wake-up press itself is not sent).
 *     Never sleeps while a client is connected. Set IDLE_SLEEP_MS to 0 to
 *     disable.
 *   Note: the battery shield's own regulator keeps drawing current in deep
 *   sleep; to really turn the device off, use the shield's switch if it has one.
 *
 * Build / flash (native USB, so CDC on boot is needed to see Serial):
 *   arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" code/scripts/008_esp_s3_zero_ble_buttons
 *   arduino-cli upload  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" --port /dev/ttyACM0 code/scripts/008_esp_s3_zero_ble_buttons
 * If the port doesn't show up: hold BOOT, tap RESET, release BOOT, upload.
 * BLE library is the one bundled with the ESP32 core; nothing to install.
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

#define DEVICE_NAME      "SentenceRemote"
#define SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ---- Buttons -------------------------------------------------------------
constexpr uint8_t  BTN_PINS[]  = { 1, 2, 3 };
constexpr uint8_t  NUM_BTNS    = sizeof(BTN_PINS) / sizeof(BTN_PINS[0]);
constexpr uint16_t DEBOUNCE_MS = 25;

// ---- Status LED (onboard WS2812) -------------------------------------------
constexpr uint8_t  LED_PIN        = 21;
constexpr uint8_t  LED_LEVEL      = 12;     // 0..255, kept dim to save battery
// rgbLedWrite() assumes GRB order. Some S3-Zero batches have red/green
// swapped; if "connected" shows red instead of green, flip this.
constexpr bool     LED_SWAP_RG    = false;
constexpr uint16_t LED_BLINK_MS   = 40;
constexpr uint32_t LED_ADV_PERIOD = 2000;
constexpr uint32_t LED_CON_PERIOD = 5000;

// ---- Power -----------------------------------------------------------------
constexpr uint32_t CPU_MHZ       = 80;
constexpr uint32_t IDLE_SLEEP_MS = 5UL * 60UL * 1000UL;   // 0 = never sleep

// ---- State -------------------------------------------------------------------
static uint8_t  stableState[NUM_BTNS];
static uint8_t  lastReading[NUM_BTNS];
static uint32_t lastChangeMs[NUM_BTNS];

static BLECharacteristic *txChar = nullptr;
static volatile bool clientConnected = false;
static uint32_t lastActivityMs = 0;   // last press or disconnect
static uint32_t ledOffAtMs = 0;       // 0 = LED not currently on
static uint32_t lastBlinkMs = 0;

// ---- LED -----------------------------------------------------------------------
static void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  if (LED_SWAP_RG) { uint8_t t = r; r = g; g = t; }
  rgbLedWrite(LED_PIN, r, g, b);
}

static void ledFlash(uint8_t r, uint8_t g, uint8_t b) {
  ledSet(r, g, b);
  ledOffAtMs = millis() + LED_BLINK_MS;
  if (ledOffAtMs == 0) ledOffAtMs = 1;
}

static void ledTick(uint32_t now) {
  if (ledOffAtMs && (int32_t)(now - ledOffAtMs) >= 0) {
    ledSet(0, 0, 0);
    ledOffAtMs = 0;
  }
  const uint32_t period = clientConnected ? LED_CON_PERIOD : LED_ADV_PERIOD;
  if (now - lastBlinkMs >= period) {
    lastBlinkMs = now;
    if (clientConnected) ledFlash(0, LED_LEVEL, 0);
    else                 ledFlash(0, 0, LED_LEVEL);
  }
}

// ---- BLE -------------------------------------------------------------------------
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    clientConnected = true;
    Serial.println("BLE client connected");
  }
  void onDisconnect(BLEServer *server) override {
    clientConnected = false;
    lastActivityMs = millis();   // give the user a full idle window to reconnect
    Serial.println("BLE client disconnected, re-advertising");
    server->startAdvertising();
  }
};

static void bleBegin() {
  BLEDevice::init(DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);
  txChar = service->createCharacteristic(
      CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());
  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Advertising as \"" DEVICE_NAME "\"");
}

static void sendButton(uint8_t i) {
  char msg[24];
  snprintf(msg, sizeof(msg), "Button %u Pressed", i + 1);
  Serial.printf("PRESS GPIO%u -> \"%s\"%s\n", BTN_PINS[i], msg,
                clientConnected ? "" : "  (not connected, dropped)");
  ledFlash(LED_LEVEL, LED_LEVEL, LED_LEVEL);
  if (!clientConnected || txChar == nullptr) return;
  txChar->setValue((uint8_t *)msg, strlen(msg));
  txChar->notify();
}

// ---- Deep sleep --------------------------------------------------------------------
static void goToSleep() {
  Serial.println("idle, going to deep sleep (press any button to wake)");
  Serial.flush();
  ledSet(0, 0, 0);
  // Don't fall asleep while a button is still held: it would wake us at once.
  for (uint8_t i = 0; i < NUM_BTNS; i++) while (digitalRead(BTN_PINS[i]) == LOW) delay(10);

  uint64_t mask = 0;
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    const gpio_num_t pin = (gpio_num_t)BTN_PINS[i];
    mask |= 1ULL << BTN_PINS[i];
    // Keep the pull-ups alive in deep sleep (RTC domain), else the pins float.
    rtc_gpio_pullup_en(pin);
    rtc_gpio_pulldown_dis(pin);
  }
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ---- Main --------------------------------------------------------------------------
void setup() {
  setCpuFrequencyMhz(CPU_MHZ);
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("008_esp_s3_zero_ble_buttons: BLE button remote on GPIO 1, 2, 3");
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) Serial.println("woke from deep sleep (button)");

  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    // After an ext1 wake the pins are still muxed to the RTC domain; hand
    // them back to the digital GPIO matrix before using them normally.
    rtc_gpio_deinit((gpio_num_t)BTN_PINS[i]);
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    // A button still held from the wake-up press counts as already "down",
    // so it isn't sent as a press when released and pressed again.
    stableState[i]  = digitalRead(BTN_PINS[i]);
    lastReading[i]  = stableState[i];
    lastChangeMs[i] = 0;
  }

  ledSet(0, 0, 0);
  bleBegin();
  lastActivityMs = millis();
}

void loop() {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    const uint8_t raw = digitalRead(BTN_PINS[i]);
    if (raw != lastReading[i]) {
      lastReading[i]  = raw;
      lastChangeMs[i] = now;
    }
    if ((now - lastChangeMs[i]) >= DEBOUNCE_MS && raw != stableState[i]) {
      stableState[i] = raw;
      if (raw == LOW) {   // HIGH -> LOW = a fresh press
        lastActivityMs = now;
        sendButton(i);
      }
    }
  }

  ledTick(now);

  if (IDLE_SLEEP_MS && !clientConnected && now - lastActivityMs >= IDLE_SLEEP_MS) goToSleep();

  delay(5);
}
