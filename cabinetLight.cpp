/**
 * @file cabinetLight.cpp
 * @brief Implementierung der Steuerlogik für die Schrankbeleuchtung (Raspberry Pi Pico).
 *
 * Diese Datei enthält die vollständige Implementierung der Klasse CabinetLight zur
 * automatischen Steuerung von bis zu vier LED-Gruppen in Schränken oder Möbeln.
 * Die Steuerung erfolgt über Reedkontakte (Magnetsensoren) und MOSFETs. Die LEDs
 * werden per PWM sanft ein- und ausgeblendet. Die Klasse übernimmt die Initialisierung
 * der Hardware, das Setup der PWM-Kanäle, die Konfiguration der Sensor-GPIOs sowie das
 * Interrupt- und Event-Handling. Besonderheiten sind die flexible Pinbelegung, die
 * Entprellung der Sensoren, ein Fading-Mechanismus und die Unterstützung von
 * active-low/active-high-Sensorlogik.
 *
 * Architektur-Highlights:
 * - Singleton-Pattern für IRQ-Callback-Weiterleitung
 * - Atomare Bitmasken für IRQ-sicheres Event-Handling
 * - Flexible API für Pin- und Polarity-Konfiguration
 * - Effiziente PWM-Dimmung mit konfigurierbarer Schrittweite und Frequenz
 *
 * Features:
 * - Bis zu 4 separat schaltbare und dimmbare LED-Gruppen
 * - Automatische Steuerung über Reedkontakte (Magnetsensoren)
 * - PWM-Dimmung für sanftes Licht (Fading)
 * - Geringe Standby-Leistung durch Low-RDS(on)-MOSFETs
 * - Entprellung und IRQ-Handling für zuverlässige Sensorerkennung
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-06-28
 * @copyright MIT
 */

#include "cabinetLight.h"

// Prototyp für die freie Callback-Funktion (IRQ-Handler für Sensoren)
void cabinet_gpio_callback(uint gpio, uint32_t events);

#include <algorithm>
#include <cstdio>
// for clock_get_hz()
#include "hardware/clocks.h"

// Definition der statischen Instanz für Singleton-Pattern (IRQ-Weiterleitung)
std::atomic<CabinetLight*> CabinetLight::instance = nullptr;

// Standard-Pinbelegung für LEDs und Sensoren (kann zur Laufzeit geändert werden)
const std::array<uint8_t, CabinetLight::DEV_COUNT> CabinetLight::DEFAULT_LED_PINS = {2, 3, 4, 5};
const std::array<uint8_t, CabinetLight::DEV_COUNT> CabinetLight::DEFAULT_SENSOR_PINS = {6, 7, 8, 9};

// Konstruktor: Initialisiert alle Kanäle, Pins und Statusarrays
CabinetLight::CabinetLight() {
    printf("[DEBUG] CabinetLight Konstruktor aufgerufen.\n");
    instance.store(this, std::memory_order_release);
    ledPins = DEFAULT_LED_PINS;
    sensorPins = DEFAULT_SENSOR_PINS;
    initialized = true;
    // PWM für alle LED-Pins initialisieren
    for (uint8_t gpio : ledPins) {
        if (!setupPwmLEDs(gpio)) {
            printf("[ERROR] PWM-Init fehlgeschlagen für GPIO %d\n", gpio);
            initialized = false;
        }
    }
    // IRQ-Callback für den ersten Sensor-Pin global registrieren (SDK-Anforderung)
    gpio_set_irq_enabled_with_callback(sensorPins[0], GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, cabinet_gpio_callback);
    // Sensor-GPIOs initialisieren (inkl. Pull-Down und IRQ)
    for (uint8_t gpio : sensorPins) {
        if (!setupSensors(gpio)) {
            printf("[ERROR] Sensor-Init fehlgeschlagen für GPIO %d\n", gpio);
            initialized = false;
        }
    }
    currentLevel.fill(0);
    targetLevel.fill(0);
    fading.fill(false);
    ledState.fill(false);
    lastTriggerTime.fill({});
    if (initialized) {
        printf("[DEBUG] CabinetLight Konstruktor abgeschlossen.\n");
    } else {
        printf("[ERROR] CabinetLight Initialisierung unvollständig!\n");
    }
}

