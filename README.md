# Schrankbeleuchtung mit Raspberry Pi Pico W

## Projektübersicht
Dieses Projekt implementiert eine Schrankbeleuchtung mit einem Raspberry Pi Pico W. Die Beleuchtung wird durch Reedkontakte gesteuert, die Türöffnungen und -schließungen erkennen. Die LED-Streifen werden über MOSFETs geschaltet und per PWM gedimmt.

## Hardwareliste
- Raspberry Pi Pico W – Steuerung via PWM und GPIO
- 4x IRLML6344TRPBF MOSFETs – LED-Treiber, je mit 100 Ω Gate-Widerstand (RQ73C2A100RBTD)
- 4x LED-Streifen (12 V) – angeschlossen über B2B-XH-AMLFSNP (Pin 1 = 12 V, Pin 2 = Drain)
- 4x Reedkontakte – angeschlossen über B2B-XH-AMLFSNP (Pin 1 = GPIO, Pin 2 = GND), je mit 10 kΩ Pull-up gegen 3,3 V
- LM2596SX-5.0NOPB – 5 V-Festspannungsregler
  - CIN: 470 µF Elko (EEE-FKE101XAL)
  - COUT: 100 µF MLCC (GRM32ER61A107ME0L)
  - Spule: 33 µH (SPM7054VC-330M-D)
  - Schottky-Diode: SS34 (Sperrrichtung gegen GND)

## Funktion
- Reedkontakte erkennen Türöffnung/-schließung.
- Pico steuert LED-Fading per PWM.
- LED-Streifen werden über MOSFETs geschaltet.
- Versorgung: 12 V Netzteil → 5 V über LM2596 für Pico.

## Aufbau
1. Verkabelung der Komponenten gemäß Hardwareliste.
2. Anschluss der LED-Streifen und Reedkontakte an die GPIO-Pins des Pico.
3. Installation des LM2596-Festspannungsreglers für die Stromversorgung.

## Software

### Hauptprogramm
Das Hauptprogramm (`main.cpp`) steuert die Schrankbeleuchtung. Es verwendet GPIO-Interrupts, um Türbewegungen zu erkennen, und PWM, um die LED-Streifen sanft zu dimmen.

### Kompilierung mit Pico SDK
1. Pico SDK einrichten: Folge der offiziellen Anleitung zur Einrichtung der Pico SDK https://github.com/raspberrypi/pico-sdk
2. Projektstruktur:
   - `CMakeLists.txt`
   - `main.cpp`
3. Kompilieren mit CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   make

## Nutzung
1. Kompiliere das Projekt und erstelle die .uf2-Datei.
2. Flashe die .uf2-Datei auf den Raspberry Pi Pico W.
3. Verbinde die Hardware gemäß dem Aufbau.
4. Öffne und schließe die Türen, um die LED-Beleuchtung zu testen.