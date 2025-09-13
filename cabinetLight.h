/**
 * @file cabinetLight.h
 * @brief Zentrale Steuerklasse für die Schrankbeleuchtung (Header).
 *
 * Diese Klasse kapselt die komplette Steuerung von bis zu vier LED-Gruppen (z.B. für Schranktüren oder Fächer) über MOSFETs und Reedkontakte.
 * Sie übernimmt Initialisierung, PWM-Dimmung, Interrupt-Handling und Event-Verarbeitung. Die Klasse ist für den Raspberry Pi Pico (W) optimiert und bietet eine flexible API für Pinbelegung und Sensorlogik.
 *
 * \par Hauptfunktionen
 * - Bis zu 4 separat schaltbare LED-Gruppen (z.B. für Türen oder Fächer)
 * - Automatische Steuerung über Magnetsensoren (Reedkontakte, active-low)
 * - PWM-Dimmung für sanftes Ein-/Ausschalten (Fading)
 * - Flexible Pinbelegung und Sensor-Polarity (active-low/high)
 * - Umfangreiche Logging-API mit LogLevel
 * - Fehlerbehandlung mit LED-Signalisierung
 * - Heartbeat-LED als Lebenszeichen
 * - Startup-Test für alle LED-Kanäle
 *
 * \par Beispiel für die Nutzung
 * \code{.cpp}
 * #include "cabinetLight.h"
 * static CabinetLight light;
 * light.setSensorPolarity({true, true, true, true});
 * light.runStartupTest();
 * while (true) {
 *     light.process();
 * }
 * \endcode
 *
 * \author Knut Welzel <knut.welzel@gmail.com>
 * \date 2025-09-13
 * \copyright MIT
 */



#ifndef CABINET_LIGHT_H
#define CABINET_LIGHT_H

#include <cstdint>          // Für uint8_t, uint16_t
#include <array>            // Für std::array
#include "pico/stdlib.h"    // Für GPIO und Standardfunktionen
#include "pico/time.h"      // Für Zeitfunktionen
#include "hardware/gpio.h"  // Für GPIO-Hardwarezugriff
#include "hardware/pwm.h"   // Für PWM-Hardwarezugriff
#include <atomic>           // Für std::atomic

/**
 * @class CabinetLight
 * @brief Kapselt die Steuerung der Schrankbeleuchtung (bis zu 4 Kanäle).
 *
 * Diese Klasse übernimmt die Initialisierung, PWM-Dimmung, Sensorabfrage und das Event-Handling für bis zu vier LED-Gruppen.
 * Sie ist für den Einsatz auf dem Raspberry Pi Pico (W) optimiert und unterstützt flexible Pinbelegung sowie verschiedene Sensor-Polarity-Einstellungen.
 *
 * \note Thread-Sicherheit: Die Klasse ist grundsätzlich nicht für parallele Zugriffe aus mehreren Threads ausgelegt, mit Ausnahme der explizit als thread-safe dokumentierten statischen Methoden und Member (z.B. Singleton-Instanz, pendingMask). IRQ-Handler und Hauptschleife können sicher zusammenarbeiten, solange alle Zugriffe auf atomare Member (z.B. pendingMask) erfolgen. Methoden wie process(), setLedPins(), setSensorPins() etc. dürfen nicht gleichzeitig aus mehreren Threads aufgerufen werden.
 *
 * \note Die Klasse ist als Singleton ausgelegt, um IRQ-Handler und Event-Weiterleitung zu ermöglichen.
 */
class CabinetLight {


public:
    // === Singleton-Instanz für IRQ-Handler ===

    /**
     * @brief Singleton-Instanz für statische Callback-Weiterleitung (z.B. IRQ-Handler).
     *
     * @threadsafe
     * @details Zugriff auf diese Instanz ist threadsicher durch std::atomic.
     */
    static std::atomic<CabinetLight*> instance;

    /**
     * @brief Gibt die Singleton-Instanz threadsicher zurück (z.B. für IRQ-Handler).
     *
     * @threadsafe
     * @return Zeiger auf die aktuelle CabinetLight-Instanz
     */
    static CabinetLight* getInstance() {
        return instance.load(std::memory_order_acquire);
    }

    /**
     * @brief Lässt die Onboard-LED blinken (z.B. als Boot- oder Heartbeat-Anzeige).
     * @param times Anzahl der Blinkzyklen
     * @param on_ms Dauer LED an (ms)
     * @param off_ms Dauer LED aus (ms)
     */

