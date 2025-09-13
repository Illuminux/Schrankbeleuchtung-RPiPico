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
 * Thread-Sicherheit: IRQ-Handler und Hauptschleife sind über atomare Bitmasken synchronisiert. Die Klasse ist ansonsten nicht für parallele Zugriffe ausgelegt.
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-09-13
 * @copyright MIT
 */

#include "cabinetLight.h"


// Prototyp für die freie Callback-Funktion (IRQ-Handler für Sensoren)
// Wird vom Pico-SDK benötigt, um IRQs an die Instanz weiterzuleiten
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
    logDebug("CabinetLight Konstruktor aufgerufen.\n");
    instance.store(this, std::memory_order_release); // Singleton-Instanz setzen
    ledPins = DEFAULT_LED_PINS;         // Standard-LED-Pins setzen
    sensorPins = DEFAULT_SENSOR_PINS;   // Standard-Sensor-Pins setzen
    initialized = true;                 // Initialisierungsstatus setzen

    // Initialisiere PWM für alle LED-Pins
    for (uint8_t gpio : ledPins) {

        // LED-Pins initialisieren (inkl. PWM-Setup)
        if (!setupPwmLEDs(gpio)) {
            logError("PWM-Init fehlgeschlagen für GPIO %d\n", gpio);
            initialized = false;    // Initialisierung fehlgeschlagen
        }
    }

    // IRQ-Callback für den ersten Sensor-Pin global registrieren (SDK-Anforderung)
    gpio_set_irq_enabled_with_callback(sensorPins[0], GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, cabinet_gpio_callback);

    // Sensor-GPIOs initialisieren (inkl. Pull-Down und IRQ)
    for (uint8_t gpio : sensorPins) {

        // Sensor-Pins initialisieren (inkl. Pull-Down und IRQ)
        if (!setupSensors(gpio)) {
            logError("Sensor-Init fehlgeschlagen für GPIO %d\n", gpio);
            initialized = false;    // Initialisierung fehlgeschlagen
        }
    }

    // Initialwerte für Statusarrays setzen
    currentLevel.fill(0);       // Alle LEDs aus
    targetLevel.fill(0);        // Alle Zielwerte auf 0 setzen
    fading.fill(false);         // Kein Fading aktiv
    ledState.fill(false);       // Alle LEDs aus
    lastTriggerTime.fill({});   // Letzte Triggerzeiten zurücksetzen

    // Debug-Ausgabe des Initialisierungsstatus
    if (initialized) {
        logDebug("CabinetLight Konstruktor abgeschlossen.\n");
    } else {
        logError("CabinetLight Initialisierung unvollständig!\n");
    }
}

// Initialisiert einen LED-Pin für PWM-Betrieb
bool CabinetLight::setupPwmLEDs(uint8_t gpio) {

    // Gültigkeit des Pins prüfen (nur GPIO 0-29 erlaubt)
    if (gpio > 29) {
        logError("Ungültiger LED-GPIO: %d\n", gpio);
        return false;
    }
    
    logDebug("setupPwmLEDs: Konfiguriere PWM für GPIO %d\n", gpio);
    
    gpio_init(gpio);                        // GPIO initialisieren
    gpio_set_function(gpio, GPIO_FUNC_PWM); // Kein Pull-Up/Down (MOSFET-Gate wird durch PWM gesteuert)
    gpio_set_pulls(gpio, false, false);     // Pull-Down aktivieren

    // PWM-Konfiguration für den Pin
    uint slice = pwm_gpio_to_slice_num(gpio);       // Slice-Nummer ermitteln   
    pwm_config config = pwm_get_default_config();   // Standard-Konfiguration laden
    float clk_hz = (float)clock_get_hz(clk_sys);    // Systemtaktfrequenz
    // Berechne Clock-Divider für gewünschte PWM-Frequenz
    float clkdiv = clk_hz / ((float)PWM_FREQ_HZ * ((float)PWM_WRAP + 1.0f));
    if (clkdiv < 1.0f) clkdiv = 1.0f;           // Minimum 1.0
    pwm_config_set_clkdiv(&config, clkdiv);     // Clock-Divider setzen
    pwm_config_set_wrap(&config, PWM_WRAP);     // TOP-Wert setzen 
    pwm_init(slice, &config, true);             // PWM mit neuer Konfiguration starten
    pwm_set_gpio_level(gpio, 0);                // LED aus
    pwm_set_enabled(slice, true);               // PWM-Ausgang aktivieren

    // Statusarrays für diesen Kanal zurücksetzen
    auto it = std::find(ledPins.begin(), ledPins.end(), gpio);

    // Wenn der Pin in der Liste ist, Index ermitteln und Status zurücksetzen
    if (it != ledPins.end()) {
        // Index ermitteln
        size_t idx = std::distance(ledPins.begin(), it);
        currentLevel[idx] = 0;  // LED aus
        targetLevel[idx] = 0;   // Ziellevel auf 0
        fading[idx] = false;    // Kein Fading aktiv
    }

    // Kurzer Test: LED einmal an/aus
    pwm_set_gpio_level(gpio, PWM_WRAP); // LED an
    sleep_ms(PWM_TEST_DELAY_MS);        // kurze Pause
    pwm_set_gpio_level(gpio, 0);        // LED aus
    return true;
}

