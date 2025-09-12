/**
 * @file cabinetLight.h
 * @brief Zentrale Klasse zur Steuerung der Schrankbeleuchtung (Header).
 *
 * Diese Klasse kapselt die komplette Steuerung von bis zu vier LED-Gruppen
 * (z. B. für Schranktüren oder Fächer) über MOSFETs und Reedkontakte.
 * Sie übernimmt die Initialisierung der Hardware, PWM-Dimmung, Interrupt-Handling
 * und die Verarbeitung der Sensorereignisse. Die Klasse ist für den Einsatz auf
 * einem Raspberry Pi Pico (W) optimiert und bietet eine flexible API für die
 * Anpassung der Pinbelegung und Sensorlogik.
 *
 * Features:
 * - Bis zu 4 separat schaltbare LED-Gruppen
 * - Automatische Steuerung über Magnetsensoren oder Reed-Schalter
 * - PWM-Dimmung für sanftes Ein-/Ausschalten
 * - Geringe Standby-Leistung durch Low-RDS(on)-MOSFETs
 * - Flexible Pinbelegung und Sensor-Polarity
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-06-28
 * @copyright MIT
 */

// Erstelle eine Classe für die CabinetLight
// Diese Klasse soll die CabinetLight repräsentieren und deren Funktionen bereitstellen

#ifndef CABINET_LIGHT_H
#define CABINET_LIGHT_H

#include "pico/stdlib.h"    // Für GPIO und Standardfunktionen
#include "pico/time.h"      // Für Zeitfunktionen
#include "hardware/gpio.h"  // Für GPIO-Hardwarezugriff
#include "hardware/pwm.h"   // Für PWM-Hardwarezugriff
#include <array>            // Für std::array
#include <cstdint>          // Für uint8_t, uint16_t
#include <atomic>           // Für std::atomic

/**
 * @class CabinetLight
 * @brief Kapselt die Steuerung der Schrankbeleuchtung (bis zu 4 Kanäle).
 *
 * Diese Klasse übernimmt die Initialisierung, PWM-Dimmung, Sensorabfrage und das
 * Event-Handling für bis zu vier LED-Gruppen. Sie ist für den Einsatz auf dem
 * Raspberry Pi Pico (W) optimiert und unterstützt flexible Pinbelegung sowie
 * verschiedene Sensor-Polarity-Einstellungen.
 */
class CabinetLight {
public:

    /**
     * @brief Singleton-Instanz für statische Callback-Weiterleitung.
     * Wird intern für IRQ-Handler benötigt.
     */
    static std::atomic<CabinetLight*> instance;
    /**
     * @brief Gibt die Singleton-Instanz threadsicher zurück (z.B. für IRQ-Handler).
     */
    static CabinetLight* getInstance() {
        return instance.load(std::memory_order_acquire);
    }

    /**
     * @brief Anzahl der unterstützten LED-/Sensor-Kanäle.
     */
    static constexpr size_t DEV_COUNT = 4;

    /**
     * @brief PWM-Auflösung (TOP-Wert für PWM).
     * 12500 entspricht ca. 12 Bit bei 1 kHz.
     */
    static constexpr uint16_t PWM_WRAP = 12500;

    /**
     * @brief PWM-Frequenz in Hz.
     */
    static constexpr uint16_t PWM_FREQ_HZ = 1000;

    /**
     * @brief Entprellzeit für Sensoren in Millisekunden.
     */
    static constexpr uint16_t DEBOUNCE_MS = 100;

    /**
     * @brief Schrittweite für das Dimmen pro process()-Aufruf.
     */
    static constexpr uint16_t FADE_STEP = 1000;

    /**
     * @brief Default-GPIO-Pins für die LEDs (Definition in .cpp).
     */
    static const std::array<uint8_t, DEV_COUNT> DEFAULT_LED_PINS;

    /**
     * @brief Default-GPIO-Pins für die Sensoren (Definition in .cpp).
     */
    static const std::array<uint8_t, DEV_COUNT> DEFAULT_SENSOR_PINS;

    /**
     * @brief Aktuelle GPIO-Pins für LEDs (veränderbar zur Laufzeit).
     */
    std::array<uint8_t, DEV_COUNT> ledPins = DEFAULT_LED_PINS;

    /**
     * @brief Aktuelle GPIO-Pins für Sensoren (veränderbar zur Laufzeit).
     */
    std::array<uint8_t, DEV_COUNT> sensorPins = DEFAULT_SENSOR_PINS;

