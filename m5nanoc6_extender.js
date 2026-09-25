/**
 * External Converter para o M5NanoC6 Zigbee Extender no Zigbee2MQTT.
 *
 * Como instalar no Zigbee2MQTT:
 * 1. Abra o painel do Zigbee2MQTT no navegador.
 * 2. Acesse: Settings (Configurações) -> External converters (Conversores externos).
 * 3. Adicione o arquivo "m5nanoc6_extender.js" com o conteúdo abaixo.
 * 4. Salve.
 */

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
