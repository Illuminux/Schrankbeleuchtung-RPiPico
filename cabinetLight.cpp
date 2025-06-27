
#include "cabinetLight.hpp"
#include <algorithm>
#include "hardware/gpio.h"

// Definition der statischen Instanz
CabinetLight* CabinetLight::instance = nullptr;

CabinetLight::CabinetLight() {
    instance = this; // Singleton-Instanz setzen

    // Initialisiere die Standard-Ein/Ausgabe
    stdio_init_all();

    // Initialisiere die GPIO-Pins für die LEDs
    for (int gpio : LED_PINS) {
        setupPwmLEDs(gpio);
    }

    // Initialisiere die GPIO-Pins für die Sensoren
    for (int gpio : SENSOR_PINS) {
        setupSensors(gpio);
    }
}

/**
 * @brief Initialisiert den PWM-Ausgang für eine LED
 * @param gpio GPIO-Pin für die LED
 */
void CabinetLight::setupPwmLEDs(uint8_t gpio) {

    // Initialisiere den GPIO-Pin für PWM

    // Setze den GPIO-Pin auf die PWM-Funktion
    // Dies ist notwendig, um den GPIO-Pin als PWM-Ausgang zu konfigurieren.
    gpio_set_function(gpio, GPIO_FUNC_PWM);   
    
    // Initialisiere den GPIO-Pin
    // Dies ist notwendig, um den GPIO-Pin für die PWM-Funktion vorzubereiten.
    gpio_init(gpio);

    // Setze den GPIO-Pin als Ausgang
    gpio_set_dir(gpio, GPIO_OUT);

    // Deaktiviere Pull-Up/Pull-Down-Widerstände
    // Dies ist wichtig, da PWM-Ausgänge keine Pull-Up/Pull-Down-Widerstände benötigen
    // und sie die PWM-Signale stören könnten.
    gpio_set_pulls(gpio, false, false);

    // Konfiguriere den PWM-Slice für den GPIO-Pin
    uint slice = pwm_gpio_to_slice_num(gpio);

    // Setze die PWM-Konfiguration
    // Hier wird die Standardkonfiguration für den PWM-Slice abgerufen.
    pwm_config config = pwm_get_default_config();

    // Setze die PWM-Frequenz und den Wrap-Wert
    // Die Frequenz wird auf 8 kHz gesetzt, was für die meisten LED-Anwendungen geeignet ist.
    pwm_config_set_clkdiv(&config, 125.0f);
    
    // Setze den Wrap-Wert für 16-bit PWM
    pwm_config_set_wrap(&config, PWM_WRAP);

    // Setze den PWM-Kanal für den GPIO-Pin
    pwm_init(slice, &config, true);

    // Setze den PWM-Ausgang auf 0 (ausgeschaltet)
    pwm_set_gpio_level(gpio, 0);
    
    // Aktiviere den PWM-Ausgang
    pwm_set_enabled(slice, true);
}

/**
 * @brief Initialisiert die GPIO-Pins für die Sensoren
 * @param gpio GPIO-Pin für den Sensor
 */
void CabinetLight::setupSensors(uint8_t gpio) {

    // Setze den GPIO-Pin als Eingang
    gpio_init(gpio);

    // Setze den GPIO-Pin als Eingang für die Reedkontakte
    gpio_set_dir(gpio, GPIO_IN);

    // Deaktiviere Pull-Down-Widerstände
    // Reedkontakte benötigen in der Regel Pull-Up-Widerstände, 
    // um im offenen Zustand einen definierten Zustand zu haben
    gpio_pull_up(gpio);

    // Finde den Index von gpio im SENSOR_PINS-Array (mit std::find)
    auto it = std::find(SENSOR_PINS.begin(), SENSOR_PINS.end(), gpio);

    // Wenn der GPIO-Pin in SENSOR_PINS gefunden wurde, aktualisiere lastTriggerTime
    if (it != SENSOR_PINS.end()) {
        // Berechne den Index des gefundenen GPIO-Pins
        int index = std::distance(SENSOR_PINS.begin(), it);
        // Setze die letzte Triggerzeit für diesen Sensor
        lastTriggerTime[index] = get_absolute_time();
    }

    /*
    // Registriere den GPIO-Interrupt für den Sensor
    // Dies ermöglicht es, auf Änderungen des GPIO-Pins zu reagieren
    gpio_set_irq_enabled_with_callback(
        gpio,   // Aktiviere Interrupts für diesen GPIO-Pin
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,  // Reagiere auf fallende und steigende Flanken
        true,   // Aktiviere den Interrupt
        &CabinetLight::gpioCallback // Verwende die statische Callback-Funktion
    );
    */
}

// Funktion zum Dimmen der LEDs
void CabinetLight::fadeLed(uint gpio, bool on) {

    // Überprüfe, ob der GPIO-Pin gültig ist
    if (gpio < 0 || gpio >= DEV_COUNT) {
        return; // Ungültiger GPIO-Pin, nichts tun
    }   

    // Konvertiere den GPIO-Pin in den entsprechenden PWM-Slice und Kanal
    uint slice = pwm_gpio_to_slice_num(gpio);

    // Konvertiere den GPIO-Pin in den entsprechenden PWM-Kanal
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

// Statischer GPIO-Interrupt-Handler (leitet an Instanz weiter)
void CabinetLight::gpioCallback(uint gpio, uint32_t events) {
    
    // Überprüfe, ob die Instanz existiert
    if (instance) {

        // Rufe die Instanzmethode auf, um die Logik zu verarbeiten
        instance->handleGpioCallback(gpio, events);
    }
}

// Instanz-Handler für Reedkontakte
void CabinetLight::handleGpioCallback(uint gpio, uint32_t events) {

    // Überprüfe, ob der GPIO-Pin gültig ist
    if (gpio < 0 || gpio >= DEV_COUNT) {
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
            else if (!door_open && ledState[i]) {
            
                fadeLed(LED_PINS[i], false);
                ledState[i] = false;
            }
        }
    }
}