// Initialisiert einen Sensor-Pin als Eingang mit Pull-Down und IRQ
bool CabinetLight::setupSensors(uint8_t gpio) {

    // Gültigkeit des Pins prüfen (nur GPIO 0-29 erlaubt)
    if (gpio > 29) {
        logError("Ungültiger Sensor-GPIO: %d\n", gpio);
        return false;
    }
    
    logDebug("setupSensors: Konfiguriere Sensor GPIO %d\n", gpio);
    
    gpio_init(gpio);                // GPIO initialisieren 
    gpio_set_dir(gpio, GPIO_IN);    // Als Eingang  
    gpio_pull_down(gpio);           // Interne Pull-Down aktivieren (Standard: active-low Sensor)

    // Entprellzeit initialisieren
    auto it = std::find(sensorPins.begin(), sensorPins.end(), gpio);
    // Wenn der Pin in der Liste ist, Index ermitteln und Zeit zurücksetzen
    if (it != sensorPins.end()) {
        // Index ermitteln
        int index = std::distance(sensorPins.begin(), it);
        // Letzten Trigger-Zeitstempel zurücksetzen
        lastTriggerTime[index] = get_absolute_time();
    }

    // IRQ für diesen Pin aktivieren
    gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, cabinet_gpio_callback);
    return true;
}


// Freie Callback-Funktion für GPIO-Interrupts (leitet an Instanz weiter)
// Wird vom Pico-SDK aufgerufen, um IRQs an die CabinetLight-Instanz weiterzuleiten
void cabinet_gpio_callback(uint gpio, uint32_t events) {
    
    // Singleton-Instanz abrufen
    CabinetLight* inst = CabinetLight::getInstance();
    // Wenn die Instanz existiert, Callback aufrufen
    if (inst) {
        // Callback aufrufen
        inst->gpioCallback(gpio, events);
    }
}

// Setzt das Ziellevel für eine LED (Fading wird aktiviert)
// Wird aufgerufen, wenn eine LED ein- oder ausgeschaltet werden soll
void CabinetLight::fadeLed(uint gpio, bool on) {

    // Finde den Index des GPIO in der ledPins-Liste
    auto it = std::find(ledPins.begin(), ledPins.end(), static_cast<uint8_t>(gpio));
    if (it == ledPins.end()) return;    // Pin nicht gefunden
    // Index ermitteln
    size_t idx = std::distance(ledPins.begin(), it);
    // Neues Ziellevel setzen
    uint16_t newTarget = on ? PWM_WRAP : 0;
    // Nur wenn sich das Ziellevel ändert, Fading aktivieren
    if (targetLevel[idx] != newTarget) {
        targetLevel[idx] = newTarget;   // Ziellevel setzen
        fading[idx] = true; // Fading nur aktivieren, wenn sich das Ziellevel ändert
    }
}

