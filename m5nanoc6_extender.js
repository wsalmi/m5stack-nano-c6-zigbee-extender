/**
 * External Converter for M5NanoC6 Zigbee Extender in Zigbee2MQTT.
 *
 * Installation instructions:
 * 1. Open the Zigbee2MQTT web frontend.
 * 2. Navigate to: Settings -> External converters.
 * 3. Add a new converter named "m5nanoc6_extender.js" and paste the code below.
 * 4. Click Submit / Save.
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
