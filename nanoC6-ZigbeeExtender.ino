/*
 * nanoC6-ZigbeeExtender.ino
 * 
 * Zigbee 3.0 Range Extender (Router) for M5Stack M5NanoC6 (ESP32-C6).
 *
 * Behavioral requirements:
 * 1. Boot / Not Connected: Flashes blue every 1s (500ms ON / 500ms OFF).
 * 2. On Connect: Solid blue for 2s, transitions to solid green for 3s, then turns off.
 * 3. Button Long Press (5s): Leaves Zigbee network, clears NVRAM/NVS, and reboots into pairing mode.
 * 4. Button Quick Click (< 1s): Forces ZCL/ZDO communication with Coordinator (0x0000) for mesh verification.
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee ZCZR (Coordinator/Router) mode must be selected in compile settings!"
#endif

#include <Arduino.h>
#include "Zigbee.h"
#include "esp_zigbee_core.h"
#include "zdo/esp_zigbee_zdo_command.h"

// Hardware pin definitions for M5NanoC6
#ifndef BLUE_LED_PIN
#define BLUE_LED_PIN 7
#endif

#ifndef RGB_LED_PWR_PIN
#define RGB_LED_PWR_PIN 19
#endif

#ifndef RGB_LED_DATA_PIN
#define RGB_LED_DATA_PIN 20
#endif

#ifndef BTN_PIN
#define BTN_PIN 9
#endif

#define ZIGBEE_EXTENDER_ENDPOINT 1
#define LED_BRIGHTNESS           120

// State machine definitions for connection and LED feedback
enum DeviceState {
  STATE_SEARCHING_NWK,
  STATE_CONNECTED_BLUE,
  STATE_CONNECTED_GREEN,
  STATE_OPERATIONAL
};

// Periodic heartbeat interval to keep Last Seen and Linkquality active in Zigbee2MQTT
#define HEARTBEAT_INTERVAL_MS    60000 // 60 seconds

DeviceState currentState = STATE_SEARCHING_NWK;
unsigned long stateTimer = 0;
unsigned long blinkTimer = 0;
bool blinkState = false;
unsigned long lastHeartbeatTimer = 0;

// Button state and debouncing timers
bool lastButtonState = HIGH;
unsigned long buttonPressStartTime = 0;
bool longPressTriggered = false;

// LED pulse duration for communication feedback
unsigned long pingFeedbackUntil = 0;

// Zigbee Range Extender endpoint instance
ZigbeeRangeExtender zbExtender(ZIGBEE_EXTENDER_ENDPOINT);

// LED helper functions
void setLeds(bool blueLedOn, uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(BLUE_LED_PIN, blueLedOn ? HIGH : LOW);
  rgbLedWrite(RGB_LED_DATA_PIN, r, g, b);
}

void turnLedsOff() {
  setLeds(false, 0, 0, 0);
}

// ZDO response callback for Coordinator communication test
static void activeEpResponseCb(esp_zb_zdp_status_t zdo_status, uint8_t ep_count, uint8_t *ep_id_list, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    Serial.printf("[ZDO] Coordinator communication OK! Active endpoints: %d\n", ep_count);
  } else {
    Serial.printf("[ZDO] Coordinator response received with status: 0x%02X\n", zdo_status);
  }
}

// Forces ZCL (Read Attribute) and ZDO communication with the Coordinator (0x0000).
// In Zigbee2MQTT / zigbee-herdsman, receiving this incoming frame updates the LQI (linkquality)
// and refreshes the 'last_seen' timestamp.
void heartbeatCoordinator(bool visualFeedback = false) {
  if (!Zigbee.connected()) {
    Serial.println("[Heartbeat] Device is not currently connected to a Zigbee network.");
    return;
  }

  if (visualFeedback) {
    // Quick green pulse (250ms) on button click
    setLeds(false, 0, LED_BRIGHTNESS, 0);
    pingFeedbackUntil = millis() + 250;
  }

  Serial.println("[Heartbeat] Sending ZCL Read Attribute (Basic Cluster) to Coordinator (0x0000)...");

  // 1. ZCL Read Attribute to Coordinator (cluster Basic, attribute ZCL_VERSION)
  // This frame triggers the 'deviceMessage' event in Zigbee2MQTT, updating linkquality and last_seen.
  uint16_t basicAttr = ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID;
  esp_zb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
  read_req.zcl_basic_cmd.dst_endpoint = 1;
  read_req.zcl_basic_cmd.src_endpoint = ZIGBEE_EXTENDER_ENDPOINT;
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;
  read_req.attr_number = 1;
  read_req.attr_field = &basicAttr;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t errRead = esp_zb_zcl_read_attr_cmd_req(&read_req);
  esp_zb_lock_release();

  if (errRead != ESP_OK) {
    Serial.printf("[Heartbeat] Failed to send ZCL Read: 0x%02X\n", errRead);
  } else {
    Serial.println("[Heartbeat] ZCL Read sent successfully!");
  }

  // 2. ZDO Active Endpoints request for mesh verification
  esp_zb_zdo_active_ep_req_param_t req;
  req.addr_of_interest = 0x0000;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zdo_active_ep_req(&req, activeEpResponseCb, NULL);
  esp_zb_lock_release();
}

// Callback for network-triggered device identification (e.g. ZHA / Z2M Identify feature)
void identifyCb(uint16_t time) {
  static bool idBlink = false;
  if (time == 0) {
    if (currentState == STATE_OPERATIONAL) {
      turnLedsOff();
    }
    return;
  }
  idBlink = !idBlink;
  if (idBlink) {
    setLeds(true, 0, 0, LED_BRIGHTNESS);
  } else {
    turnLedsOff();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  M5NanoC6 - Zigbee Range Extender (Router) ");
  Serial.println("==========================================");

  // Pin initialization
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, LOW);

  pinMode(RGB_LED_PWR_PIN, OUTPUT);
  digitalWrite(RGB_LED_PWR_PIN, HIGH); // Enable power to WS2812 RGB LED
  delay(10);
  rgbLedWrite(RGB_LED_DATA_PIN, 0, 0, 0);

  pinMode(BTN_PIN, INPUT_PULLUP);

  // Zigbee Range Extender endpoint configuration
  zbExtender.onIdentify(identifyCb);
  zbExtender.setManufacturerAndModel("M5Stack", "NanoC6-ZigbeeExtender");
  zbExtender.setPowerSource(ZB_POWER_SOURCE_MAINS);

  Serial.println("[Zigbee] Adding Range Extender endpoint to Zigbee Core...");
  Zigbee.addEndpoint(&zbExtender);

  // Configure Router mode with up to 20 child end devices
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ROUTER_CONFIG();
  zigbeeConfig.nwk_cfg.zczr_cfg.max_children = 20;

  Serial.println("[Zigbee] Initializing Zigbee stack as Router...");
  if (!Zigbee.begin(&zigbeeConfig)) {
    Serial.println("[Zigbee] Zigbee failed to start! Rebooting in 2 seconds...");
    delay(2000);
    ESP.restart();
  }

  Serial.println("[Zigbee] Stack initialized successfully. Searching for network...");
  currentState = STATE_SEARCHING_NWK;
  blinkTimer = millis();
}

void updateLedStateMachine() {
  unsigned long now = millis();

  // Temporary ping feedback LED pulse handling
  if (pingFeedbackUntil > 0) {
    if (now >= pingFeedbackUntil) {
      pingFeedbackUntil = 0;
      if (currentState == STATE_OPERATIONAL) {
        turnLedsOff();
      }
    } else {
      return; // Keep feedback LED active until expiration
    }
  }

  switch (currentState) {
    case STATE_SEARCHING_NWK:
      // Flashes blue every 1s (500ms ON / 500ms OFF)
      if (now - blinkTimer >= 500) {
        blinkTimer = now;
        blinkState = !blinkState;
        if (blinkState) {
          setLeds(true, 0, 0, LED_BRIGHTNESS);
        } else {
          turnLedsOff();
        }
      }

      // Check if connection is established
      if (Zigbee.connected()) {
        Serial.println("[Zigbee] Network connection established!");
        currentState = STATE_CONNECTED_BLUE;
        stateTimer = now;
        setLeds(true, 0, 0, LED_BRIGHTNESS); // Solid blue
      }
      break;

    case STATE_CONNECTED_BLUE:
      // Keep solid blue for 2 seconds (2000ms)
      if (now - stateTimer >= 2000) {
        Serial.println("[Status] Transitioning to Solid Green for 3 seconds...");
        currentState = STATE_CONNECTED_GREEN;
        stateTimer = now;
        setLeds(false, 0, LED_BRIGHTNESS, 0); // Solid green
      }
      break;

    case STATE_CONNECTED_GREEN:
      // Keep solid green for 3 seconds (3000ms)
      if (now - stateTimer >= 3000) {
        Serial.println("[Status] Connection confirmed. Turning off LEDs for discrete operation.");
        currentState = STATE_OPERATIONAL;
        turnLedsOff();
        lastHeartbeatTimer = now;
        // Send initial heartbeat once operational state is reached
        heartbeatCoordinator(false);
      }
      break;

    case STATE_OPERATIONAL:
      // If connection is lost unexpectedly, return to searching state
      if (!Zigbee.connected()) {
        Serial.println("[Zigbee] Connection lost. Returning to network pairing mode...");
        currentState = STATE_SEARCHING_NWK;
        blinkTimer = now;
      } else {
        // Periodic heartbeat to keep Linkquality and Last Seen updated in Zigbee2MQTT
        if (now - lastHeartbeatTimer >= HEARTBEAT_INTERVAL_MS) {
          lastHeartbeatTimer = now;
          Serial.println("[Heartbeat] Sending periodic heartbeat...");
          heartbeatCoordinator(false);
        }
      }
      break;
  }
}

void handleButton() {
  int reading = digitalRead(BTN_PIN);
  unsigned long now = millis();

  // Transition: Button pressed (HIGH -> LOW)
  if (lastButtonState == HIGH && reading == LOW) {
    buttonPressStartTime = now;
    longPressTriggered = false;
  }

  // Button held down
  if (reading == LOW) {
    unsigned long heldDuration = now - buttonPressStartTime;

    // Held for 5 seconds or more: Trigger factory reset
    if (heldDuration >= 5000 && !longPressTriggered) {
      longPressTriggered = true;
      Serial.println("\n[Button] Button pressed for 5 seconds!");
      Serial.println("[Button] Disconnecting from network and factory resetting Zigbee stack...");
      
      // Visual reset warning: solid red for 1s
      setLeds(false, LED_BRIGHTNESS, 0, 0);
      delay(1000);
      turnLedsOff();

      // Factory reset Zigbee stack (erases NVRAM/NVS credentials and reboots)
      Zigbee.factoryReset(true);
    }
  }

  // Transition: Button released (LOW -> HIGH)
  if (lastButtonState == LOW && reading == HIGH) {
    unsigned long pressedDuration = now - buttonPressStartTime;

    // Short click: between 50ms (debounce) and 4999ms
    if (!longPressTriggered && pressedDuration >= 50) {
      Serial.printf("[Button] Quick click detected (%lu ms).\n", pressedDuration);
      heartbeatCoordinator(true); // Force communication with visual LED feedback
    }
  }

  lastButtonState = reading;
}

void loop() {
  updateLedStateMachine();
  handleButton();
  delay(10);
}