// Statischer IRQ-Handler: leitet an Instanz weiter
// Wird von der freien Callback-Funktion aufgerufen
void CabinetLight::gpioCallback(uint gpio, uint32_t events) {

    logDebug("gpioCallback: GPIO %d, events=0x%08x\n", gpio, events);
    // Singleton-Instanz abrufen
    CabinetLight* inst = getInstance();
    if (!inst) return;
    inst->onGpioIrq(gpio);  // IRQ-Event weiterleiten
}

// IRQ-Event: Setzt das Pending-Bit für den betroffenen Sensor
// Wird von gpioCallback() aufgerufen, um das Event in die Hauptschleife zu signalisieren
void CabinetLight::onGpioIrq(uint gpio) {
    // Finde den Index des GPIO in der sensorPins-Liste
    for (int i = 0; i < static_cast<int>(DEV_COUNT); ++i) {
        if (sensorPins[i] == gpio) {
            logDebug("onGpioIrq: matched sensor index %d (gpio %d)\n", i, gpio);
            // Setze das Pending-Bit für diesen Sensor (atomar)
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
            logDebug("process: sensor %d gpio=%d state=%d door_open=%d ledState=%d\n", i, sensorPins[i], gpio_state, door_open, ledState[i]);
            if (door_open && !ledState[i]) {
                // Tür wurde geöffnet, LED einschalten (faden)
                logDebug("process: opening detected on sensor %d -> fade on\n", i);
                fadeLed(ledPins[i], true);
                ledState[i] = true;
            } else if (!door_open && ledState[i]) {
                // Tür wurde geschlossen, LED ausschalten (faden)
                logDebug("process: closing detected on sensor %d -> fade off\n", i);
                fadeLed(ledPins[i], false);
                ledState[i] = false;
            }
        }
    }

    // 2. Polling-Fallback: prüft regelmäßig die Sensor-GPIOs (falls IRQs verloren gehen)
    if (pollingFallback) {
        for (int i = 0; i < static_cast<int>(DEV_COUNT); ++i) {
            bool raw = gpio_get(sensorPins[i]) != 0;
            if (raw != lastRawState[i]) {
                absolute_time_t now = get_absolute_time();
                // Entprellung auch beim Polling beachten
                if (absolute_time_diff_us(lastTriggerTime[i], now) >= DEBOUNCE_MS * 1000) {
                    lastTriggerTime[i] = now;
                    logDebug("[POLL] sensor %d raw=%d (changed)\n", i, raw);
                    bool door_open = sensorActiveLow[i] ? (raw == 0) : (raw != 0);
                    logDebug("[POLL] sensor %d door_open=%d\n", i, door_open);
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
        // PWM-Level setzen (LED heller/dunkler)
        pwm_set_gpio_level(ledPins[i], currentLevel[i]);
        if (currentLevel[i] == targetLevel[i]) fading[i] = false;
        sleep_ms(FADING_STEP_MS); // Fading-Schritt-Delay jetzt als constexpr
    }
}

// Setzt neue LED-Pins und initialisiert PWM für diese
// Kann zur Laufzeit aufgerufen werden, um die LED-Pinbelegung zu ändern
bool CabinetLight::setLedPins(const std::array<uint8_t, DEV_COUNT>& pins) {
    bool ok = true;
    // Pins auf Gültigkeit prüfen
    for (uint8_t g : pins) {
        if (g > 29) {
            printf("[ERROR] Ungültiger LED-Pin: %d\n", g);
            ok = false;
        }
    }
    if (!ok) return false;

    // Alte PWM-Kanäle deaktivieren
    for (uint8_t g : ledPins) {
        uint slice = pwm_gpio_to_slice_num(g);
        pwm_set_enabled(slice, false);
    }
    ledPins = pins;

    // Neue PWM-Kanäle initialisieren
    for (uint8_t g : ledPins) {
        if (!setupPwmLEDs(g)) ok = false;
    }
    return ok;
}

// Setzt neue Sensor-Pins und initialisiert IRQs für diese
// Kann zur Laufzeit aufgerufen werden, um die Sensor-Pinbelegung zu ändern
bool CabinetLight::setSensorPins(const std::array<uint8_t, DEV_COUNT>& pins) {
    bool ok = true;
    // Pins auf Gültigkeit prüfen
    for (uint8_t g : pins) {
        if (g > 29) {
            printf("[ERROR] Ungültiger Sensor-Pin: %d\n", g);
            ok = false;
        }
    }
    if (!ok) return false;

    // Alte IRQs deaktivieren
    for (uint8_t g : sensorPins) {
        gpio_set_irq_enabled(g, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);
    }
    sensorPins = pins;

    // Neue Sensoren initialisieren
    for (uint8_t g : sensorPins) {
        if (!setupSensors(g)) ok = false;
    }
    return ok;
}

// Setzt die Sensor-Polarity (active-low/active-high) für alle Kanäle
// true = active-low (Standard), false = active-high
void CabinetLight::setSensorPolarity(const std::array<bool, DEV_COUNT>& polarity) {
    sensorActiveLow = polarity;
}

// Lässt alle LEDs nacheinander kurz aufleuchten (Test beim Start)
// Kann zur Funktionsprüfung beim Systemstart verwendet werden
void CabinetLight::runStartupTest() {
    logInfo("[TEST] Running startup LED test...\n");
    for (size_t i = 0; i < DEV_COUNT; ++i) {
        uint8_t g = ledPins[i];
        logInfo("[TEST] Blink LED on GPIO %d\n", g);
        pwm_set_gpio_level(g, PWM_WRAP); // LED an
        sleep_ms(STARTUP_LED_ON_MS);
        pwm_set_gpio_level(g, 0);        // LED aus
        sleep_ms(STARTUP_LED_OFF_MS);
    }
    logInfo("[TEST] Startup LED test completed.\n");
}

// Aktiviert oder deaktiviert das Polling-Fallback für Sensoren
// Sollte nur bei Problemen mit IRQs aktiviert werden
void CabinetLight::setPollingFallback(bool enable) {
    pollingFallback = enable;
    logInfo("Polling-Fallback %s\n", enable ? "aktiviert" : "deaktiviert");
}

// Gibt zurück, ob das Polling-Fallback aktiv ist
bool CabinetLight::getPollingFallback() const {
    return pollingFallback;
}

// Blinkt die Onboard-LED (z.B. Boot- oder Heartbeat-Anzeige)
// Kann für Statusanzeigen verwendet werden
void CabinetLight::blinkOnboardLed(int times, int on_ms, int off_ms) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < times; ++i) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(on_ms);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(off_ms);
    }
}

