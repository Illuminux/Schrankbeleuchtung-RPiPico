#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <array>

constexpr uint LED_PINS[] = {2, 3, 4, 5};       // PWM-Ausgänge für MOSFETs
constexpr uint REED_PINS[] = {6, 7, 8, 9};      // Reedkontakte
constexpr uint PWM_WRAP = 65535;                // 16-bit PWM
constexpr uint DEBOUNCE_MS = 100;               // Sperrzeit nach Interrupt

std::array<absolute_time_t, 4> last_trigger_time;
std::array<bool, 4> led_state = {false, false, false, false};

/**
 * @brief Initialisiert PWM für einen bestimmten GPIO-Pin
 */
void setup_pwm(uint gpio) {

    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice, PWM_WRAP);
    pwm_set_enabled(slice, true);
}

/**
 * @brief Dimmt eine LED sanft hoch oder runter
 */
void fade_led(uint gpio, bool on) {

    uint slice = pwm_gpio_to_slice_num(gpio);
    uint channel = pwm_gpio_to_channel(gpio);

    if (on) {
        for (uint16_t level = 0; level <= PWM_WRAP; level += 1000) {
            pwm_set_chan_level(slice, channel, level);
            sleep_ms(2);
        }
    } else {
        for (int level = PWM_WRAP; level >= 0; level -= 1000) {
            pwm_set_chan_level(slice, channel, level);
            sleep_ms(2);
        }
    }
}

/**
 * @brief GPIO-Interrupt-Handler für Reedkontakte
 */
void gpio_callback(uint gpio, uint32_t events) {
    
    for (int i = 0; i < 4; ++i) {
        if (gpio == REED_PINS[i]) {
            absolute_time_t now = get_absolute_time();
            if (absolute_time_diff_us(last_trigger_time[i], now) < DEBOUNCE_MS * 1000) {
                return; // Sperrzeit aktiv
            }
            last_trigger_time[i] = now;

            bool door_open = gpio_get(gpio) == 0; // Reedkontakt geschlossen = Tür offen

            if (door_open && !led_state[i]) {
                fade_led(LED_PINS[i], true);
                led_state[i] = true;
            } else if (!door_open && led_state[i]) {
                fade_led(LED_PINS[i], false);
                led_state[i] = false;
            }
        }
    }
}

int main() {
    stdio_init_all();

    // PWM-Setup
    for (uint gpio : LED_PINS) {
        setup_pwm(gpio);
    }

    // Reedkontakte einrichten
    for (int i = 0; i < 4; ++i) {
        gpio_init(REED_PINS[i]);
        gpio_set_dir(REED_PINS[i], GPIO_IN);
        gpio_pull_up(REED_PINS[i]);
        last_trigger_time[i] = get_absolute_time();

        gpio_set_irq_enabled_with_callback(REED_PINS[i],
            GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    }

    while (true) {
        tight_loop_contents(); // Idle loop
    }
}
