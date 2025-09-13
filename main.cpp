/**
 * @file main.cpp
 * @brief Einstiegspunkt der Firmware für die Schrankbeleuchtung (Raspberry Pi Pico W)
 *
 * Diese Firmware steuert bis zu vier LED-Gruppen in einem Schrank oder Möbelstück
 * automatisch über Reedkontakte (Magnetsensoren) und MOSFETs. Die LEDs werden per
 * PWM sanft ein- und ausgeblendet. Die Steuerung erfolgt ereignisbasiert über GPIO-Interrupts
 * (IRQ) – ein optionales Polling-Fallback kann aktiviert werden. Die Firmware bietet
 * robuste Fehlerbehandlung, flexible Sensor-Polarity, einen Startup-Test und eine Heartbeat-LED.
 *
 * Hauptfunktionen:
 * - Automatische Lichtsteuerung für bis zu 4 Türen/Fächer
 * - Reedkontakte (Magnetsensoren) als Türsensoren (active-low, konfigurierbar)
 * - PWM-Dimmung für sanftes Ein-/Ausschalten (Fading)
 * - IRQ-basiertes Event-Handling (Polling-Fallback optional)
 * - Fehlerbehandlung mit LED-Signalisierung
 * - Heartbeat-LED als Lebenszeichen
 * - Startup-Test für alle LED-Kanäle
 * - Umfangreiche Logging-API mit LogLevel
 *
 * Hardware-Anforderungen:
 * - Raspberry Pi Pico W
 * - 12V DC Versorgung (intern auf 5V/3.3V geregelt)
 * - MOSFETs für LED-Schaltung
 * - Reedkontakte an den Türen (GPIO)
 *
 * Besonderheiten:
 * - Alle Zeit- und Fading-Parameter als constexpr zentral definiert
 * - Modularer Aufbau: CabinetLight-Klasse kapselt alle Steuerungsfunktionen
 * - Einfache Erweiterbarkeit für weitere Kanäle oder Sensorlogik
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-09-13
 * @copyright MIT
 */

#include "cabinetLight.h"
#include "hardware/irq.h"
#include <cstdio>

/**
 * @brief Hauptfunktion: Initialisiert Hardware und steuert die Schrankbeleuchtung.
 *
 * - Initialisiert USB-CDC für Debug-Ausgaben
 * - Führt einen Boot-Blink auf der Onboard-LED aus
 * - Aktiviert GPIO-Interrupts für die Sensoren
 * - Erstellt und konfiguriert die CabinetLight-Instanz
 * - Setzt die Sensor-Polarity (active-low)
 * - Führt einen Startup-Test der LEDs aus
 * - Startet die Hauptschleife mit periodischer Event-Verarbeitung und Heartbeat-LED
 *
 * @return int Rückgabewert (0 bei Erfolg)
 */
int main() {

    // 1. USB-CDC initialisieren (ermöglicht printf-Debug-Ausgaben über USB)
    stdio_init_all();
    sleep_ms(200); // Warten, damit Host Zeit für USB-Enumeration hat
    printf("[DEBUG] Firmware-Start.\n");

    // 2. Boot-Blink: Onboard-LED blinkt 3x als Lebenszeichen nach Reset
    CabinetLight::blinkOnboardLed(3, 150, 150);

    // 3. GPIO-Interrupts für Sensoren aktivieren (ermöglicht IRQ-basiertes Event-Handling)
    irq_set_enabled(IO_IRQ_BANK0, true);

    // 4. CabinetLight-Instanz erzeugen und konfigurieren
    //    - Kapselt alle Logik für Sensoren, LEDs, PWM, Fading, Fehlerbehandlung
    static CabinetLight cabinetLightInstance;
    CabinetLight *cabinetLight = &cabinetLightInstance;
    cabinetLight->setPollingFallback(false); // Polling-Fallback deaktiviert (nur IRQ-Betrieb)

    // 5. Initialisierung prüfen: Bei Fehler Endlosschleife mit Fehler-Blink
    if (!cabinetLight->isInitialized()) {
        printf("[FATAL] Fehler bei der Initialisierung der CabinetLight-Hardware!\n");
        CabinetLight::fatalErrorBlink();
    }

    // 6. Sensor-Polarity setzen: Alle Sensoren als active-low (Reedkontakt schließt gegen Masse)
    cabinetLight->setSensorPolarity({true, true, true, true});
    printf("[TEST] Sensor polarity set to active-low (true für active-low)\n");

    // 7. Startup-Test: LEDs nacheinander blinken lassen (zeigt Funktion aller Kanäle)
    cabinetLight->runStartupTest();

    // 8. Hauptschleife: Event-Verarbeitung und Heartbeat-LED
    //    - process(): verarbeitet Sensor- und LED-Events, Fading, IRQs
    //    - Heartbeat: Onboard-LED blinkt im Sekundentakt als Lebenszeichen
    absolute_time_t hb_last = get_absolute_time();
    bool hb_state = false;

    // Hauptschleife: Verarbeitet Events und steuert Heartbeat
    while (true) {
        // Event-Verarbeitung
        cabinetLight->process();
        // Heartbeat-LED toggeln (alle 1s)
        absolute_time_t now = get_absolute_time();
        // Zeitdifferenz berechnen
        if (absolute_time_diff_us(hb_last, now) > CabinetLight::HEARTBEAT_INTERVAL_MS * 1000) {
            hb_last = now;
            hb_state = !hb_state;
            // Onboard-LED setzen
            gpio_put(PICO_DEFAULT_LED_PIN, hb_state);
        }
        sleep_ms(50); // Kurze Pause zur CPU-Entlastung
    }
}