    /**
     * @brief Lässt die Onboard-LED blinken (z.B. als Boot- oder Heartbeat-Anzeige).
     *
     * @param times  Anzahl der Blinkzyklen
     * @param on_ms  Dauer LED an (ms)
     * @param off_ms Dauer LED aus (ms)
     *
     * @details Diese Methode kann genutzt werden, um einen Boot- oder Fehlerstatus optisch anzuzeigen.
     */
    static void blinkOnboardLed(int times, int on_ms, int off_ms);

    /**
     * @brief Endlosschleife für Fehleranzeige (Onboard-LED schnelles Blinken).
     * @noreturn
     */

    /**
     * @brief Endlosschleife für Fehleranzeige (Onboard-LED schnelles Blinken).
     *
     * @details Wird bei fatalen Fehlern aufgerufen und signalisiert dauerhaft einen Fehlerzustand.
     * @noreturn
     */
    [[noreturn]] static void fatalErrorBlink();

    /**
     * @brief Anzahl der unterstützten LED-/Sensor-Kanäle.
     */

    /**
     * @brief Anzahl der unterstützten LED-/Sensor-Kanäle (maximal 4).
     *
     * @details Die Klasse unterstützt bis zu vier unabhängige Kanäle für LEDs und Sensoren.
     */
    static constexpr size_t DEV_COUNT = 4;

    /**
     * @brief PWM-Auflösung (TOP-Wert für PWM).
     * 12500 entspricht ca. 12 Bit bei 1 kHz.
     */

    /**
     * @brief PWM-Auflösung (TOP-Wert für PWM).
     *
     * @details 12500 entspricht ca. 12 Bit bei 1 kHz PWM-Frequenz.
     */
    static constexpr uint16_t PWM_WRAP = 12500;

    /**
     * @brief PWM-Frequenz in Hz.
     */

    /**
     * @brief PWM-Frequenz in Hz (Standard: 1000 Hz).
     */
    static constexpr uint16_t PWM_FREQ_HZ = 1000;

    /**
     * @brief Entprellzeit für Sensoren in Millisekunden.
     */

    /**
     * @brief Entprellzeit für Sensoren in Millisekunden (Standard: 100 ms).
     *
     * @details Verhindert Mehrfachauslösung durch Prellen der Reedkontakte.
     */
    static constexpr uint16_t DEBOUNCE_MS = 100;

    /**
     * @brief Schrittweite für das Dimmen pro process()-Aufruf.
     */

    /**
     * @brief Schrittweite für das Dimmen pro process()-Aufruf (PWM-Level pro Schritt).
     */
    static constexpr uint16_t FADE_STEP = 1000;

        /**
     * @brief Standard-Intervall für das Heartbeat-Blinken (Millisekunden)
     */

    /**
     * @brief Standard-Intervall für das Heartbeat-Blinken (Millisekunden).
     *
     * @details Gibt an, wie oft die Heartbeat-LED im Normalbetrieb blinkt.
     */
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 1000;

    /**
     * @brief Standard-Intervall für Fading-Schritte (Millisekunden)
     */

    /**
     * @brief Standard-Intervall für Fading-Schritte (Millisekunden).
     */
    static constexpr uint32_t FADING_STEP_MS = 50;

    /**
     * @brief Verzögerung für LED-Test beim Start (Millisekunden)
     */

    /**
     * @brief Verzögerung für LED-Test beim Start (Millisekunden).
     *
     * @details Gibt die An- und Aus-Dauer für den Startup-Test der LEDs an.
     */
    static constexpr uint32_t STARTUP_LED_ON_MS = 300;
    static constexpr uint32_t STARTUP_LED_OFF_MS = 50;

    /**
     * @brief PWM-Test-Delay (Millisekunden)
     */

    /**
     * @brief PWM-Test-Delay (Millisekunden).
     */
    static constexpr uint32_t PWM_TEST_DELAY_MS = 100;

    /**
     * @brief Default-GPIO-Pins für die LEDs (Definition in .cpp).
     */

    /**
     * @brief Default-GPIO-Pins für die LEDs (Definition in .cpp).
     *
     * @details Diese Pins werden verwendet, wenn keine eigenen Pins gesetzt werden.
     */
    static const std::array<uint8_t, DEV_COUNT> DEFAULT_LED_PINS;

    /**
     * @brief Default-GPIO-Pins für die Sensoren (Definition in .cpp).
     */
    static const std::array<uint8_t, DEV_COUNT> DEFAULT_SENSOR_PINS;

    /**
     * @brief Aktuelle GPIO-Pins für LEDs (veränderbar zur Laufzeit).
     *
     * @details Kann über setLedPins() geändert werden.
     */
    std::array<uint8_t, DEV_COUNT> ledPins = DEFAULT_LED_PINS;


    /**
     * @brief Aktuelle GPIO-Pins für Sensoren (veränderbar zur Laufzeit).
     *
     * @details Kann über setSensorPins() geändert werden.
     */
    std::array<uint8_t, DEV_COUNT> sensorPins = DEFAULT_SENSOR_PINS;


