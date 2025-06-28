/**
 * @file main.cpp
 * @brief Firmware-Einstiegspunkt für die automatisierte Schrankbeleuchtung.
 *
 * Automatisierte LED-Beleuchtung für Schränke mit bis zu vier LED-Gruppen,
 * gesteuert über Reedkontakte und MOSFETs auf Basis eines Raspberry Pi Pico W.
 * Die Firmware initialisiert die Hardware und verarbeitet Sensorereignisse,
 * um die LEDs je nach Türzustand zu dimmen.
 *
 * Features:
 * - Bis zu 4 separat schaltbare LED-Gruppen
 * - Automatische Steuerung über Magnetsensoren oder Reed-Schalter
 * - Versorgung mit 12 V DC, intern auf 5 V und 3.3 V geregelt
 * - Geringe Standby-Leistung durch Low-RDS(on)-MOSFETs
 * - Erweiterbare Anschlüsse über XH-Steckerleisten
 *
 * @author Knut Welzel <knut.welzel@gmail.com>
 * @date 2025-06-28
 * @copyright MIT
 */

#include "cabinetLight.h"

// Globale Instanz der CabinetLight-Klasse
// Diese Instanz wird im Hauptprogramm verwendet, um die CabinetLight-Funktionen zu nutzen
CabinetLight *cabinetLight = nullptr;

/**
 * @brief Hauptfunktion des Programms
 * 
 * Initialisiert die CabinetLight-Instanz und startet die Endlosschleife.
 * In dieser Schleife wird die Firmware im Leerlauf gehalten, bis ein Interrupt
 * von einem Sensor ausgelöst wird.
 * 
 * @return int Rückgabewert des Programms (0 bei Erfolg)
 */
int main() {
    
    // Erstelle eine neue Instanz der CabinetLight-Klasse
    // Diese Instanz initialisiert die GPIO-Pins und PWM-Ausgänge für die LEDs
    cabinetLight = new CabinetLight();
    
    // Überprüfe, ob die CabinetLight-Instanz erfolgreich erstellt wurde
    // Wenn die Instanz nicht erstellt werden konnte, gibt es einen Fehler
    if (!cabinetLight) {
        return -1; // Fehler beim Erstellen der CabinetLight Instanz
    }

    // Endlosschleife, die die Firmware im Leerlauf hält
    // Diese Schleife wird durch Interrupts von den Sensoren unterbrochen,
    // um die LEDs entsprechend der Türzustände zu steuern.
    // In dieser Schleife wird die Firmware im Leerlauf gehalten, bis ein Interrupt
    // von einem Sensor ausgelöst wird.
    // Diese Schleife ist notwendig, um die Firmware am Laufen zu halten,
    // da die Verarbeitung der Sensorereignisse in den Interrupt-Handlern erfolgt.
    while (true) {

        // Halte die Firmware im Leerlauf
        tight_loop_contents(); // Idle loop
    }
}