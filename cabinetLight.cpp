/**
 * @file cabinetLight.cpp
 * @brief Implementierung der Steuerlogik für die Schrankbeleuchtung.
 *
 * Realisiert die Initialisierung der Hardware, PWM-Dimmung der LEDs und
 * das Interrupt-Handling für die Reedkontakte. Grundlage ist ein Raspberry Pi Pico W,
 * der bis zu vier LED-Gruppen über MOSFETs schaltet und dimmt.
 *
 * Features:
 * - Bis zu 4 separat schaltbare LED-Gruppen
 * - Automatische Steuerung über Magnetsensoren oder Reed-Schalter
 * - Geringe Standby-Leistung durch Low-RDS(on)-MOSFETs
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-06-28
 * @copyright MIT
 */

#include "cabinetLight.h"   // Header-Datei der CabinetLight-Klasse
#include <algorithm>
#include <cstdio>

// Definition der statischen Instanz
CabinetLight* CabinetLight::instance = nullptr;

/**
 * @brief Konstruktor der CabinetLight-Klasse
 * Initialisiert die GPIO-Pins und PWM-Ausgänge für die LEDs
 */ 
CabinetLight::CabinetLight() {

    // Initialisiere die Standard-Ein/Ausgabe (nur einmal nötig, aber für Debug hier belassen)
    stdio_init_all();
    printf("[DEBUG] CabinetLight Konstruktor aufgerufen.\n");

    instance = this; // Singleton-Instanz setzen

    // Initialisiere die GPIO-Pins für die LEDs
    for (int gpio : LED_PINS) {

        setupPwmLEDs(gpio);
    }

    // Initialisiere die GPIO-Pins für die Sensoren
    for (int gpio : SENSOR_PINS) {
        
        setupSensors(gpio);
    }

    // Debug-Ausgabe, um den Abschluss des Konstruktors zu bestätigen
    printf("[DEBUG] CabinetLight Konstruktor abgeschlossen.\n");
}

/**
 * @brief Initialisiert den PWM-Ausgang für eine LED
 * @param gpio GPIO-Pin für die LED
 */