    /**
     * @brief Letzte Triggerzeit für jeden Sensor (für Entprellung).
     *
     * @details Wird intern zur Entprellung der Reedkontakte verwendet.
     */
    std::array<absolute_time_t, DEV_COUNT> lastTriggerTime = {};


    /**
     * @brief Aktueller Zustand der LEDs (an/aus).
     *
     * @details true = LED an, false = LED aus
     */
    std::array<bool, DEV_COUNT> ledState = {};

    /**
     * @brief Aktuelle PWM-Level (0..PWM_WRAP) für jeden Kanal.
     */
    std::array<uint16_t, DEV_COUNT> currentLevel = {};

    /**
     * @brief Ziel-PWM-Level (0..PWM_WRAP) für jeden Kanal.
     *
     * @details Wird für sanftes Fading verwendet.
     */
    std::array<uint16_t, DEV_COUNT> targetLevel = {};

    /**
     * @brief Gibt an, ob ein Kanal gerade fadet (Dimmen aktiv).
     */
    std::array<bool, DEV_COUNT> fading = {};

    /**
     * @brief Letzter gelesener GPIO-Zustand (für Polling-Fallback).
     */
    std::array<bool, DEV_COUNT> lastRawState = {};
    
    /**
     * @brief Bitmaske für anstehende Sensorereignisse (IRQ-sicher, atomar).
     *
     * @threadsafe
     * @details Zugriff auf dieses Feld ist threadsicher (std::atomic). Wird für die Kommunikation zwischen IRQ und Hauptschleife verwendet.
     */
    std::atomic<uint8_t> pendingMask {0};

    /**
     * @brief Sensor-Polarity: true = active-low (Standard: Pull-down).
     *
     * @details Kann über setSensorPolarity() angepasst werden.
     */
    std::array<bool, DEV_COUNT> sensorActiveLow{{true, true, true, true}};

    /**
     * @brief LogLevel für die Logging-API.
     *
     * ERROR: Nur Fehler
     * WARN:  Fehler und Warnungen
     * INFO:  Fehler, Warnungen, Info
     * DEBUG: Alle Meldungen
     */
    enum class LogLevel : uint8_t { 
        ERROR = 0, 
        WARN = 1, 
        INFO = 2, 
        DEBUG = 3 
    };

    /**
     * @brief Konstruktor: Initialisiert GPIOs und PWM für alle Kanäle.
     *
     * @details Führt die Initialisierung der Hardware durch. Nach dem Konstruktor sollte isInitialized() geprüft werden.
     */
    CabinetLight();

    /**
     * @brief Gibt den Initialisierungsstatus zurück.
     *
     * @return true = erfolgreich, false = Fehler aufgetreten
     *
     * @details Sollte nach dem Konstruktor geprüft werden, um Hardwarefehler zu erkennen.
     */
    bool isInitialized() const { return initialized; }

    /**
     * @brief Initialisiert die PWM für einen LED-Kanal. Prüft Pin-Gültigkeit.
     *
     * @param gpio GPIO-Pin für die LED
     * @return true bei Erfolg, false bei Fehler
     *
     * @details Diese Methode wird intern beim Setzen der Pins verwendet.
     */
    bool setupPwmLEDs(uint8_t gpio);

    /**
     * @brief Initialisiert die Sensor-GPIOs und IRQs für einen Kanal. Prüft Pin-Gültigkeit.
     *
     * @param gpio GPIO-Pin für den Sensor
     * @return true bei Erfolg, false bei Fehler
     *
     * @details Diese Methode wird intern beim Setzen der Pins verwendet.
     */
    bool setupSensors(uint8_t gpio);

    /**
     * @brief Statischer GPIO-Interrupt-Handler (leitet an Instanz weiter).
     *
     * @param gpio   GPIO-Pin, der den Interrupt ausgelöst hat
     * @param events Ereignisse, die den Interrupt ausgelöst haben
     *
     * @details Wird von der Pico-IRQ-API aufgerufen und leitet an die Instanz weiter.
     */
    static void gpioCallback(uint gpio, uint32_t events);

    /**
     * @brief Verarbeitet anstehende Events (z. B. Sensoränderungen, Fading).
     *
     * @warning Nicht thread-safe! Darf nur aus einem Thread (z.B. der Mainloop) aufgerufen werden.
     * @details Diese Methode sollte regelmäßig in der Hauptschleife aufgerufen werden, um Sensor- und LED-Events zu verarbeiten.
     */
    void process();
    