// Endlosschleife für Fehleranzeige (Onboard-LED schnelles Blinken)
// Wird bei fatalen Fehlern aufgerufen und blockiert das System
[[noreturn]] void CabinetLight::fatalErrorBlink() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    while (true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(100);
    }
}

// === Logging-Implementierung ===
// Statisches LogLevel-Flag (global für alle Instanzen)
CabinetLight::LogLevel CabinetLight::logLevel = CabinetLight::LogLevel::INFO;

// Setzt das globale LogLevel
void CabinetLight::setLogLevel(LogLevel level) {
    logLevel = level;
}

// Gibt das aktuelle LogLevel zurück
CabinetLight::LogLevel CabinetLight::getLogLevel() {
    return logLevel;
}

// Gibt eine Fehlermeldung aus (sofern LogLevel >= ERROR)
void CabinetLight::logError(const char* fmt, ...) {
    if (logLevel >= LogLevel::ERROR) {
        printf("[ERROR] ");
        va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args);
    }
}

// Gibt eine Warnung aus (sofern LogLevel >= WARN)
void CabinetLight::logWarn(const char* fmt, ...) {
    if (logLevel >= LogLevel::WARN) {
        printf("[WARN] ");
        va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args);
    }
}

// Gibt eine Info-Meldung aus (sofern LogLevel >= INFO)
void CabinetLight::logInfo(const char* fmt, ...) {
    if (logLevel >= LogLevel::INFO) {
        printf("[INFO] ");
        va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args);
    }
}

// Gibt eine Debug-Meldung aus (sofern LogLevel >= DEBUG)
void CabinetLight::logDebug(const char* fmt, ...) {
    if (logLevel >= LogLevel::DEBUG) {
        printf("[DEBUG] ");
        va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args);
    }
}