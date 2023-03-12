# ESP32 NanoView
Reads data from a [NanoView](http://www.nanoview.co.za/) and sends it as JSON via MQTT.  
Intended to be useful with Grafana + InfluxDB or similar.


[Protocol details](http://www.nanoview.co.za/protocol.html)

## Wiring
Useful info can be found at: [Hacking the NanoView with a RPi Zero W](http://silico.co.za/blog/2019/04/13/hacking-the-nanoview-with-a-rpi-zero-w/).  
Instead of connecting TX to the RPi via voltage divider (Step 8 in the article linked above), connect it to the ESP32 configured UART RX pin (GPIO 5 as set in `nanoview_uart.c`). Ensure the ground of NanoHub and ESP32 are connected together too.

## ESP32 esp-idf Project Configuration
### Menuconfig
Either run `idf.py menuconfig` and set the WiFi and MQTT parameters (under "MQTT and WiFi Configuration"), or use an include file as below.

### Include File 
Create `nanoview_config.h` in the `main/` directory, and enable the `Use nanoview_config.h instead of below` configuration option from `idf.py menuconfig`

The file content should be as below:

```
#define ESP_WIFI_SSID      "your_wifi_name"
#define ESP_WIFI_PASS      "your_super_secret_wifi_key"
#define ESP_MAXIMUM_RETRY  10

#define BROKER_URL "mqtt://esp32-nanoview:yourpassword@192.168.0.123"
```

## Home Assistant Setup  
Install the Mosquitto MQTT broker Add-on, and follow the basic configuration

**configuration.yaml**
```
mqtt:
  sensor: !include sensor-mqtt.yaml
```

**sensor-mqtt.yaml**
Configure the live wattage and kWh readings from MQTT as sensors
```
- name: "Mains Voltage Reading"
  state_topic: /esp-mqtt/nanoview/live_power
  value_template: >-
    {%-for i in value_json-%}
      {{ i.volts|int if i.name == 'mains_voltage' and i.volts|int < 32768 }}
    {%-endfor%}
  expire_after: 20
  device_class: energy
  unit_of_measurement: V

- name: "Live Power - Total Reading"
  state_topic: /esp-mqtt/nanoview/live_power
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int if i.channel == 1 and i.value|int < 32768 }}
    {%-endfor%}
  expire_after: 20
  device_class: energy
  unit_of_measurement: W

- name: "Total Power"
  state_topic: /esp-mqtt/nanoview/accumulated_power
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int / 1000 if i.channel == 1 and i.value|int / 1000 < 8000 }}
    {%-endfor%}
  device_class: energy
  unit_of_measurement: kWh
  state_class: total_increasing

- name: "Total Power - Plugs"
  state_topic: /esp-mqtt/nanoview/accumulated_power
  object_id: "total_power_plugs"
  value_template: >-
    {% set vars = namespace(plugs=0,lights=0) %}
    {%-for i in value_json-%}  
      {% if i.channel == 2 and i.value|int / 1000 < 8000 %}
        {% set vars.plugs = i.value|int / 1000 %}
      {% endif %}
      {% if i.channel == 3 and i.value|int / 1000 < 8000 %}
        {% set vars.lights = i.value|int / 1000 %}
      {% endif %}
    {%-endfor%}
    {{ vars.plugs - vars.lights }}
  device_class: energy
  state_class: total_increasing
  unit_of_measurement: kWh

- name: "Live Power - Geyser Reading"
  state_topic: /esp-mqtt/nanoview/live_power
  object_id: live_power_geyser_reading
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int if i.channel == 7 and i.value|int < 32768 }}
    {%-endfor%}
  expire_after: 20
  device_class: energy
  unit_of_measurement: W

- name: "Total Power - Geyser"
  state_topic: /esp-mqtt/nanoview/accumulated_power
  object_id: "total_power_geyser"
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int / 1000 if i.channel == 7 and i.value|int / 1000 < 8000 }}
    {%-endfor%}
  device_class: energy
  unit_of_measurement: kWh
  state_class: total_increasing

- name: "Live Power - Stove Reading"
  state_topic: /esp-mqtt/nanoview/live_power
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int if i.channel == 5 and i.value|int < 32768 }}
    {%-endfor%}
  expire_after: 20
  device_class: energy
  unit_of_measurement: W

- name: "Total Power - Stove"
  state_topic: /esp-mqtt/nanoview/accumulated_power
  object_id: "total_power_stove"
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int / 1000 if i.channel == 5 and i.value|int / 1000 < 8000 }}
    {%-endfor%}
  device_class: energy
  unit_of_measurement: kWh
  state_class: total_increasing

- name: "Live Power - Lights Reading"
  state_topic: /esp-mqtt/nanoview/live_power
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int if i.channel == 3 and i.value|int < 32768 }}
    {%-endfor%}
  expire_after: 20
  device_class: energy
  unit_of_measurement: W

- name: "Total Power - Lights"
  icon: "mdi:light-recessed"
  state_topic: /esp-mqtt/nanoview/accumulated_power
  value_template: >-
    {%-for i in value_json-%}
      {{ i.value|int / 1000 if i.channel == 3 and i.value|int / 1000 < 8000 }}
    {%-endfor%}
  device_class: energy
  unit_of_measurement: kWh
  state_class: total_increasing

# Using math to calculate difference between 2 readings for cases where 1 CT monitors 2 circuits, and another monitors only 1 of the 2
- name: "Live Power - Plugs Reading"
  state_topic: /esp-mqtt/nanoview/live_power
  value_template: >-
    {% set vars = namespace(plugs=0,lights=0) %}
    {%-for i in value_json-%}  
      {% if i.channel == 2 and i.value|int < 32768 %}
        {% set vars.plugs = i.value|int %}
      {% endif %}
      {% if i.channel == 3 and i.value|int < 32768 %}
        {% set vars.lights = i.value|int %}
      {% endif %}
    {%-endfor%}
    {{ vars.plugs - vars.lights }}
  expire_after: 20
  device_class: energy
  unit_of_measurement: W
  ```
  
**sensor-template.yaml**
Configure sensors based on the MQTT sensors, so that if they are `Unknown` they report `0`  
  ```
  - sensor:
    - name: "Mains Voltage"
      device_class: energy
      unit_of_measurement: V
      state: >-
        {{ states('sensor.mains_voltage_reading')|int(0) }}

- sensor:
    - name: "Live Power - Total"
      device_class: energy
      unit_of_measurement: W
      state: >-
        {{ states('sensor.live_power_total_reading')|int(0) }}
- sensor:
    - name: "Live Power - Geyser"
      device_class: energy
      unit_of_measurement: W
      state: >-
        {{ (states('sensor.live_power_geyser_reading')|int(0)) }}

- sensor:
    - name: "Live Power - Plugs"
      device_class: energy
      unit_of_measurement: W
      state: >-
        {{ (states('sensor.live_power_plugs_reading')|int(0)) }}

- sensor:
    - name: "Live Power - Lights"
      device_class: energy
      unit_of_measurement: W
      state: >-
        {{ states('sensor.live_power_lights_reading')|int(0) }}

- sensor:
    - name: "Live Power - Grid"
      device_class: energy
      unit_of_measurement: W
      state: >-
        {{ states('sensor.live_power_grid_reading')|int(0) }}

- sensor:
    - name: "Live Power - Stove"
      device_class: energy
      unit_of_measurement: W
      state: >-
        {{ states('sensor.live_power_stove_reading')|int(0) }}

- binary_sensor:
    - name: "Grid Status"
      state: "{{ states('sensor.grid_voltage')|int(0) > 200 }}"

```