    /**
     * @brief Setzt die GPIO-Pins für die LED-Kanäle und reinitialisiert PWM. Prüft Pins.
     *
     * @warning Nicht thread-safe! Darf nicht parallel zu anderen Methoden aufgerufen werden.
     * @param pins Neues Array mit DEV_COUNT GPIO-Pins für LEDs
     * @return true bei Erfolg, false bei Fehler
     *
     * @details Kann zur Laufzeit aufgerufen werden, um die Pinbelegung zu ändern.
     */
    bool setLedPins(const std::array<uint8_t, DEV_COUNT>& pins);

    /**
     * @brief Setzt die GPIO-Pins für die Sensoren und reinitialisiert IRQs. Prüft Pins.
     *
     * @warning Nicht thread-safe! Darf nicht parallel zu anderen Methoden aufgerufen werden.
     * @param pins Neues Array mit DEV_COUNT GPIO-Pins für Sensoren
     * @return true bei Erfolg, false bei Fehler
     *
     * @details Kann zur Laufzeit aufgerufen werden, um die Pinbelegung zu ändern.
     */
    bool setSensorPins(const std::array<uint8_t, DEV_COUNT>& pins);

    /**
     * @brief Setzt die Polarity (active low/high) für jeden Sensor.
     *
     * @warning Nicht thread-safe! Darf nicht parallel zu anderen Methoden aufgerufen werden.
     * @param polarity Array mit true=active-low, false=active-high
     *
     * @details Ermöglicht die Anpassung an verschiedene Sensorlogiken.
     */
    void setSensorPolarity(const std::array<bool, DEV_COUNT>& polarity);

    /**
     * @brief Führt einen sichtbaren Startup-Test aus (alle LEDs blinken nacheinander).
     *
     * @warning Nicht thread-safe! Darf nicht parallel zu anderen Methoden aufgerufen werden.
     * @details Kann zur Funktionsprüfung beim Start verwendet werden.
     */
    void runStartupTest();

    // === Logging ===

    /**
     * @brief Setzt das globale LogLevel für die Logging-API.
     * @param level Neues LogLevel
     */
    static void setLogLevel(LogLevel level);

    /**
     * @brief Gibt das aktuelle LogLevel zurück.
     * @return Aktuelles LogLevel
     */
    static LogLevel getLogLevel();

    /**
     * @brief Gibt eine Fehlermeldung aus (LogLevel ERROR).
     * @param fmt Formatstring (wie printf)
     * @param ... Argumente
     */
    static void logError(const char* fmt, ...);

    /**
     * @brief Gibt eine Warnung aus (LogLevel WARN).
     * @param fmt Formatstring (wie printf)
     * @param ... Argumente
     */
    static void logWarn(const char* fmt, ...);

    /**
     * @brief Gibt eine Info-Meldung aus (LogLevel INFO).
     * @param fmt Formatstring (wie printf)
     * @param ... Argumente
     */
    static void logInfo(const char* fmt, ...);

    /**
     * @brief Gibt eine Debug-Meldung aus (LogLevel DEBUG).
     * @param fmt Formatstring (wie printf)
     * @param ... Argumente
     */
    static void logDebug(const char* fmt, ...);

    /**
     * @brief Aktiviert/deaktiviert das Polling-Fallback für Sensoren.
     *
     * @param enable true = Polling aktivieren, false = nur IRQ
     *
     * @details Standard ist false (nur IRQ). Polling kann als Fallback genutzt werden, falls IRQ nicht zuverlässig funktioniert.
     */
    void setPollingFallback(bool enable);

    /**
     * @brief Gibt zurück, ob das Polling-Fallback aktiv ist.
     * @return true = Polling aktiv, false = nur IRQ
     */
    bool getPollingFallback() const;

private:


    /**
     * @brief Gibt an, ob das Polling-Fallback für Sensoren aktiv ist.
     */
    bool pollingFallback = false;


    /**
     * @brief Globales LogLevel für die Logging-API.
     */
    static LogLevel logLevel;

    /**
     * @brief Interner Initialisierungsstatus (true = OK, false = Fehler).
     */
    bool initialized = false;

    /**
     * @brief Dimmt die LED eines Kanals ein oder aus (Fading).
     *
     * @param gpio GPIO-Pin für die LED
     * @param on true = einblenden, false = ausblenden
     *
     * @details Wird intern für sanftes Ein-/Ausblenden verwendet.
     */
    void fadeLed(uint gpio, bool on);

    /**
     * @brief Verarbeitet den GPIO-Interrupt für einen Sensor.
     *
     * @param gpio GPIO-Pin, der den Interrupt ausgelöst hat
     *
     * @details Wird intern vom statischen IRQ-Handler aufgerufen.
     */
    void onGpioIrq(uint gpio);
};

#endif // CABINET_LIGHT_H