void CabinetLight::setupPwmLEDs(uint8_t gpio) {

    // Debug-Ausgabe, um den GPIO-Pin anzuzeigen, der für PWM konfiguriert wird
    printf("[DEBUG] setupPwmLEDs: Konfiguriere PWM für GPIO %d\n", gpio);

    // Initialisiere den GPIO-Pin für PWM

    // Setze den GPIO-Pin auf die PWM-Funktion
    // Dies ist notwendig, um den GPIO-Pin als PWM-Ausgang zu konfigurieren.
    printf("[DEBUG] setupPwmLEDs: Setze GPIO %d auf PWM-Funktion\n", gpio);
    // gpio_set_function ist eine Funktion, die den GPIO-Pin auf die gewünschte Funktion setzt.
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    // Initialisiere den GPIO-Pin
    // Dies ist notwendig, um den GPIO-Pin für die PWM-Funktion vorzubereiten.
    printf("[DEBUG] setupPwmLEDs: Initialisiere GPIO %d\n", gpio);
    // gpio_init ist eine Funktion, die den GPIO-Pin initialisiert.
    gpio_init(gpio);

    // Setze den GPIO-Pin als Ausgang
    printf("[DEBUG] setupPwmLEDs: Setze GPIO %d als Ausgang\n", gpio);
    // gpio_set_dir ist eine Funktion, die den GPIO-Pin als Eingang oder Ausgang konfiguriert.
    // Hier wird der GPIO-Pin als Ausgang gesetzt, da er für PWM-Ausgaben verwendet
    gpio_set_dir(gpio, GPIO_OUT);

    // Deaktiviere Pull-Up/Pull-Down-Widerstände
    // Dies ist wichtig, da PWM-Ausgänge keine Pull-Up/Pull-Down-Widerstände benötigen
    // und sie die PWM-Signale stören könnten.
    printf("[DEBUG] setupPwmLEDs: Deaktiviere Pull-Up/Pull-Down-Widerstände für GPIO %d\n", gpio);
    // gpio_set_pulls ist eine Funktion, die die Pull-Up/Pull-Down-W
    gpio_set_pulls(gpio, false, false);

    // Konfiguriere den PWM-Slice für den GPIO-Pin
    printf("[DEBUG] setupPwmLEDs: Konfiguriere PWM-Slice für GPIO %d\n", gpio);
    // Konvertiere den GPIO-Pin in den entsprechenden PWM-Slice
    uint slice = pwm_gpio_to_slice_num(gpio);

    // Setze die PWM-Konfiguration
    // Hier wird die Standardkonfiguration für den PWM-Slice abgerufen.
    printf("[DEBUG] setupPwmLEDs: Hole Standard-PWM-Konfiguration für Slice %d\n", slice);
    // pwm_get_default_config ist eine Funktion, die die Standard-PWM-Konfiguration zurückg
    pwm_config config = pwm_get_default_config();

    // Setze die PWM-Frequenz und den Wrap-Wert
    // Die Frequenz wird auf 8 kHz gesetzt, was für die meisten LED-Anwendungen geeignet ist.
    printf("[DEBUG] setupPwmLEDs: Setze PWM-Frequenz auf 8 kHz für Slice %d\n", slice);
    // pwm_config_set_clkdiv ist eine Funktion, die den Taktteiler für die
    pwm_config_set_clkdiv(&config, 125.0f);
    
    // Setze den Wrap-Wert für 16-bit PWM
    printf("[DEBUG] setupPwmLEDs: Setze Wrap-Wert auf %d für Slice %d\n", PWM_WRAP, slice);
    // pwm_config_set_wrap ist eine Funktion, die den Wrap-Wert für die PWM-Ausgabe konfiguriert.
    pwm_config_set_wrap(&config, PWM_WRAP);

    // Setze den PWM-Kanal für den GPIO-Pin
    printf("[DEBUG] setupPwmLEDs: Setze PWM-Kanal für GPIO %d\n", gpio);
    // pwm_gpio_to_channel ist eine Funktion, die den GPIO-Pin in den entsprechenden PWM
    pwm_init(slice, &config, true);

    // Setze den PWM-Ausgang auf 0 (ausgeschaltet)  
    printf("[DEBUG] setupPwmLEDs: Setze PWM-Ausgang für GPIO %d auf 0\n", gpio);
    // pwm_set_gpio_level ist eine Funktion, die den PWM-Ausgang auf den angegebenen
    pwm_set_gpio_level(gpio, 0);
    
    // Aktiviere den PWM-Ausgang
    printf("[DEBUG] setupPwmLEDs: Aktiviere PWM-Ausgang für Slice %d\n", slice);
    // pwm_set_enabled ist eine Funktion, die den PWM-Ausgang aktiviert.
    pwm_set_enabled(slice, true);
}

/**
 * @brief Initialisiert die GPIO-Pins für die Sensoren
 * @param gpio GPIO-Pin für den Sensor
 */
