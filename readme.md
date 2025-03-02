# esphome_sd_card

SD MMC cards components for esphome.

## Config

```yaml
sd_mmc_card:
  id: sd_mmc_card
  mode_1bit: false
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  data1_pin: GPIO4
  data2_pin: GPIO12
  data3_pin: GPIO13
  power_ctrl_pin: GPIO43  # Optionnel : GPIO pour contrôler l'alimentation de la carte SD
```

* **mode_1bit** (Optional, bool): spécifie si le mode 1 bit ou 4 bits est utilisé
* **clk_pin** : (Required, GPIO): broche d'horloge
* **cmd_pin** : (Required, GPIO): broche de commande
* **data0_pin**: (Required, GPIO): broche de données 0
* **data1_pin**: (Optional, GPIO): broche de données 1, utilisée uniquement en mode 4 bits
* **data2_pin**: (Optional, GPIO): broche de données 2, utilisée uniquement en mode 4 bits
* **data3_pin**: (Optional, GPIO): broche de données 3, utilisée uniquement en mode 4 bits
* **power_ctrl_pin**: (Optional, GPIO): broche pour contrôler l'alimentation de la carte SD (par exemple, GPIO43 pour l'ESP32-S3-Box-3)

### Contrôle d'alimentation (PWR_CTRL)

Pour les appareils comme l'ESP32-S3-Box-3, vous pouvez utiliser la broche `power_ctrl_pin` pour activer ou désactiver l'alimentation de la carte SD. Par exemple, sur l'ESP32-S3-Box-3, la broche GPIO43 est souvent utilisée pour contrôler l'alimentation du lecteur de carte SD.

Exemple de configuration pour l'ESP32-S3-Box-3 :
```yaml
sd_mmc_card:
  clk_pin: GPIO14
  cmd_pin: GPIO15
  data0_pin: GPIO2
  power_ctrl_pin: GPIO43  # Active l'alimentation du lecteur de carte SD
```

### Notes

#### Arduino Framework

Ce composant utilise la bibliothèque SD_MMC et partage ses limitations. Pour plus de détails, voir :
[SD_MMC](https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC)

