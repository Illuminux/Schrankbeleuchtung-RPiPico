#include "cabinetLight.h"

CabinetLight *cabinetLight = nullptr;

int main() {
    
    cabinetLight = new CabinetLight();
    
    if (!cabinetLight) {
        return -1; // Fehler beim Erstellen der CabinetLight Instanz
    }

    while (true) {
        tight_loop_contents(); // Idle loop
    }
}