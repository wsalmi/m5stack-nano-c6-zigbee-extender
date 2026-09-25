/*
 * nanoC6-ZigbeeExtender.ino
 * 
 * Zigbee Range Extender (Router) para M5Stack M5NanoC6 (ESP32-C6).
 *
 * Requisitos comportamentais:
 * 1. Ao iniciar/desconectado: Pisca em azul a cada 1s (500ms ON / 500ms OFF).
 * 2. Ao conectar: Acende azul por 2s, depois verde por 3s, e depois apaga.
 * 3. Botão pressionado por 5s: Desconecta da rede anterior, limpa NVRAM e volta ao modo de pareamento.
 * 4. Botão clicado 1x rapidamente: Força comunicação com o coordenador (0x0000) para teste de malha.
 */

#ifndef ZIGBEE_MODE_ZCZR
#error "O modo Zigbee ZCZR (Coordinator/Router) deve ser selecionado nas configurações de compilação!"
#endif

#include <Arduino.h>
#include "Zigbee.h"
#include "esp_zigbee_core.h"
#include "zdo/esp_zigbee_zdo_command.h"

// Definições de hardware para M5NanoC6
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

// Estados da máquina de conexão e LED
enum DeviceState {
  STATE_SEARCHING_NWK,
  STATE_CONNECTED_BLUE,
  STATE_CONNECTED_GREEN,
  STATE_OPERATIONAL
};

// Intervalo de Heartbeat periódico para manter Last Seen e Linkquality atualizados no Zigbee2MQTT
#define HEARTBEAT_INTERVAL_MS    60000 // 60 segundos

DeviceState currentState = STATE_SEARCHING_NWK;
unsigned long stateTimer = 0;
unsigned long blinkTimer = 0;
bool blinkState = false;
unsigned long lastHeartbeatTimer = 0;

// Estado e timing do botão
bool lastButtonState = HIGH;
unsigned long buttonPressStartTime = 0;
bool longPressTriggered = false;

// Pulso de LED para feedback de teste de comunicação
unsigned long pingFeedbackUntil = 0;

// Endpoint do Zigbee Range Extender
ZigbeeRangeExtender zbExtender(ZIGBEE_EXTENDER_ENDPOINT);

// Funções auxiliares para controle de LED
void setLeds(bool blueLedOn, uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(BLUE_LED_PIN, blueLedOn ? HIGH : LOW);
  rgbLedWrite(RGB_LED_DATA_PIN, r, g, b);
}

void turnLedsOff() {
  setLeds(false, 0, 0, 0);
}

// Callback de resposta do ZDO ao testar comunicação com o Coordenador
static void activeEpResponseCb(esp_zb_zdp_status_t zdo_status, uint8_t ep_count, uint8_t *ep_id_list, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    Serial.printf("[ZDO] Comunicação com o coordenador OK! Endpoints ativos: %d\n", ep_count);
  } else {
    Serial.printf("[ZDO] Resposta recebida do coordenador com status: 0x%02X\n", zdo_status);
  }
}

// Força comunicação ZCL (Read Attribute e ZDO) com o coordenador.
// O recebimento desse pacote ZCL no coordenador (0x0000) força o Zigbee2MQTT / zigbee-herdsman
// a registrar o LQI (linkquality) e atualizar o timestamp de 'last_seen'.
void heartbeatCoordinator(bool visualFeedback = false) {
  if (!Zigbee.connected()) {
    Serial.println("[Heartbeat] Dispositivo não está conectado à rede Zigbee no momento.");
    return;
  }

  if (visualFeedback) {
    // Breve flash verde (250ms) ao clicar no botão
    setLeds(false, 0, LED_BRIGHTNESS, 0);
    pingFeedbackUntil = millis() + 250;
  }

  Serial.println("[Heartbeat] Enviando ZCL Read Attribute (Basic Cluster) ao Coordenador (0x0000)...");

  // 1. ZCL Read Attribute para o Coordenador (cluster Basic, attribute ZCL_VERSION)
  // Esse frame ZCL gera o evento 'deviceMessage' no Zigbee2MQTT, atualizando linkquality e last_seen.
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
    Serial.printf("[Heartbeat] Falha ao enviar ZCL Read: 0x%02X\n", errRead);
  } else {
    Serial.println("[Heartbeat] ZCL Read enviado com sucesso!");
  }

  // 2. Requisição ZDO Active Endpoints como confirmação adicional de malha
  esp_zb_zdo_active_ep_req_param_t req;
  req.addr_of_interest = 0x0000;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zdo_active_ep_req(&req, activeEpResponseCb, NULL);
  esp_zb_lock_release();
}