void CabinetLight::setupSensors(uint8_t gpio) {

    // Debug-Ausgabe, um den GPIO-Pin anzuzeigen, der für den Sensor konfiguriert wird
    printf("[DEBUG] setupSensors: Konfiguriere Sensor GPIO %d\n", gpio);

    // Setze den GPIO-Pin als Eingang
    printf("[DEBUG] setupSensors: Setze GPIO %d als Eingang\n", gpio);
    // gpio_init ist eine Funktion, die den GPIO-Pin initialisiert.
    gpio_init(gpio);

    // Setze den GPIO-Pin als Eingang für die Reedkontakte
    printf("[DEBUG] setupSensors: Setze GPIO %d als Eingang\n", gpio);
    // gpio_set_dir ist eine Funktion, die den GPIO-Pin als Eingang oder Ausgang konfiguriert.
    gpio_set_dir(gpio, GPIO_IN);

    // Aktiviere Pull-Up-Widerstände
    // Reedkontakte benötigen in der Regel Pull-Up-Widerstände, um im offenen Zustand einen definierten Zustand zu haben
    printf("[DEBUG] setupSensors: Aktiviere Pull-Up-Widerstände für GPIO %d\n", gpio);
    // gpio_pull_up ist eine Funktion, die den Pull-Up-Widerstand für den GPIO
    gpio_pull_up(gpio);

    // Finde den Index von gpio im SENSOR_PINS-Array (mit std::find)
    auto it = std::find(SENSOR_PINS.begin(), SENSOR_PINS.end(), gpio);

    // Wenn der GPIO-Pin in SENSOR_PINS gefunden wurde, aktualisiere lastTriggerTime
    if (it != SENSOR_PINS.end()) {

        // Berechne den Index des gefundenen GPIO-Pins
        int index = std::distance(SENSOR_PINS.begin(), it);

        // Setze die letzte Triggerzeit für diesen Sensor
        printf("[DEBUG] setupSensors: Setze letzte Triggerzeit für GPIO %d auf aktuelle Zeit\n", gpio);
        // get_absolute_time() gibt die aktuelle Zeit in Mikrosekunden zurück
        lastTriggerTime[index] = get_absolute_time();
    }

    // Registriere den GPIO-Interrupt für den Sensor
    // Dies ermöglicht es, auf Änderungen des GPIO-Pins zu reagieren
    printf("[DEBUG] setupSensors: Registriere GPIO-Interrupt für GPIO %d\n", gpio);
    // gpio_set_irq_enabled_with_callback ist eine Funktion, die den GPIO-Pin für Interrupts konfiguriert.
    gpio_set_irq_enabled_with_callback(
        gpio,   // Aktiviere Interrupts für diesen GPIO-Pin
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,  // Reagiere auf fallende und steigende Flanken
        true,   // Aktiviere den Interrupt
        &CabinetLight::gpioCallback // Verwende die statische Callback-Funktion
    );

}

/**
 * @brief Dimmt die LED ein oder aus
 * @param gpio GPIO-Pin für die LED
 * @param on true, um die LED einzuschalten, false, um sie auszuschalten
 */
void CabinetLight::fadeLed(uint gpio, bool on) {

    // Debug-Ausgabe, um den GPIO-Pin und den Zustand der LED anzuzeigen
    printf("[DEBUG] fadeLed: GPIO %d, Zustand: %s\n", gpio, on ? "EIN" : "AUS");

    // Überprüfe, ob der GPIO-Pin gültig ist
    if (gpio < 0 || gpio >= DEV_COUNT) {
        printf("[ERROR] fadeLed: Ungültiger GPIO-Pin %d\n", gpio);
        return; // Ungültiger GPIO-Pin, nichts tun
    }   

    // Konvertiere den GPIO-Pin in den entsprechenden PWM-Slice und Kanal
    uint slice = pwm_gpio_to_slice_num(gpio);
    uint channel = pwm_gpio_to_channel(gpio);

    // Setze den PWM-Kanal auf den gewünschten Zustand
    // Wenn 'on' wahr ist, dimme die LED hoch, andernfalls dimme sie runter
    if (on) {

        // Setze den PWM-Kanal auf den Startwert (0)
        pwm_set_chan_level(slice, channel, 0);

        // Dimme die LED hoch
        for (uint16_t level = 0; level <= PWM_WRAP; level += 1000) {
            // Setze den PWM-Kanal auf den aktuellen Level
            // Dies erhöht die Helligkeit der LED schrittweise
            pwm_set_chan_level(slice, channel, level);
            // Warte kurz, um die Helligkeit zu erhöhen
            // Dies ermöglicht ein sanftes Hochdimmen der LED
            sleep_ms(2);
        }
    } 
    else {
        // Dimme die LED runter
        for (int level = PWM_WRAP; level >= 0; level -= 1000) {
            // Setze den PWM-Kanal auf den aktuellen Level
            // Dies verringert die Helligkeit der LED schrittweise
            pwm_set_chan_level(slice, channel, level);
            // Warte kurz, um die Helligkeit zu reduzieren
            // Dies ermöglicht ein sanftes Herunterdimmen der LED
            sleep_ms(2);
        }
    }
}


