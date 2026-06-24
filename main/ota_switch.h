#pragma once

/**
 * @brief Switches the boot partition to OTA_1 and reboots.
 *        The board will boot the main application firmware from OTA_1.
 */
void ota_switch_trigger(void);

/**
 * @brief Initializes the switch GPIO and starts the monitoring task.
 *        When a press >5 seconds is detected, calls ota_switch_trigger().
 * @param sw_gpio Switch GPIO number (active low, internal pull-up)
 */
void ota_switch_start(int sw_gpio);