// Callback para identificar o dispositivo caso requisitado pela rede (ex: ZHA/Z2M Identify)
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

  // Inicialização dos pinos
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, LOW);

  pinMode(RGB_LED_PWR_PIN, OUTPUT);
  digitalWrite(RGB_LED_PWR_PIN, HIGH); // Ativa energia do WS2812
  delay(10);
  rgbLedWrite(RGB_LED_DATA_PIN, 0, 0, 0);

  pinMode(BTN_PIN, INPUT_PULLUP);

  // Configuração do Endpoint Zigbee Range Extender
  zbExtender.onIdentify(identifyCb);
  zbExtender.setManufacturerAndModel("M5Stack", "NanoC6-ZigbeeExtender");
  zbExtender.setPowerSource(ZB_POWER_SOURCE_MAINS);

  Serial.println("[Zigbee] Adicionando endpoint Range Extender ao núcleo...");
  Zigbee.addEndpoint(&zbExtender);

  // Configuração do modo Roteador com até 20 filhos
  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ROUTER_CONFIG();
  zigbeeConfig.nwk_cfg.zczr_cfg.max_children = 20;

  Serial.println("[Zigbee] Inicializando stack Zigbee como Roteador...");
  if (!Zigbee.begin(&zigbeeConfig)) {
    Serial.println("[Zigbee] Falha ao iniciar Zigbee! Reiniciando em 2 segundos...");
    delay(2000);
    ESP.restart();
  }

  Serial.println("[Zigbee] Stack iniciado com sucesso. Aguardando conexão...");
  currentState = STATE_SEARCHING_NWK;
  blinkTimer = millis();
}

void updateLedStateMachine() {
  unsigned long now = millis();

  // Tratamento de feedback temporário de ping
  if (pingFeedbackUntil > 0) {
    if (now >= pingFeedbackUntil) {
      pingFeedbackUntil = 0;
      if (currentState == STATE_OPERATIONAL) {
        turnLedsOff();
      }
    } else {
      return; // Mantém o feedback de teste ativo temporariamente
    }
  }

  switch (currentState) {
    case STATE_SEARCHING_NWK:
      // Pisca em azul a cada 1s (500ms ON / 500ms OFF)
      if (now - blinkTimer >= 500) {
        blinkTimer = now;
        blinkState = !blinkState;
        if (blinkState) {
          setLeds(true, 0, 0, LED_BRIGHTNESS);
        } else {
          turnLedsOff();
        }
      }

      // Verifica se conectou
      if (Zigbee.connected()) {
        Serial.println("[Zigbee] Conexão estabelecida com a rede!");
        currentState = STATE_CONNECTED_BLUE;
        stateTimer = now;
        setLeds(true, 0, 0, LED_BRIGHTNESS); // Azul sólido
      }
      break;

    case STATE_CONNECTED_BLUE:
      // Mantém azul por 2 segundos (2000ms)
      if (now - stateTimer >= 2000) {
        Serial.println("[Status] Transicionando para Verde por 3 segundos...");
        currentState = STATE_CONNECTED_GREEN;
        stateTimer = now;
        setLeds(false, 0, LED_BRIGHTNESS, 0); // Verde sólido
      }
      break;

    case STATE_CONNECTED_GREEN:
      // Mantém verde por 3 segundos (3000ms)
      if (now - stateTimer >= 3000) {
        Serial.println("[Status] Conexão concluída. Apagando LEDs para operação discreta.");
        currentState = STATE_OPERATIONAL;
        turnLedsOff();
        lastHeartbeatTimer = now;
        // Envia heartbeat inicial logo após a conexão se estabilizar
        heartbeatCoordinator(false);
      }
      break;

    case STATE_OPERATIONAL:
      // Se desconectar da rede inesperadamente, volta a buscar conexão
      if (!Zigbee.connected()) {
        Serial.println("[Zigbee] Conexão perdida. Retornando ao modo de busca...");
        currentState = STATE_SEARCHING_NWK;
        blinkTimer = now;
      } else {
        // Envio periódico de Heartbeat para manter Linkquality e Last Seen atualizados no Zigbee2MQTT
        if (now - lastHeartbeatTimer >= HEARTBEAT_INTERVAL_MS) {
          lastHeartbeatTimer = now;
          Serial.println("[Heartbeat] Enviando heartbeat periódico...");
          heartbeatCoordinator(false);
        }
      }
      break;
  }
}

void handleButton() {
  int reading = digitalRead(BTN_PIN);
  unsigned long now = millis();

  // Transição: Botão pressionado (HIGH -> LOW)
  if (lastButtonState == HIGH && reading == LOW) {
    buttonPressStartTime = now;
    longPressTriggered = false;
  }

  // Botão mantido pressionado
  if (reading == LOW) {
    unsigned long heldDuration = now - buttonPressStartTime;

    // Se segurado por 5 segundos ou mais
    if (heldDuration >= 5000 && !longPressTriggered) {
      longPressTriggered = true;
      Serial.println("\n[Botão] Botão pressionado por 5 segundos!");
      Serial.println("[Botão] Desconectando da rede e redefinindo Zigbee para padrões de fábrica...");
      
      // Indicação visual de reset (vermelho por 1s)
      setLeds(false, LED_BRIGHTNESS, 0, 0);
      delay(1000);
      turnLedsOff();

      // Executa o factory reset do Zigbee (limpa credenciais da NVRAM e reinicia)
      Zigbee.factoryReset(true);
    }
  }

  // Transição: Botão solto (LOW -> HIGH)
  if (lastButtonState == LOW && reading == HIGH) {
    unsigned long pressedDuration = now - buttonPressStartTime;

    // Se não foi um long press e durou entre 50ms (debounce) e 4999ms
    if (!longPressTriggered && pressedDuration >= 50) {
      Serial.printf("[Botão] Clique rápido detectado (%lu ms).\n", pressedDuration);
      heartbeatCoordinator(true); // Força comunicação com feedback visual
    }
  }

  lastButtonState = reading;
}

void loop() {
  updateLedStateMachine();
  handleButton();
  delay(10);
}

