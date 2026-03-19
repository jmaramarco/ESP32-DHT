# ESP32-DHT

Sensor de temperatura y humedad con ESP32 y DHT22, con interfaz web via WebSocket y integración con Home Assistant via MQTT.

![ESP32](https://img.shields.io/badge/ESP32-Arduino-blue) ![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-orange) ![License](https://img.shields.io/badge/License-MIT-green)

---

## Características

- Lectura de temperatura y humedad en tiempo real via **WebSocket**
- Interfaz web servida desde **LittleFS** (sin dependencias externas)
- Integración con **Home Assistant** via MQTT discovery automático
- Configuración de WiFi y broker MQTT desde la interfaz web
- Modo **Access Point** para configuración inicial
- Acceso por nombre via **mDNS** (`http://esp32-dht.local`)
- Todos los recursos (íconos, CSS) disponibles **offline**

---

## Hardware requerido

| Componente | Detalle |
|---|---|
| ESP32 | Cualquier variante con WiFi |
| DHT22 | Sensor de temperatura y humedad |
| Resistencia | 10kΩ pull-up entre DATA y VCC |

### Conexiones

| DHT22 | ESP32 |
|---|---|
| VCC | 3.3V |
| DATA | GPIO 4 |
| GND | GND |

---

## Software requerido

- [PlatformIO](https://platformio.org/) (extensión de VSCode recomendada)
- [Git](https://git-scm.com/)

---

## Instalación

### 1. Clonar el repositorio

```bash
git clone https://github.com/tu-usuario/ESP32-DHT.git
cd ESP32-DHT
```

### 2. Estructura del proyecto

```
ESP32-DHT/
├── src/
│   └── main.cpp
├── data/
│   ├── index.html
│   ├── config.html
│   ├── style.css
│   ├── script.js
│   ├── favicon.svg
│   ├── gear.svg
│   ├── thermometer.svg
│   └── droplet.svg
└── platformio.ini
```

### 3. Compilar y flashear

```bash
# Subir el filesystem (archivos web)
pio run --target uploadfs

# Compilar y subir el firmware
pio run --target upload
```

---

## Configuración inicial

### Primera vez (sin credenciales guardadas)

1. El ESP32 arranca en **modo AP** con la red `ESP32-DHT` (contraseña: `12345678`)
2. Conectate a esa red desde tu celular o PC
3. Abrí `http://192.168.4.1/config`
4. Ingresá las credenciales de tu red WiFi y guardá
5. El ESP32 se reinicia y se conecta a tu red

### Configurar MQTT (opcional)

1. Abrí `http://esp32-dht.local/config` (o la IP del dispositivo)
2. Tildá **"Utilizar Broker con Home Assistant"**
3. Completá la IP del broker, puerto, usuario y contraseña
4. Guardá la configuración
5. Presioná **"Conectar"** para iniciar la conexión

---

## Integración con Home Assistant

El dispositivo usa **MQTT Discovery** para registrarse automáticamente en Home Assistant. Al conectarse al broker, publica los siguientes tópicos:

| Entidad | Tópico de estado |
|---|---|
| Temperatura | `home/dht/temperature` |
| Humedad | `home/dht/humidity` |

Los sensores aparecen automáticamente en **Configuración → Dispositivos y servicios → MQTT**.

### Requisitos en Home Assistant

- Complemento **Mosquitto broker** instalado y corriendo
- MQTT Discovery habilitado (viene habilitado por defecto)
- Usuario con acceso al broker configurado

---

## Acceso a la interfaz web

| URL | Descripción |
|---|---|
| `http://esp32-dht.local` | Página principal con lecturas |
| `http://esp32-dht.local/config` | Configuración WiFi y MQTT |
| `http://esp32-dht.local/reset/wifi` | Resetear credenciales WiFi |
| `http://esp32-dht.local/reset/mqtt` | Resetear configuración MQTT |
| `http://esp32-dht.local/mqtt/connect` | Conectar al broker manualmente |
| `http://esp32-dht.local/mqtt/status` | Estado de la conexión MQTT (JSON) |

> **Nota:** Si mDNS no funciona (por ejemplo con Tailscale activo), usar la IP directa del dispositivo.

---

## Dependencias

```ini
lib_deps =
    adafruit/DHT sensor library@^1.4.4
    adafruit/Adafruit Unified Sensor@^1.1.14
    me-no-dev/ESPAsyncWebServer
    me-no-dev/AsyncTCP
    knolleary/PubSubClient@^2.8
```

---

## Licencia

MIT — libre para usar, modificar y distribuir.
