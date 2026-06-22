#pragma once

#include <stdint.h>

/**
 * @brief Inicializa el LED normal (GPIO 21) y el LED RGB WS2812 (GPIO 48 via RMT).
 */
void leds_init(void);

/**
 * @brief Establece el color del LED RGB WS2812.
 * @param r  Componente rojo   (0–255)
 * @param g  Componente verde  (0–255)
 * @param b  Componente azul   (0–255)
 */
void led_rgb_set(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Enciende o apaga el LED normal.
 * @param on  1 para encender, 0 para apagar
 */
void led_gpio_set(int on);
