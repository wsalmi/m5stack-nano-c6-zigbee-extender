# M5Stack M5NanoC6 Zigbee 3.0 Range Extender (Router)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Hardware: M5NanoC6](https://img.shields.io/badge/Hardware-M5Stack%20M5NanoC6-orange.svg)](https://docs.m5stack.com/en/core/M5NanoC6)
[![SoC: ESP32-C6](https://img.shields.io/badge/SoC-ESP32--C6%20(RISC--V)-blue.svg)](https://www.espressif.com/en/products/socs/esp32-c6)
[![Zigbee: 3.0 Router](https://img.shields.io/badge/Zigbee-3.0%20Router%20%2F%20Extender-green.svg)](https://csa-iot.org/all-solutions/zigbee/)
[![Platform: Arduino-ESP32](https://img.shields.io/badge/Platform-Arduino--ESP32%20v3.x-teal.svg)](https://github.com/espressif/arduino-esp32)
[![Firmware: v1.0.0](https://img.shields.io/badge/Firmware-v1.0.0-blue.svg)](./firmware/)

<p align="center">
  <img src="./assets/m5nanoc6_zigbee_icon.png" width="200" alt="M5NanoC6 Zigbee Icon" />
</p>

<p align="center">
  <b>Language / Idioma:</b><br>
  <a href="#english">English</a> | <a href="#espanol">Español</a> | <a href="#portugues">Português</a>
</p>

---

<a id="english"></a>
## 🇬🇧 English

Open-source firmware turning the ultra-compact **M5Stack M5NanoC6** (ESP32-C6FH4) development board into a high-performance **Zigbee 3.0 Range Extender (Router)**.

Ideal for expanding Zigbee mesh coverage across your smart home, bridging dead zones, and improving link reliability for Zigbee coordinators like Home Assistant (ZHA) and Zigbee2MQTT.

### ✨ Features
- 🌐 **True Zigbee 3.0 Router (Range Extender):** Routes mesh traffic at the IEEE 802.15.4 layer and supports up to 20 direct child end devices.
- 💡 **Dual LED State Machine:**
  - **Searching/Pairing:** Flashing blue every 1s (500ms ON / 500ms OFF).
  - **Connected:** Solid blue for 2s, followed by solid green for 3s, then fully off.
  - **Discrete Normal Operation:** LEDs remain completely OFF during routing operation to eliminate annoying ambient light in bedrooms or living areas.
- 💓 **Active ZCL Heartbeat for Zigbee2MQTT:** Automatically transmits a periodic `genBasic` cluster read request every 60s to ensure `last_seen` and `linkquality` (LQI) stay continuously updated in Zigbee2MQTT and Home Assistant.
- 🔘 **Multi-Function Button (GPIO 9):**
  - **Quick Click (< 1s):** Instantly forces a ZCL heartbeat and ZDO query to coordinator `0x0000` with a rapid green confirmation pulse.
  - **Long Press (5s):** Clears Zigbee NVRAM/NVS credentials, leaves the current network, and reboots in pairing mode.
- 🔌 **Mains Powered Classification:** Declares `Power Source = Mains` to ensure coordinators maintain permanent keep-alive and routing paths.
- 🧩 **Zigbee2MQTT External Converter Included:** Ready-to-use converter (`m5nanoc6_extender.js`) for full vendor, model, and device identification in Z2M.

### 📌 Hardware Pinout (M5NanoC6)
| Pin | Function | Configuration | Description |
| :--- | :--- | :--- | :--- |
| **GPIO 9** | Front Button | `INPUT_PULLUP` | Active-low input (Quick Click = Ping, 5s = Factory Reset) |
| **GPIO 7** | Blue LED | `OUTPUT` | Built-in monochrome blue LED |
| **GPIO 19** | RGB LED Power Switch | `OUTPUT` | Power gate for WS2812 (set `HIGH` to enable) |
| **GPIO 20** | RGB LED Data | `rgbLedWrite()` | WS2812 addressable RGB LED data signal |
| **USB CDC** | Serial Monitor & Upload | 115200 bps | Native USB-Serial/JTAG port |

### 🚦 LED Status Indicators
| State | Pattern | Details |
| :--- | :--- | :--- |
| **Pairing / Searching** | 🔵 Blinking Blue (1s period) | Device looking for an open Zigbee network. |
| **Connected (Stage 1)** | 🔵 Solid Blue for 2s | Successfully joined Zigbee network. |
| **Connected (Stage 2)** | 🟢 Solid Green for 3s | Network connection confirmed. |
| **Operational Mode** | ⚫ Off | Silent mesh routing active; no ambient light disturbance. |
| **Communication Test** | 🟢 Rapid Green Pulse (250ms) | Triggered by 1x quick button click. |
| **Factory Reset** | 🔴 Solid Red for 1s | Triggered after holding button for 5s before reboot. |

### 📡 Zigbee2MQTT Integration
Zigbee routers route mesh frames at the MAC layer, meaning routed packets from other devices do not originate from the router's own address. This firmware solves inactive `last_seen` and `linkquality` states by sending periodic ZCL heartbeat frames every 60s.

#### Adding the External Converter in Zigbee2MQTT
1. In the Zigbee2MQTT Web UI, navigate to **Settings ➔ External converters**.
2. Click **Add converter** and create `m5nanoc6_extender.js`.
3. Paste the contents of [`m5nanoc6_extender.js`](./m5nanoc6_extender.js):
```javascript
const {identify} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['NanoC6-ZigbeeExtender'],
    model: 'NanoC6-ZigbeeExtender',
    vendor: 'M5Stack',
    description: 'M5Stack M5NanoC6 Zigbee 3.0 Range Extender / Router',
    extend: [
        identify(),
    ],
};

module.exports = definition;
```
4. Click **Save**. The device will immediately show as **Supported: true** with `linkquality` and an `identify` button!

### 🏠 Home Assistant (ZHA) Integration
1. Go to **Settings ➔ Devices & Services ➔ Zigbee Home Automation**.
2. Click **Add Device** (search for devices).
3. Power on the M5NanoC6 (it will blink blue).
4. ZHA will discover the device as `M5Stack NanoC6-ZigbeeExtender` and configure it as a router.
5. In the ZHA Network Visualization map, the device will appear with routing connections to nearby nodes.

### 🔥 M5Burner & Pre-compiled Binaries (v1.0.0)
Pre-compiled binary releases are versioned and ready in the [`firmware/`](./firmware/) directory:

| File | Offset / Address | Description |
| :--- | :--- | :--- |
| **`m5nanoc6_zigbee_extender_v1.0.0_merged.bin`** | `0x0000` | **Recommended:** Complete all-in-one flash image (Bootloader + Partitions + App). |
| **`m5nanoc6_zigbee_extender_v1.0.0.bin`** | `0x10000` | Application firmware binary. |
| **`bootloader.bin`** | `0x0000` | ESP32-C6 second-stage bootloader. |
| **`partitions.bin`** | `0x8000` | Zigbee ZCZR 4MB partition table. |

#### Flashing with M5Burner (Custom Burn)
1. Open **M5Burner** on your computer.
2. Select **NanoC6** as the target device.
3. Use the **Custom Burn** option:
   - Select `firmware/m5nanoc6_zigbee_extender_v1.0.0_merged.bin` at address `0x0000`.
4. Click **Burn**!

### 🛠️ Building & Flashing
```bash
# 1. Compile
arduino-cli compile -b esp32:esp32:m5stack_nanoc6:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr,CDCOnBoot=cdc .

# 2. Upload
arduino-cli upload -p /dev/cu.usbmodem14101 -b esp32:esp32:m5stack_nanoc6:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr,CDCOnBoot=cdc .

# 3. Monitor Serial
arduino-cli monitor -p /dev/cu.usbmodem14101 -c baudrate=115200
```

---

<a id="espanol"></a><a id="español"></a>
## 🇪🇸 Español

Firmware de código abierto que convierte la placa de desarrollo **M5Stack M5NanoC6** (ESP32-C6FH4) en un **Extensor de Alcance (Router) Zigbee 3.0** de alto rendimiento.

Ideal para ampliar la cobertura de red Zigbee en el hogar inteligente, eliminar zonas sin señal y mejorar la estabilidad de coordinadores como Home Assistant (ZHA) y Zigbee2MQTT.

### ✨ Características
- 🌐 **Router Zigbee 3.0 Real:** Enruta el tráfico de malla en la capa IEEE 802.15.4 y admite hasta 20 dispositivos hijos directos.
- 💡 **Indicadores LED Inteligentes:**
  - **Buscando red / Emparejamiento:** Parpadeo azul cada 1s (500ms encendido / 500ms apagado).
  - **Conectado:** Azul fijo durante 2s, seguido de verde fijo durante 3s y luego se apaga.
  - **Modo Operativo Discreto:** Los LEDs permanecen apagados durante el funcionamiento normal para evitar molestias lumínicas nocturnas.
- 💓 **Heartbeat ZCL Periódico para Zigbee2MQTT:** Envía automáticamente una solicitud `genBasic` cada 60s para que `last_seen` y `linkquality` (LQI) se actualicen de manera continua en Zigbee2MQTT.
- 🔘 **Botón Multifunción (GPIO 9):**
  - **Clic Rápido (< 1s):** Fuerza el envío inmediato de paquetes ZCL y ZDO al coordinador `0x0000` con un pulso verde de confirmación.
  - **Pulsación Larga (5s):** Borra las credenciales NVRAM/NVS de Zigbee, abandona la red y reinicia en modo de emparejamiento.
- 🔌 **Clasificación Alimentación de Red:** Informa `Power Source = Mains` para que los coordinadores mantengan rutas activas permanentes.
- 🧩 **Conversor Externo para Zigbee2MQTT Incluido:** Archivo listo [`m5nanoc6_extender.js`](./m5nanoc6_extender.js) para soporte nativo completo.

### 📌 Pines de Hardware (M5NanoC6)
| Pin | Función | Configuración | Descripción |
| :--- | :--- | :--- | :--- |
| **GPIO 9** | Botón Frontal | `INPUT_PULLUP` | Entrada activo en bajo (Clic = Prueba, 5s = Reset) |
| **GPIO 7** | LED Azul | `OUTPUT` | LED azul integrado |
| **GPIO 19** | Alimentación LED RGB | `OUTPUT` | Conmutador de energía del WS2812 (`HIGH` = encendido) |
| **GPIO 20** | Datos LED RGB | `rgbLedWrite()` | Línea de datos del LED direccionable WS2812 |
| **USB CDC** | Monitor Serial y Carga | 115200 bps | Puerto nativo USB-Serial/JTAG |

### 🚦 Estados de los LEDs
| Estado | Patrón | Detalle |
| :--- | :--- | :--- |
| **Emparejando / Buscando** | 🔵 Parpadeo Azul (ciclo 1s) | Buscando una red Zigbee abierta. |
| **Conectado (Etapa 1)** | 🔵 Azul Fijo durante 2s | Unión a la red exitosa. |
| **Conectado (Etapa 2)** | 🟢 Verde Fijo durante 3s | Confirmación de conexión. |
| **Modo Operativo** | ⚫ Apagado | Enrutamiento silencioso activo; sin luz molesta. |
| **Prueba de Comunicación**| 🟢 Pulso Verde Rápido (250ms) | Disparado con 1 clic rápido en el botón. |
| **Reset de Fábrica** | 🔴 Rojo Fijo durante 1s | Notificación antes de borrar NVRAM y reiniciar. |

### 📡 Integración con Zigbee2MQTT
Para que el dispositivo aparezca como soportado:
1. En la interfaz de Zigbee2MQTT, vaya a **Settings ➔ External converters**.
2. Añada un conversor llamado `m5nanoc6_extender.js` y pegue el código del archivo [`m5nanoc6_extender.js`](./m5nanoc6_extender.js).
3. Guarde. El dispositivo se reconocerá con estado **Supported: true** y botón de identificar LED.

### 🔥 Binarios para M5Burner (v1.0.0)
Los binarios precompilados se encuentran versionados en [`firmware/`](./firmware/):
- **`m5nanoc6_zigbee_extender_v1.0.0_merged.bin`**: Imagen completa lista para grabar en la dirección **`0x0000`** en la opción *Custom Burn* del M5Burner.

---

<a id="portugues"></a><a id="português"></a>
## 🇧🇷 Português

Firmware de código aberto que transforma a placa **M5Stack M5NanoC6** (ESP32-C6FH4) em um **Extensor de Alcance (Roteador) Zigbee 3.0** de alto desempenho.

Ideal para expandir o alcance da malha Zigbee residencial, eliminar pontos cegos e aumentar a estabilidade do Home Assistant (ZHA) e Zigbee2MQTT.

### ✨ Funcionalidades
- 🌐 **Roteador Zigbee 3.0 Real:** Roteia pacotes de malha na camada IEEE 802.15.4 e suporta até 20 dispositivos filhos diretos.
- 💡 **Máquina de Estados de LEDs:**
  - **Buscando rede / Pareamento:** Piscando em azul a cada 1s (500ms ligado / 500ms desligado).
  - **Conectado:** Azul fixo por 2s, seguido de verde fixo por 3s e depois apaga.
  - **Operação Discreta:** LEDs permanecem apagados durante a operação normal para não incomodar em quartos ou salas.
- 💓 **Heartbeat ZCL Periódico para Zigbee2MQTT:** Envia a cada 60s uma requisição `genBasic` ao coordenador, mantendo `last_seen` e `linkquality` (LQI) sempre atualizados no MQTT.
- 🔘 **Comandos do Botão Frontal (GPIO 9):**
  - **Clique Rápido (< 1s):** Força o envio imediato de comando ZCL e ZDO ao coordenador `0x0000` com pulso verde no LED.
  - **Pressão Longa (5s):** Limpa a memória NVRAM/NVS do Zigbee, desconecta da rede e reinicia no modo de pareamento.
- 🔌 **Alimentação de Rede (Mains):** Reporta `Power Source = Mains` para assegurar conexões e rotas ativas permanentes.
- 🧩 **Conversor Externo para Zigbee2MQTT:** Arquivo pronto [`m5nanoc6_extender.js`](./m5nanoc6_extender.js) para suporte completo.

### 📌 Mapeamento de Pinos (M5NanoC6)
| Pino | Função | Configuração | Descrição |
| :--- | :--- | :--- | :--- |
| **GPIO 9** | Botão Frontal | `INPUT_PULLUP` | Entrada ativa em nível baixo (Clique = Teste, 5s = Reset) |
| **GPIO 7** | LED Azul | `OUTPUT` | LED azul integrado |
| **GPIO 19** | Chave Força LED RGB | `OUTPUT` | Chave de alimentação do WS2812 (`HIGH` = ligado) |
| **GPIO 20** | Dados LED RGB | `rgbLedWrite()` | Linha de dados do LED endereçável WS2812 |
| **USB CDC** | Monitor Serial e Gravação | 115200 bps | Porta nativa USB-Serial/JTAG |

### 🚦 Padrões dos LEDs
| Estado | Padrão | Descrição |
| :--- | :--- | :--- |
| **Pareamento / Busca** | 🔵 Piscando Azul (1s ciclo) | Dispositivo procurando rede Zigbee aberta. |
| **Conectado (Etapa 1)** | 🔵 Azul Sólido por 2s | Associação à rede realizada com sucesso. |
| **Conectado (Etapa 2)** | 🟢 Verde Sólido por 3s | Confirmação de conexão. |
| **Operação Normal** | ⚫ Apagado | Roteamento ativo e discreto; sem luz incômoda. |
| **Teste de Comunicação** | 🟢 Pulso Verde Rápido (250ms) | Disparado por 1 clique rápido no botão. |
| **Reset de Fábrica** | 🔴 Vermelho Sólido por 1s | Alerta antes de limpar a NVRAM e reiniciar. |

### 📡 Integração com Zigbee2MQTT
1. No painel do Zigbee2MQTT, acesse **Settings ➔ External converters**.
2. Adicione um conversor com o nome `m5nanoc6_extender.js` e cole o conteúdo de [`m5nanoc6_extender.js`](./m5nanoc6_extender.js).
3. Salve. O dispositivo será reconhecido formalmente com status **Supported: true** e botão de identificar LED.

### 🔥 Binários Pré-compilados para M5Burner (v1.0.0)
Na pasta [`firmware/`](./firmware/):
- **`m5nanoc6_zigbee_extender_v1.0.0_merged.bin`**: Imagem consolidada de 4MB para gravação a partir de **`0x0000`** no *Custom Burn* do M5Burner.
- **`m5nanoc6_zigbee_extender_v1.0.0.bin`**: Binário da aplicação compilada (endereço `0x10000`).

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](https://github.com/wsalmi/m5stack-nano-c6-zigbee-extender/issues).

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