// Initialisiert einen LED-Pin für PWM-Betrieb
bool CabinetLight::setupPwmLEDs(uint8_t gpio) {
    if (gpio > 29) {
        printf("[ERROR] Ungültiger LED-GPIO: %d\n", gpio);
        return false;
    }
    printf("[DEBUG] setupPwmLEDs: Konfiguriere PWM für GPIO %d\n", gpio);
    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    gpio_set_pulls(gpio, false, false);
    uint slice = pwm_gpio_to_slice_num(gpio);
    pwm_config config = pwm_get_default_config();
    float clk_hz = (float)clock_get_hz(clk_sys);
    float clkdiv = clk_hz / ((float)PWM_FREQ_HZ * ((float)PWM_WRAP + 1.0f));
    if (clkdiv < 1.0f) clkdiv = 1.0f;
    pwm_config_set_clkdiv(&config, clkdiv);
    pwm_config_set_wrap(&config, PWM_WRAP);
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(gpio, 0);
    pwm_set_enabled(slice, true);
    auto it = std::find(ledPins.begin(), ledPins.end(), gpio);
    if (it != ledPins.end()) {
        size_t idx = std::distance(ledPins.begin(), it);
        currentLevel[idx] = 0;
        targetLevel[idx] = 0;
        fading[idx] = false;
    }
    pwm_set_gpio_level(gpio, PWM_WRAP);
    sleep_ms(100);
    pwm_set_gpio_level(gpio, 0);
    return true;
}

// Initialisiert einen Sensor-Pin als Eingang mit Pull-Down und IRQ
bool CabinetLight::setupSensors(uint8_t gpio) {
    if (gpio > 29) {
        printf("[ERROR] Ungültiger Sensor-GPIO: %d\n", gpio);
        return false;
    }
    printf("[DEBUG] setupSensors: Konfiguriere Sensor GPIO %d\n", gpio);
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_down(gpio);
    auto it = std::find(sensorPins.begin(), sensorPins.end(), gpio);
    if (it != sensorPins.end()) {
        int index = std::distance(sensorPins.begin(), it);
        lastTriggerTime[index] = get_absolute_time();
    }
    gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, cabinet_gpio_callback);
    return true;
}


// Freie Callback-Funktion für GPIO-Interrupts (leitet an Instanz weiter)
void cabinet_gpio_callback(uint gpio, uint32_t events) {
    CabinetLight* inst = CabinetLight::getInstance();
    if (inst) {
        inst->gpioCallback(gpio, events);
    }
}

// Setzt das Ziellevel für eine LED (Fading wird aktiviert)
void CabinetLight::fadeLed(uint gpio, bool on) {
    auto it = std::find(ledPins.begin(), ledPins.end(), static_cast<uint8_t>(gpio));
    if (it == ledPins.end()) return;
    size_t idx = std::distance(ledPins.begin(), it);
    uint16_t newTarget = on ? PWM_WRAP : 0;
    if (targetLevel[idx] != newTarget) {
        targetLevel[idx] = newTarget;
        fading[idx] = true; // Fading nur aktivieren, wenn sich das Ziellevel ändert
    }
}

// Statischer IRQ-Handler: leitet an Instanz weiter
void CabinetLight::gpioCallback(uint gpio, uint32_t events) {
    printf("[DEBUG] gpioCallback: GPIO %d, events=0x%08x\n", gpio, events);
    CabinetLight* inst = getInstance();
    if (!inst) return;
    inst->onGpioIrq(gpio);
}

// IRQ-Event: Setzt das Pending-Bit für den betroffenen Sensor
void CabinetLight::onGpioIrq(uint gpio) {
    for (int i = 0; i < static_cast<int>(DEV_COUNT); ++i) {
        if (sensorPins[i] == gpio) {
            printf("[DEBUG] onGpioIrq: matched sensor index %d (gpio %d)\n", i, gpio);
            pendingMask.fetch_or(static_cast<uint8_t>(1u << i));
            break;
        }
    }
}

