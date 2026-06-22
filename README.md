# HYBRID Bootloader — OTA Firmware Updater

Bootloader OTA para **ESP32-S3** que actúa como un punto de acceso WiFi y sirve una interfaz web moderna para actualizar el firmware vía OTA (Over-The-Air).

## 🧩 Características

- **Punto de acceso WiFi** — el ESP32-S3 crea su propia red WiFi (AP) para acceder a la interfaz de actualización.
- **Interfaz web moderna** — diseño oscuro tipo GitHub, drag & drop de archivos `.bin`, barra de progreso en vivo y log de estado. Sin dependencias externas (sin jQuery, sin Bootstrap).
- **Streaming OTA** — el firmware se escribe directamente en flash mientras se recibe, sin necesidad de buffers enormes en RAM (~3 MB ahorrados).
- **Sistema de doble partición OTA** (`ota_0` y `ota_1`) con rollback automático ante fallos.
- **Cancelación / Go to App** — permite reiniciar y ejecutar la aplicación principal desde la web.

## 🖼️ Interfaz

La página web se sirve desde la memoria flash del ESP32 y ofrece:

- Drop zone con arrastrar y soltar
- Botón **Upload & Flash** (solo se habilita tras seleccionar un archivo)
- Botón **Go to App** (reinicia hacia la aplicación principal)
- Barra de progreso con gradiente
- Log de eventos en tiempo real
- Versión del firmware actual

## 🧱 Tabla de particiones

| Partition | Tipo | Offset  | Tamaño   |
|-----------|------|---------|----------|
| otadata   | data | 0xd000  | 8 KB     |
| phy_init  | data | 0xf000  | 4 KB     |
| ota_0     | app  | 0x10000 | 1 MB     |
| ota_1     | app  | 0x110000| 2.5 MB   |
| nvs       | data | 0x390000| 256 KB   |

## 🔧 Configuración

Usa `idf.py menuconfig` y ve a **OTA WiFi Configuration** para configurar:

- `STATION_SSID` — nombre de la red WiFi (opcional, para modo estación)
- `STATION_PASSPHRASE` — contraseña WiFi (opcional)

Si no se configura nada, el ESP32 inicia en modo AP.

## 🚀 Compilación y grabación

```bash
# Configurar entorno ESP-IDF 5.5.4
source activate_idf.sh

# Compilar
idf.py build

# Grabar
idf.py -p /dev/ttyUSB0 flash

# Monitorear
idf.py -p /dev/ttyUSB0 monitor
```

## 🌐 Uso

1. Conecta tu PC a la red WiFi del ESP32 (SSID: `UPGRADE_XXXXXX`).
2. Abre un navegador en `http://192.168.4.1`.
3. Selecciona o arrastra un archivo `.bin` compilado.
4. Haz clic en **Upload & Flash**.
5. Espera a que el firmware se flashee y el dispositivo se reinicie.

## 📁 Estructura del proyecto

```
components/
  esp_idf_components/   (componentes oficiales ESP-IDF)
main/
  BOOTmain.c            — punto de entrada, inicializa NVS y WiFi AP
  MyWiFi.c / MyWiFi.h  — configuración WiFi (AP o estación)
  OTAServer.c           — servidor HTTP y lógica OTA en streaming
  OTAServer.h           — declaraciones del servidor OTA
  index.html            — interfaz web (embebida en flash)
  server/
    index.html          — copia del index.html
    favicon.ico         — favicon (embebido en flash)
```

## 🧰 Requisitos

- **ESP-IDF** v5.5.4
- **Target**: ESP32-S3
- Python 3.12+
- Toolchain: `xtensa-esp-elf` (se instala con ESP-IDF)
