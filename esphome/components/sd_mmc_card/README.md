esphome:
  name: esp32-s3-box3
  friendly_name: ESP32-S3-BOX3

esp32:
  board: esp32-s3-box3
  framework:
    type: esp-idf
    version: latest

# Configuration basique
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

# API et OTA
api:
  encryption:
    key: !secret api_encryption_key

ota:
  password: !secret ota_password

# Logger
logger:
  level: INFO

# Configuration de la carte SD
external_components:
  - source:
      type: local
      path: components/sd_card
    components: [ sd_card ]

sd_box_card:
  id: sd_card_component
  clk_pin: GPIO11
  cmd_pin: GPIO14
  data0_pin: GPIO9
  data1_pin: GPIO13
  data2_pin: GPIO42
  data3_pin: GPIO12
  mode_1bit: false

# Capteurs pour la carte SD
sensor:
  - platform: sd_card
    sd_box_card: sd_card_component
    total_space:
      name: "SD Card Total Space"
    used_space:
      name: "SD Card Used Space"
    free_space:
      name: "SD Card Free Space"

text_sensor:
  - platform: sd_card
    sd_box_card: sd_card_component
    card_type:
      name: "SD Card Type"

# Exemple de media player pour lire un fichier audio de la carte SD
media_player:
  - platform: i2s_audio
    name: "ESP32 Speaker"
    dac_type: external
    i2s_lrclk_pin: GPIO19
    i2s_bclk_pin: GPIO20
    i2s_dout_pin: GPIO18
    media_url: "file:///sdcard/test.flac"

# Exemple d'affichage pour montrer une image de la carte SD
display:
  - platform: ili9xxx
    model: ili9342
    cs_pin: GPIO5
    dc_pin: GPIO4
    reset_pin: GPIO8
    update_interval: 5s
    lambda: |-
      it.fill(COLOR_BLACK);
      it.image(0, 0, id(windy_image));

image:
  - file: 'file:///sdcard/windy_variant3.png'
    id: windy_image
    resize: 320x240