// Hauptverarbeitung: prüft Sensorereignisse, Polling und steuert das Fading
void CabinetLight::process() {
    // 1. IRQ-Events abarbeiten (pendingMask wird atomar zurückgesetzt)
    uint8_t pending = pendingMask.exchange(0);
    if (pending) {
        for (int i = 0; i < static_cast<int>(DEV_COUNT); ++i) {
            if (!(pending & (1u << i))) continue;
            absolute_time_t now = get_absolute_time();
            // Entprellung: nur behandeln, wenn genug Zeit vergangen ist
            if (absolute_time_diff_us(lastTriggerTime[i], now) < DEBOUNCE_MS * 1000) continue;
            lastTriggerTime[i] = now;
            bool gpio_state = gpio_get(sensorPins[i]);
            // Sensorlogik: active-low oder active-high
            bool door_open = sensorActiveLow[i] ? (gpio_state == 0) : (gpio_state != 0);
            printf("[DEBUG] process: sensor %d gpio=%d state=%d door_open=%d ledState=%d\n", i, sensorPins[i], gpio_state, door_open, ledState[i]);
            if (door_open && !ledState[i]) {
                // Tür wurde geöffnet, LED einschalten (faden)
                printf("[DEBUG] process: opening detected on sensor %d -> fade on\n", i);
                fadeLed(ledPins[i], true);
                ledState[i] = true;
            } else if (!door_open && ledState[i]) {
                // Tür wurde geschlossen, LED ausschalten (faden)
                printf("[DEBUG] process: closing detected on sensor %d -> fade off\n", i);
                fadeLed(ledPins[i], false);
                ledState[i] = false;
            }
        }
    }
    // 2. Polling-Fallback: prüft regelmäßig die Sensor-GPIOs (falls IRQs verloren gehen)
    for (int i = 0; i < static_cast<int>(DEV_COUNT); ++i) {
        bool raw = gpio_get(sensorPins[i]) != 0;
        if (raw != lastRawState[i]) {
            absolute_time_t now = get_absolute_time();
            if (absolute_time_diff_us(lastTriggerTime[i], now) >= DEBOUNCE_MS * 1000) {
                lastTriggerTime[i] = now;
                printf("[POLL] sensor %d raw=%d (changed)\n", i, raw);
                bool door_open = sensorActiveLow[i] ? (raw == 0) : (raw != 0);
                printf("[POLL] sensor %d door_open=%d\n", i, door_open);
                if (door_open && !ledState[i]) {
                    fadeLed(ledPins[i], true);
                    ledState[i] = true;
                } else if (!door_open && ledState[i]) {
                    fadeLed(ledPins[i], false);
                    ledState[i] = false;
                }
            }
        }
        lastRawState[i] = raw;
    }
    // 3. Fading-Logik: aktuelles PWM-Level schrittweise ans Ziellevel anpassen
    for (size_t i = 0; i < DEV_COUNT; ++i) {
        if (!fading[i]) continue;
        uint32_t cur = currentLevel[i];
        uint32_t tgt = targetLevel[i];
        if (cur == tgt) { fading[i] = false; continue; }
        if (cur < tgt) {
            uint32_t next = cur + FADE_STEP;
            if (next > tgt) next = tgt;
            currentLevel[i] = static_cast<uint16_t>(next);
        } else {
            uint32_t next = cur > FADE_STEP ? cur - FADE_STEP : 0;
            currentLevel[i] = static_cast<uint16_t>(next);
        }
        pwm_set_gpio_level(ledPins[i], currentLevel[i]);
        if (currentLevel[i] == targetLevel[i]) fading[i] = false;
    }
}

// Setzt neue LED-Pins und initialisiert PWM für diese
bool CabinetLight::setLedPins(const std::array<uint8_t, DEV_COUNT>& pins) {
    bool ok = true;
    for (uint8_t g : pins) {
        if (g > 29) {
            printf("[ERROR] Ungültiger LED-Pin: %d\n", g);
            ok = false;
        }
    }
    if (!ok) return false;
    for (uint8_t g : ledPins) {
        uint slice = pwm_gpio_to_slice_num(g);
        pwm_set_enabled(slice, false);
    }
    ledPins = pins;
    for (uint8_t g : ledPins) {
        if (!setupPwmLEDs(g)) ok = false;
    }
    return ok;
}

// Setzt neue Sensor-Pins und initialisiert IRQs für diese
bool CabinetLight::setSensorPins(const std::array<uint8_t, DEV_COUNT>& pins) {
    bool ok = true;
    for (uint8_t g : pins) {
        if (g > 29) {
            printf("[ERROR] Ungültiger Sensor-Pin: %d\n", g);
            ok = false;
        }
    }
    if (!ok) return false;
    for (uint8_t g : sensorPins) {
        gpio_set_irq_enabled(g, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    }
    sensorPins = pins;
    for (uint8_t g : sensorPins) {
        if (!setupSensors(g)) ok = false;
    }
    return ok;
}

// Setzt die Sensor-Polarity (active-low/active-high) für alle Kanäle
void CabinetLight::setSensorPolarity(const std::array<bool, DEV_COUNT>& polarity) {
    sensorActiveLow = polarity;
}

// Lässt alle LEDs nacheinander kurz aufleuchten (Test beim Start)
void CabinetLight::runStartupTest() {
    printf("[TEST] Running startup LED test...\n");
    for (size_t i = 0; i < DEV_COUNT; ++i) {
        uint8_t g = ledPins[i];
        printf("[TEST] Blink LED on GPIO %d\n", g);
        pwm_set_gpio_level(g, PWM_WRAP); // LED an
        sleep_ms(300);
        pwm_set_gpio_level(g, 0);        // LED aus
        sleep_ms(50);
    }
    printf("[TEST] Startup LED test completed.\n");
}