/**
 * @file cabinetLight.h
 * @brief Zentrale Klasse zur Steuerung der Schrankbeleuchtung.
 *
 * Kapselt die Ansteuerung von bis zu vier LED-Gruppen über MOSFETs und
 * die Auswertung von Reedkontakten zur automatischen Schrankbeleuchtung.
 * Die Klasse übernimmt Initialisierung, PWM-Dimmung und Interrupt-Handling.
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

// Erstelle eine Classe für die CabinetLight
// Diese Klasse soll die CabinetLight repräsentieren und deren Funktionen bereitstellen

#ifndef CABINET_LIGHT_H
#define CABINET_LIGHT_H

#include "pico/stdlib.h"      // Für GPIO und Standardfunktionen
#include "pico/time.h"        // Für Zeitfunktionen
#include "hardware/gpio.h"    // Für GPIO-Hardwarezugriff
#include "hardware/pwm.h"     // Für PWM-Hardwarezugriff
#include <vector>

/**
 * @brief Klasse für die CabinetLight
 */
class CabinetLight {
public:

    static CabinetLight* instance; //!< Singleton-Instanz für Callback


    const uint8_t DEV_COUNT = 4; //!< Anzahl der LEDs
    const std::vector<uint8_t> LED_PINS = {2, 3, 4, 5};             //!< GPIO-Pins für die LEDs
    const std::vector<uint8_t> SENSOR_PINS = {6, 7, 8, 9};          //!< GPIO-Pins für die Sensoren
    const uint16_t PWM_WRAP = 65535;                                //!< 16-bit PWM
    const uint16_t DEBOUNCE_MS = 100;                               //!< Sperrzeit nach Interrupt

    std::vector<absolute_time_t> lastTriggerTime = {0, 0, 0, 0};    //!< Letzte Triggerzeit für jeden Sensor
    std::vector<bool> ledState = {false, false, false, false};      //!< Aktueller Zustand der LEDs

    /**
     * @brief Konstruktor der CabinetLight-Klasse
     * Initialisiert die GPIO-Pins und PWM-Ausgänge für die LEDs
     */
    CabinetLight();

    /**
     * @brief Statischer GPIO-Interrupt-Handler (leitet an Instanz weiter)
     * @param gpio GPIO-Pin, der den Interrupt ausgelöst hat
     * @param events Ereignisse, die den Interrupt ausgelöst haben
     */
    static void gpioCallback(uint gpio, uint32_t events);

private:

    /**
     * @brief Initialisiert die GPIO-Pins für die Sensoren
     * @param gpio GPIO-Pin für den Sensor
     * 
     */
    void setupPwmLEDs(uint8_t gpio);

    /**
     * @brief Initialisiert die GPIO-Pins für die Sensoren
     * @param gpio GPIO-Pin für den Sensor
     */ 
    void setupSensors(uint8_t gpio);

    /**
     * @brief Dimmt die LED ein oder aus
     * @param gpio GPIO-Pin für die LED
     * @param on true, um die LED einzuschalten, false, um sie auszuschalten
     */
    void fadeLed(uint gpio, bool on);

    /**
     * @brief Initialisiert die GPIO-Pins für die Sensoren
     * @param gpio GPIO-Pin für den Sensor
     */
    void handleGpioCallback(uint gpio, uint32_t events);
};

#endif // CABINET_LIGHT_H
