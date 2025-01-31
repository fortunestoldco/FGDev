# Seeed Xiao ESP32C6 Plant Monitor

A comprehensive plant monitoring system using ZephyrOS, integrating multiple sensors and AWS IoT Core for data visualization and management.

## Features

- **Temperature & Humidity Monitoring:** Utilizes the AHT10 sensor.
- **Soil Moisture Sensing:** Employs a capacitive soil moisture sensor.
- **Light Level Measurement:** Uses a photoresistor connected to an ADC.
- **Battery Level Monitoring:** Incorporates the MAX17043 fuel gauge.
- **UUID Generation:** Generates a unique UUID on the first boot.
- **Wi-Fi Provisioning over BLE:** Allows user to provision Wi-Fi credentials via Bluetooth.
- **AWS IoT Core Integration:** Publishes sensor data to AWS IoT Core using MQTT.
- **Data Caching:** Caches data locally if Wi-Fi connection is lost.
- **Button Interactions:** Supports soft and hard resets, and re-provisioning.
- **Over-The-Air (OTA) Updates:** Supports remote firmware updates.

## Hardware Components

- **Seeed Xiao ESP32C6**
- **AHT10 Temperature & Humidity Sensor**
- **Capacitive Soil Moisture Sensor**
- **Photoresistor**
- **MAX17043 Fuel Gauge**
- **Push Button**
- **LED Indicator**

## Setup Instructions

### Prerequisites

- **ZephyrOS SDK:** Ensure you have the Zephyr development environment set up. [Zephyr Getting Started](https://docs.zephyrproject.org/latest/getting_started/index.html)
- **AWS IoT Core:** Set up your AWS IoT Core and obtain necessary certificates.
- **Hardware Connections:**
  - Connect the sensors to the appropriate GPIO and I2C pins on the Seeed Xiao ESP32C6.
  - Ensure the push button and LED are connected correctly.

### Configuration

1. **Clone the Repository:**

   ```bash
   git clone https://github.com/your-repo/plant_monitor.git
   cd plant_monitor
