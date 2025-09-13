/**
 * @file main.cpp
 * @brief Firmware-Einstiegspunkt für die automatisierte Schrankbeleuchtung.
 *
 * Dieses Programm steuert bis zu vier LED-Gruppen in einem Schrank automatisch
 * über Reedkontakte (Magnetsensoren) und MOSFETs mit einem Raspberry Pi Pico W.
 * Die Firmware übernimmt die Initialisierung der Hardware, die Konfiguration der
 * Sensoren und LEDs sowie die Verarbeitung von Sensorereignissen, um die LEDs
 * abhängig vom Türzustand sanft ein- und auszublenden (PWM-Dimmung).
 *
 * Features:
 * - Bis zu 4 separat schaltbare LED-Gruppen (z. B. für mehrere Türen/Fächer)
 * - Automatische Steuerung über Reedkontakte (Magnetsensoren)
 * - PWM-Dimmung für sanftes Ein-/Ausschalten der LEDs
 * - Geringe Standby-Leistung durch effiziente MOSFETs
 * - Heartbeat-LED zur Laufzeitüberwachung
 * - Erweiterbare Hardware-Anschlüsse
 *
 * Hardware-Anforderungen:
 * - Raspberry Pi Pico W
 * - 12V DC Versorgung (intern auf 5V/3.3V geregelt)
 * - MOSFETs für LED-Schaltung
 * - Reedkontakte an den Türen
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-06-28
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

    // === 1. Debug-Ausgabe und USB-Initialisierung ===
    stdio_init_all(); ///< Initialisiere USB-CDC für serielle Debug-Ausgaben
    sleep_ms(200);    ///< Warte, damit der Host Zeit für USB-Enumeration hat
    printf("[DEBUG] Firmware-Start.\n");

    // === 2. Boot-Blink auf der Onboard-LED (GPIO25) ===
    // Zeigt an, dass die Firmware erfolgreich gestartet ist
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 3; ++i) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1); // LED an
        sleep_ms(150);
        gpio_put(PICO_DEFAULT_LED_PIN, 0); // LED aus
        sleep_ms(150);
    }

    // === 3. Interrupts für GPIO-Bank 0 aktivieren ===
    // Ermöglicht die Verarbeitung von Sensorereignissen über IRQs
    irq_set_enabled(IO_IRQ_BANK0, true);


    // === 4. CabinetLight-Instanz erstellen und konfigurieren ===
    static CabinetLight cabinetLightInstance;
    CabinetLight *cabinetLight = &cabinetLightInstance;

    // === 3a. Polling-Fallback explizit deaktivieren (nur IRQ-Betrieb) ===
    // Für reinen IRQ-Betrieb: Polling-Fallback bleibt aus (Standard: false)
    // Zum Aktivieren: cabinetLight->setPollingFallback(true);
    cabinetLight->setPollingFallback(false);

    // === 5. Initialisierungsstatus prüfen ===
    if (!cabinetLight->isInitialized()) {
        printf("[FATAL] Fehler bei der Initialisierung der CabinetLight-Hardware!\n");
        while (true) {
            // Fehleranzeige: Onboard-LED blinkt schnell
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            sleep_ms(100);
            gpio_put(PICO_DEFAULT_LED_PIN, 0);
            sleep_ms(100);
        }
    }

    // === 6. Sensor-Polarity setzen ===
    std::array<bool, CabinetLight::DEV_COUNT> sensorPol = {true, true, true, true};
    cabinetLight->setSensorPolarity(sensorPol);
    printf("[TEST] Sensor polarity set to active-low (true for active-low)\n");

    // === 7. Startup-Test: LEDs nacheinander blinken lassen ===
    cabinetLight->runStartupTest();

    // === 8. Heartbeat-LED-Setup ===
    // Die Onboard-LED blinkt alle 1s als Lebenszeichen
    absolute_time_t hb_last = get_absolute_time();
    bool hb_state = false;

    // === 9. Hauptschleife ===
    while (true) {

        // --- 9.1. Verarbeite Sensor- und LED-Events ---
        cabinetLight->process();

        // --- 9.2. Heartbeat-LED toggeln (alle 1s) ---
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(hb_last, now) > CabinetLight::HEARTBEAT_INTERVAL_MS * 1000) {
            hb_last = now;
            hb_state = !hb_state;
            gpio_put(PICO_DEFAULT_LED_PIN, hb_state);
        }

        // --- 9.3. CPU-Entlastung durch kurze Pause ---
        sleep_ms(50);
    }
}