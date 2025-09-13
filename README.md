# 💡 Schrankbeleuchtung – Firmware für den Raspberry Pi Pico W


Automatisierte LED-Beleuchtung für Schränke, realisiert mit einem Raspberry Pi Pico W. Die Firmware steuert bis zu vier LED-Gruppen über MOSFETs und wertet Türsensoren (Reedkontakte) aus, um die Beleuchtung automatisch zu schalten und sanft zu dimmen.

**Neu:**
- Ausführliche Code-Kommentare und Doxygen-Dokumentation (siehe Quellcode)
- Logging-API mit LogLevel (ERROR, WARN, INFO, DEBUG)
- Robuste Fehlerbehandlung mit LED-Signalisierung
- Hinweise zur Thread-Sicherheit und atomaren Event-Verarbeitung

**Die zugehörige elektronische Schaltung (Schaltplan und Layout) findest du im separaten Repository:**  
👉 [Illuminux/Schrankbeleuchtung-KiCad](https://github.com/Illuminux/Schrankbeleuchtung-KiCad.git)

---


## 🧩 Features

- Bis zu 4 separat schaltbare LED-Gruppen
- Automatische Steuerung über Magnetsensoren oder Reed-Schalter
- PWM-Dimmung für sanftes Ein- und Ausschalten der LEDs
- Flexible Pinbelegung für LEDs und Sensoren (per Software konfigurierbar)
- Sensor-Polarity (active-low/active-high) individuell einstellbar
- IRQ-basierte Ereignisverarbeitung mit atomarer Bitmaske, Fallback auf Polling
- Umfangreiche Logging-API (LogLevel wählbar)
- Fehlerbehandlung mit LED-Signalisierung (fatalErrorBlink)
- Doxygen-Dokumentation und ausführliche Code-Kommentare
- Versorgung mit 12 V DC, intern auf 5 V und 3.3 V geregelt
- Geringe Standby-Leistung durch Low-RDS(on)-MOSFETs
- Erweiterbare Anschlüsse über XH-Steckerleisten

---


## ⚙️ Architektur & Hinweise

- **PWM-Frequenz:** 1 kHz (PWM_WRAP = 12500, 12 Bit Auflösung)
- **Fading:** Dimmzeit von 0 auf 100 % ca. 125 ms (bei Standard-Setup)
- **Sensor-GPIOs:** mit Pull-Down und Interrupt (kein Pull-Up!)
- **Flexible API:** Pinbelegung und Sensor-Polarity zur Laufzeit änderbar
- **IRQ-Handling:** Singleton-Pattern, atomare Bitmasken für sichere Event-Verarbeitung
- **Polling-Fallback:** Falls IRQs verloren gehen, werden Sensoren regelmäßig gepollt
- **Sanftes Dimmen:** LEDs werden beim Öffnen/Schließen der Tür sanft ein- und ausgeblendet
- **Logging:** Umfangreiche Logging-API mit LogLevel (ERROR, WARN, INFO, DEBUG)
- **Fehlerbehandlung:** Fehler werden per LED und Log ausgegeben (fatalErrorBlink)
- **Thread-Sicherheit:** Atomare Event-Flags, Hinweise im Code (siehe Doxygen)
## 📝 Beispiel: Nutzung der API

```cpp
#include "cabinetLight.h"

int main() {
   stdio_init_all();
   CabinetLight light;
   if (!light.isInitialized()) {
      CabinetLight::fatalErrorBlink();
   }
   light.setSensorPolarity({true, true, true, true});
   light.runStartupTest();
   while (true) {
      light.process();
   }
}
```

Weitere Beispiele und Hinweise findest du direkt im Quellcode (ausführliche Kommentare und Doxygen).

---

## 🔌 GPIO-Belegung (Pico W)

| Funktion         | GPIO     |
|------------------|----------|
| LED 1 (Q1)       | GPIO02   |
| LED 2 (Q2)       | GPIO03   |
| LED 3 (Q3)       | GPIO04   |
| LED 4 (Q4)       | GPIO05   |
| Sensor 1         | GPIO06   |
| Sensor 2         | GPIO07   |
| Sensor 3         | GPIO08   |
| Sensor 4         | GPIO09   |

> **Hinweis:** Die tatsächliche Zuordnung kann je nach Hardware-Revision abweichen. Siehe Schaltplan und `cabinetLight.h`.

---

## 🛠 Komponenten (Auszug)

| Bauteil      | Typ / Wert              | Bestellnummer (Mouser)  |
|--------------|-------------------------|--------------------------|
| A1           | Raspberry Pi Pico W     | 358-SC0918               |
| U1           | TPS560430XDBVT          | 595-TPS560430XDBVT       |
| Q1–Q4        | IRLML0030TRPBF          | 942-IRLML0030TRPBF       |
| R1–R4        | 187 Ω Widerstände       | 667-ERA-6AEB1870V        |
| R5–R9        | 10 kΩ Pull-Ups          | 667-ERA-6APB103V         |
| C1–C3        | 10 µF / 22 µF / 0.1 µF  | siehe BOM                |
| L1           | 10 µH Spule             | 81-LQH44PN100MPRL        |
| J2–J9        | XH-Stecker + Buchsen    | 306-B2B-XH-AMLFSNP       |

Die vollständige Stückliste sowie Schaltplan und Layout befinden sich im [Schrankbeleuchtung-KiCad-Repository](https://github.com/Illuminux/Schrankbeleuchtung-KiCad.git).

---

## ⚙️ Firmware

### Aufbau

- **main.cpp**: Einstiegspunkt, Initialisierung und Hauptschleife
- **cabinetLight.h/cpp**: Zentrale Steuerlogik für LEDs und Sensoren

### Kompilieren & Flashen

1. **Abhängigkeiten:**  
   - [Pico SDK](https://github.com/raspberrypi/pico-sdk) installiert und eingerichtet
   - CMake ≥ 3.13
   - GCC-Toolchain für ARM

2. **Build:**
   ```sh
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Flashen:**  
   Die erzeugte `.uf2`-Datei auf den Pico W kopieren (BOOTSEL-Modus).

---


## 🧠 Firmware-Hinweise & Dokumentation

- LED-GPIOs werden als PWM-Ausgänge initialisiert
- Sensor-GPIOs mit Pull-Down und Interrupt/Debounce
- Bei Türöffnung wird die zugehörige LED sanft hochgedimmt, beim Schließen heruntergedimmt
- Die Steuerung erfolgt vollständig interruptbasiert für schnelle Reaktion und niedrigen Stromverbrauch
- Flexible Anpassung der Pinbelegung und Sensorlogik per Software
- Umfangreiche Doxygen-Kommentare und Hinweise zu Thread-Sicherheit im Code

---


## 📁 Verzeichnisstruktur (Auszug)

```
Schrankbeleuchtung/
├── cabinetLight.cpp
├── cabinetLight.h
├── main.cpp
├── CMakeLists.txt
├── README.md
└── ...
```

---


## 📚 Dokumentation generieren (optional)

Mit Doxygen kannst du eine HTML-Dokumentation aus den Quelltext-Kommentaren erzeugen:

```sh
doxygen Doxyfile
```

Siehe Doxygen-Website für Details zur Konfiguration.

---

## ⚖️ Lizenz

Dieses Projekt steht unter der MIT-Lizenz.  
Feel free to modify, use & share!

---

**Autor:** Knut Welzel  
**Kontakt:** knut.welzel@gmail.com