/**
 * @brief Statischer GPIO-Interrupt-Handler (leitet an Instanz weiter)
 * @param gpio GPIO-Pin, der den Interrupt ausgelöst hat
 * @param events Ereignisse, die den Interrupt ausgelöst haben
 */
void CabinetLight::gpioCallback(uint gpio, uint32_t events) {

    // Debug-Ausgabe, um den GPIO-Pin und die Ereignisse anzuzeigen
    printf("[DEBUG] gpioCallback: GPIO %d, Events: 0x%08x\n", gpio, events);
    
    // Überprüfe, ob die Instanz existiert
    if (instance) {

        // Rufe die Instanzmethode auf, um die Logik zu verarbeiten
        instance->handleGpioCallback(gpio, events);
    }
}


/**
 * @brief Verarbeitet den GPIO-Interrupt für die Sensoren
 * @param gpio GPIO-Pin, der den Interrupt ausgelöst hat
 * @param events Ereignisse, die den Interrupt ausgelöst haben
 */
void CabinetLight::handleGpioCallback(uint gpio, uint32_t events) {

    // Debug-Ausgabe, um den GPIO-Pin und die Ereignisse anzuzeigen
    printf("[DEBUG] handleGpioCallback: GPIO %d, Events: 0x%08x\n", gpio, events);

    // Überprüfe, ob der GPIO-Pin gültig ist
    if (gpio < 0 || gpio >= DEV_COUNT) {

        printf("[ERROR] handleGpioCallback: Ungültiger GPIO-Pin %d\n", gpio);
        return; // Ungültiger GPIO-Pin, nichts tun
    }

    // Iteriere über alle Sensor-Pins
    // und prüfe, ob der GPIO-Pin mit einem der Sensor-Pins übereinstimmt
    for (int i = 0; i < DEV_COUNT; ++i) {
        
        // Überprüfe, ob der GPIO-Pin mit einem der Sensor-Pins übereinstimmt
        if (gpio == SENSOR_PINS[i]) {

            // Speichere die aktuelle Zeit
            absolute_time_t now = get_absolute_time();

            // Prüfe, ob die Sperrzeit abgelaufen ist
            if (absolute_time_diff_us(lastTriggerTime[i], now) < DEBOUNCE_MS * 1000) {
                return; // Sperrzeit aktiv
            }

            // Aktualisiere die letzte Triggerzeit
            // Dies verhindert, dass der Handler zu oft aufgerufen wird
            lastTriggerTime[i] = now;

            // Überprüfe den Zustand des Reedkontakts
            // Ein Reedkontakt ist geschlossen, wenn der GPIO-Pin auf 0 ist
            bool door_open = gpio_get(gpio) == 0; // Reedkontakt geschlossen = Tür offen

            // Überprüfe den Zustand des Reedkontakts
            // Wenn die Tür offen ist und die LED nicht leuchtet, dimme sie hoch
            // Wenn die Tür geschlossen ist und die LED leuchtet, dimme sie runter
            if (door_open && !ledState[i]) {

                // Dimme die LED hoch
                fadeLed(LED_PINS[i], true);
                // Setze den LED-Zustand auf wahr
                ledState[i] = true;
            } 
            // Wenn die Tür geschlossen ist und die LED leuchtet, dimme sie runter
            else if (!door_open && ledState[i]) {
            
                // Dimme die LED runter
                fadeLed(LED_PINS[i], false);
                // Setze den LED-Zustand auf falsch
                // Dies schaltet die LED aus
                ledState[i] = false;
            }
        }
    }

    // Debug-Ausgabe, um den Abschluss der Interrupt-Verarbeitung zu bestätigen
    printf("[DEBUG] handleGpioCallback: Interrupt-Verarbeitung für GPIO %d abgeschlossen.\n", gpio);

}