Le mode 4 bits ne fonctionne pas avec les versions d'Arduino Framework antérieures à `2.0.7` en raison d'un problème dans la fonction `setPins` de SD_MMC. Ce problème a été corrigé par la pull request [espressif/arduino-esp32/#7646](https://github.com/espressif/arduino-esp32/pull/7646)

La version recommandée par esphome est `2.0.5`.

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
    version: latest
```

#### ESP-IDF Framework

Par défaut, les noms de fichiers longs ne sont pas activés. Pour changer ce comportement, `CONFIG_FATFS_LFN_STACK` ou `CONFIG_FATFS_LFN_HEAP` doit être défini dans la configuration du framework. Voir la [documentation Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html#config-fatfs-long-filenames) pour plus de détails.

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"
```

## Actions

### Write file

```yaml
sd_mmc_card.write_file:
    path: !lambda "/test.txt" 
    data: !lambda |
        std::string str("content");
        return std::vector<uint8_t>(str.begin(), str.end())
```

Écrit un fichier sur la carte SD.

* **path** (Templatable, string): chemin absolu du fichier
* **data** (Templatable, vector<uint8_t>): contenu du fichier

### Append file

```yaml
sd_mmc_card.append_file:
    path: "/test.txt" 
    data: !lambda |
        std::string str("new content");
        return std::vector<uint8_t>(str.begin(), str.end())
```
Ajoute du contenu à un fichier sur la carte SD.

* **path** (Templatable, string): chemin absolu du fichier
* **data** (Templatable, vector<uint8_t>): contenu à ajouter

### Delete file

```yaml
sd_mmc_card.delete_file:
    path: "/test.txt"
```

Supprime un fichier de la carte SD.

* **path** (Templatable, string): chemin absolu du fichier

### Create directory

```yaml
sd_mmc_card.create_directory:
    path: "/test"
```

Crée un dossier sur la carte SD.

* **path** (Templatable, string): chemin absolu du dossier

### Remove directory

Supprime un dossier de la carte SD.

```yaml
sd_mmc_card.remove_directory:
    path: "/test"
```

## Sensors

### Used space

```yaml
sensor:
  - platform: sd_mmc_card
    type: used_space
    name: "SD card used space"
```

Espace utilisé sur la carte SD en octets.

* Toutes les options [sensor](https://esphome.io/components/sensor/) sont disponibles

### Total space

```yaml
sensor:
  - platform: sd_mmc_card
    type: total_space
    name: "SD card total space"
```

Capacité totale de la carte SD.

* Toutes les options [sensor](https://esphome.io/components/sensor/) sont disponibles

### Free space

```yaml
sensor:
  - platform: sd_mmc_card
    type: free_space
    name: "SD card free space"
```

Espace libre sur la carte SD.

* Toutes les options [sensor](https://esphome.io/components/sensor/) sont disponibles

### File size

```yaml
sensor:
  - platform: sd_mmc_card
    type: file_size
    path: "/test.txt"
```

Retourne la taille du fichier spécifié.

* **path** (Required, string): chemin du fichier
* Toutes les options [sensor](https://esphome.io/components/sensor/) sont disponibles

## Text Sensor

```yaml
text_sensor:
  - platform: sd_mmc_card
    sd_card_type:
      name: "SD card type"
```

Type de carte SD (MMC, SDSC, ...)

* Toutes les options [text sensor](https://esphome.io/components/text_sensor/) sont disponibles

## Others

### List Directory

```cpp
std::vector<std::string> list_directory(const char *path, uint8_t depth);
std::vector<std::string> list_directory(std::string path, uint8_t depth);
```

* **path** : répertoire racine
* **depth**: profondeur maximale 

Exemple

```yaml
- lambda: |
  for (auto const & file : id(esp_camera_sd_card)->list_directory("/", 1))
    ESP_LOGE("   ", "File: %s\n", file.c_str());
```

### List Directory File Info

```cpp
struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &, size_t, bool);
};

std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth);
std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
```

* **path** : répertoire racine
* **depth**: profondeur maximale 

Exemple

```yaml
- lambda: |
  for (auto const & file : id(sd_mmc_card)->list_directory_file_info("/", 1))
    ESP_LOGE("   ", "File: %s, size: %d\n", file.path.c_str(), file.size);
```

### Is Directory

```cpp
bool is_directory(const char *path);
bool is_directory(std::string const &path);
```

* **path**: chemin du répertoire à tester

Exemple

```yaml
- lambda: return id(sd_mmc_card)->is_directory("/folder");
```

### File Size

```cpp
size_t file_size(const char *path);
size_t file_size(std::string const &path);
```

* **path**: chemin du fichier

Exemple

```yaml
- lambda: return id(sd_mmc_card)->file_size("/file");
```

### Read File

```cpp
std::vector<uint8_t> read_file(char const *path);
std::vector<uint8_t> read_file(std::string const &path);
```

Retourne le contenu du fichier sous forme de vecteur. Essayer de lire un fichier volumineux peut saturer la mémoire de l'ESP.

* **path**: chemin du fichier

Exemple

```yaml
- lambda: return id(sd_mmc_card)->read_file("/file");
```

## Helpers

### Convert Bytes

```cpp
enum MemoryUnits : short {
    Byte = 0,
    KiloByte = 1,
    MegaByte = 2,
    GigaByte = 3,
    TeraByte = 4,
    PetaByte = 5
};

long double convertBytes(uint64_t, MemoryUnits);
```

Convertit une valeur en octets vers une autre unité de mémoire.

Exemple:

```yaml
sensor:
    - platform: sd_mmc_card
        type: file_size
        name: "text.txt size"
        unit_of_measurement: Kb
        path: "/test.txt"
        filters:
        - lambda: return sd_mmc_card::convertBytes(x, sd_mmc_card::MemoryUnits::KiloByte);
```
