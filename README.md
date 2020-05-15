# Projekt: Davis zu GSM Gateway

In diesem Projekt entwickle ich für unseren Surfverein (http://www.lsvsw-surfen.de/) eine Schaltung+Software, welche die Wetterdaten der Davis-Wetterstation empfängt, decodiert und via GSM an einen Server schickt, der die Auswertung auf unserer Webseite darstellt.

Ziele:
* Zuverlässige integrierte Schaltung
* Betrieb an einer Autobatterie (11-13V)
* Niedriger Stromverbrauch für Solarbetrieb (~0,5 Watt)
* Preiswert nachzubauen

## Hardware

### Spannungswandler

LM2596S (http://www.ti.com/lit/ds/symlink/lm2596.pdf) als fertiges Modul

### Controller

STM32F103C8 "Blue Pill" mit Arduino Firmware (http://wiki.stm32duino.com/index.php?title=Blue_Pill)

### ISM-Decoder

RFM69W (https://www.mikrocontroller.net/articles/RFM69)

### GSM-Modem

SIM800L (https://nettigo.eu/products/sim800l-gsm-grps-module)

### 433 MHz Empfänger

* Optional für zusätzlichen Temperatursensor
* Besseres Modul mit SYN470R Chip
* Außenantenne

### Sonstiges

* Lochrasterplatine
* Diode mit ~1V Spannungsabfall (z.B. 1N400x, https://www.diodes.com/assets/Datasheets/ds28002.pdf)

# Bauplan

* Lochrasterplatine mit freier Verkabelung

### Spannungswandler

* Eingangsspannung am Spannungswandler: 7-30V
* Der Spannungswandler wird auf eine Ausgangsspannung von 4,8V eingestellt und am 5V Eingang sowie Masse des Controllers angeschlossen

### RFM69 an Controller

| RFM69W      | STM32F103C8 |
| ----------- | ----------- |
| GND         | GND         |
| 3.3V        | 3.3V        |
| NSS (CS/SS) | A4          |
| SCK         | A5          |
| MISO        | A6          |
| MOSI        | A7          |

Zusätzlich muss am ANT eine passende Antenne angeschlossen werden (17,3 cm Klingeldraht, auf einem Stift aufgerollt, dass er etwas kompakter ist). Ohne die Antenne beträgt die Empfangsreichweite nur wenige Meter!

### GSM-Modem

* Mittels der Diode wird die Spannung auf ~4V reduziert, dies dient als Eingangsspannung für das GSM-Modul
* Das Modem wird an den seriellen Port 2 (Pins A2, A3) angeschlossen
* An Pin A1 wird über einen Transistor der Reset-Pin des Modems angeschlossen. Ein Pullup-Widerstand sorgt dafür, dass Reset auf HIGH ist. Über A1 kann es auf LOW gezogen und damit ein Reset ausgelöst werden.

| GSM800      | STM32F103C8 |
| ----------- | ----------- |
| GND         | GND         |
| RX          | A2          |
| TX          | A3          |

### 433 MHz-Empfänger

* Data an B8
* GND an GND
* VCC an 3.3V

# Software

Voraussetzung ist eine Arduino-IDE mit den Treibern und Libs für das STM32-Board (STM32duino-Projekt, https://github.com/stm32duino/Arduino_Core_STM32)

## Bibliotheken

* Davis Decoder-Bibliothek https://github.com/dekay/DavisRFM69