    /**
     * @brief Letzte Triggerzeit für jeden Sensor (für Entprellung).
     */
    std::array<absolute_time_t, DEV_COUNT> lastTriggerTime = {};

    /**
     * @brief Aktueller Zustand der LEDs (an/aus).
     */
    std::array<bool, DEV_COUNT> ledState = {};

    /**
     * @brief Aktuelle PWM-Level (0..PWM_WRAP) für jeden Kanal.
     */
    std::array<uint16_t, DEV_COUNT> currentLevel = {};

    /**
     * @brief Ziel-PWM-Level (0..PWM_WRAP) für jeden Kanal.
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
     */
    std::atomic<uint8_t> pendingMask {0};

    /**
     * @brief Sensor-Polarity: true = active-low (Standard: Pull-down).
     */
    std::array<bool, DEV_COUNT> sensorActiveLow{{true, true, true, true}};

    /**
     * @brief Konstruktor: Initialisiert GPIOs und PWM für alle Kanäle.
     */
    CabinetLight();
    
    /**
     * @brief Gibt den Initialisierungsstatus zurück.
     * true = erfolgreich, false = Fehler aufgetreten
     */
    bool isInitialized() const { return initialized; }

    /**
     * @brief Initialisiert die PWM für einen LED-Kanal. Prüft Pin-Gültigkeit.
     * @param gpio GPIO-Pin für die LED
     * @return true bei Erfolg, false bei Fehler
     */
    bool setupPwmLEDs(uint8_t gpio);

    /**
     * @brief Initialisiert die Sensor-GPIOs und IRQs für einen Kanal. Prüft Pin-Gültigkeit.
     * @param gpio GPIO-Pin für den Sensor
     * @return true bei Erfolg, false bei Fehler
     */
    bool setupSensors(uint8_t gpio);

    /**
     * @brief Statischer GPIO-Interrupt-Handler (leitet an Instanz weiter).
     *
     * @param gpio GPIO-Pin, der den Interrupt ausgelöst hat
     * @param events Ereignisse, die den Interrupt ausgelöst haben
     */
    static void gpioCallback(uint gpio, uint32_t events);

    /**
     * @brief Verarbeitet anstehende Events (z. B. Sensoränderungen, Fading).
     *
     * Diese Methode sollte regelmäßig in der Hauptschleife aufgerufen werden.
     */
    void process();

    /**
     * @brief Setzt die GPIO-Pins für die LED-Kanäle und reinitialisiert PWM.
     *
     * @param pins Neues Array mit DEV_COUNT GPIO-Pins für LEDs
     */
    /**
     * @brief Setzt die GPIO-Pins für die LED-Kanäle und reinitialisiert PWM. Prüft Pins.
     * @param pins Neues Array mit DEV_COUNT GPIO-Pins für LEDs
     * @return true bei Erfolg, false bei Fehler
     */
    bool setLedPins(const std::array<uint8_t, DEV_COUNT>& pins);

    /**
     * @brief Setzt die GPIO-Pins für die Sensoren und reinitialisiert IRQs.
     *
     * @param pins Neues Array mit DEV_COUNT GPIO-Pins für Sensoren
     */
    /**
     * @brief Setzt die GPIO-Pins für die Sensoren und reinitialisiert IRQs. Prüft Pins.
     * @param pins Neues Array mit DEV_COUNT GPIO-Pins für Sensoren
     * @return true bei Erfolg, false bei Fehler
     */
    bool setSensorPins(const std::array<uint8_t, DEV_COUNT>& pins);

    /**
     * @brief Setzt die Polarity (active low/high) für jeden Sensor.
     *
     * @param polarity Array mit true=active-low, false=active-high
     */
    void setSensorPolarity(const std::array<bool, DEV_COUNT>& polarity);

    /**
     * @brief Führt einen sichtbaren Startup-Test aus (alle LEDs blinken nacheinander).
     */
    void runStartupTest();

private:

    /**
     * @brief Interner Initialisierungsstatus
     */
    bool initialized = false;

    /**
     * @brief Dimmt die LED eines Kanals ein oder aus (Fading).
     *
     * @param gpio GPIO-Pin für die LED
     * @param on true = einblenden, false = ausblenden
     */
    void fadeLed(uint gpio, bool on);



    /**
     * @brief Verarbeitet den GPIO-Interrupt für einen Sensor.
     *
     * @param gpio GPIO-Pin, der den Interrupt ausgelöst hat
     */
    void onGpioIrq(uint gpio);
};

#endif // CABINET_LIGHT_H
