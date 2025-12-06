#pragma once

#include <furi.h>

/**
 * @brief Battery and power optimization
 */

typedef enum {
    PowerModeNormal,      // Full performance
    PowerModeLowPower,    // Reduced scanning
    PowerModeUltraLow,    // Minimal activity
} PowerMode;

typedef struct PowerManager PowerManager;

/**
 * @brief Create power manager
 * @return Power manager instance
 */
PowerManager* power_manager_create(void);

/**
 * @brief Free power manager
 * @param pm Power manager
 */
void power_manager_free(PowerManager* pm);

/**
 * @brief Set power mode
 * @param pm Power manager
 * @param mode Power mode
 */
void power_manager_set_mode(PowerManager* pm, PowerMode mode);

/**
 * @brief Get current power mode
 * @param pm Power manager
 * @return Current power mode
 */
PowerMode power_manager_get_mode(PowerManager* pm);

/**
 * @brief Get scan interval for current power mode (in ms)
 * @param pm Power manager
 * @return Scan interval in milliseconds
 */
uint32_t power_manager_get_scan_interval(PowerManager* pm);

/**
 * @brief Get advertising interval for current power mode (in ms)
 * @param pm Power manager
 * @return Advertising interval in milliseconds
 */
uint32_t power_manager_get_advertising_interval(PowerManager* pm);

/**
 * @brief Should perform scan now (duty cycling)
 * @param pm Power manager
 * @return true if scan should be performed
 */
bool power_manager_should_scan(PowerManager* pm);

/**
 * @brief Should advertise now (duty cycling)
 * @param pm Power manager
 * @return true if advertising should be active
 */
bool power_manager_should_advertise(PowerManager* pm);

/**
 * @brief Update power manager state (call periodically)
 * @param pm Power manager
 */
void power_manager_update(PowerManager* pm);

/**
 * @brief Get battery level (0-100)
 * @return Battery percentage
 */
uint8_t power_manager_get_battery_level(void);
