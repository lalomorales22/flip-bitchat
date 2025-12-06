#include "power.h"
#include <furi.h>
#include <furi_hal_power.h>

#define TAG "PowerManager"

// Battery thresholds for auto power mode adjustment
#define BATTERY_ULTRA_LOW_THRESHOLD 20
#define BATTERY_LOW_POWER_THRESHOLD 50

// Scan intervals (in ms) for different power modes
#define SCAN_INTERVAL_NORMAL 1000
#define SCAN_INTERVAL_LOW_POWER 5000
#define SCAN_INTERVAL_ULTRA_LOW 15000

// Advertising intervals (in ms)
#define ADV_INTERVAL_NORMAL 500
#define ADV_INTERVAL_LOW_POWER 2000
#define ADV_INTERVAL_ULTRA_LOW 10000

// Duty cycle percentages
#define DUTY_CYCLE_NORMAL 100
#define DUTY_CYCLE_LOW_POWER 50
#define DUTY_CYCLE_ULTRA_LOW 20

struct PowerManager {
    PowerMode mode;
    uint32_t last_scan_time;
    uint32_t last_adv_time;
    bool scan_active;
    bool adv_active;
    FuriMutex* mutex;
};

PowerManager* power_manager_create(void) {
    PowerManager* pm = malloc(sizeof(PowerManager));
    
    pm->mode = PowerModeNormal;
    pm->last_scan_time = 0;
    pm->last_adv_time = 0;
    pm->scan_active = true;
    pm->adv_active = true;
    pm->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    FURI_LOG_I(TAG, "Power manager created (mode: Normal)");
    
    return pm;
}

void power_manager_free(PowerManager* pm) {
    if(!pm) return;
    
    furi_mutex_free(pm->mutex);
    free(pm);
    
    FURI_LOG_I(TAG, "Power manager freed");
}

void power_manager_set_mode(PowerManager* pm, PowerMode mode) {
    if(!pm) return;
    
    furi_mutex_acquire(pm->mutex, FuriWaitForever);
    
    pm->mode = mode;
    
    const char* mode_str = "Unknown";
    switch(mode) {
        case PowerModeNormal:
            mode_str = "Normal";
            break;
        case PowerModeLowPower:
            mode_str = "Low Power";
            break;
        case PowerModeUltraLow:
            mode_str = "Ultra Low";
            break;
    }
    
    FURI_LOG_I(TAG, "Power mode set to: %s", mode_str);
    
    furi_mutex_release(pm->mutex);
}

PowerMode power_manager_get_mode(PowerManager* pm) {
    if(!pm) return PowerModeNormal;
    
    furi_mutex_acquire(pm->mutex, FuriWaitForever);
    PowerMode mode = pm->mode;
    furi_mutex_release(pm->mutex);
    
    return mode;
}

uint32_t power_manager_get_scan_interval(PowerManager* pm) {
    if(!pm) return SCAN_INTERVAL_NORMAL;
    
    switch(pm->mode) {
        case PowerModeNormal:
            return SCAN_INTERVAL_NORMAL;
        case PowerModeLowPower:
            return SCAN_INTERVAL_LOW_POWER;
        case PowerModeUltraLow:
            return SCAN_INTERVAL_ULTRA_LOW;
        default:
            return SCAN_INTERVAL_NORMAL;
    }
}

uint32_t power_manager_get_advertising_interval(PowerManager* pm) {
    if(!pm) return ADV_INTERVAL_NORMAL;
    
    switch(pm->mode) {
        case PowerModeNormal:
            return ADV_INTERVAL_NORMAL;
        case PowerModeLowPower:
            return ADV_INTERVAL_LOW_POWER;
        case PowerModeUltraLow:
            return ADV_INTERVAL_ULTRA_LOW;
        default:
            return ADV_INTERVAL_NORMAL;
    }
}

bool power_manager_should_scan(PowerManager* pm) {
    if(!pm) return true;
    
    furi_mutex_acquire(pm->mutex, FuriWaitForever);
    
    uint32_t now = furi_get_tick();
    uint32_t interval = power_manager_get_scan_interval(pm);
    
    bool should_scan = false;
    
    if(pm->mode == PowerModeNormal) {
        // Always scan in normal mode
        should_scan = true;
    } else {
        // Duty cycling for low power modes
        uint32_t duty_cycle = (pm->mode == PowerModeLowPower) ? 
            DUTY_CYCLE_LOW_POWER : DUTY_CYCLE_ULTRA_LOW;
        
        uint32_t elapsed = now - pm->last_scan_time;
        
        if(elapsed >= interval) {
            pm->last_scan_time = now;
            pm->scan_active = true;
            should_scan = true;
        } else if(pm->scan_active) {
            // Check if we should stop scanning (duty cycle)
            uint32_t active_duration = (interval * duty_cycle) / 100;
            if(elapsed < active_duration) {
                should_scan = true;
            } else {
                pm->scan_active = false;
                should_scan = false;
            }
        }
    }
    
    furi_mutex_release(pm->mutex);
    
    return should_scan;
}

bool power_manager_should_advertise(PowerManager* pm) {
    if(!pm) return true;
    
    furi_mutex_acquire(pm->mutex, FuriWaitForever);
    
    uint32_t now = furi_get_tick();
    uint32_t interval = power_manager_get_advertising_interval(pm);
    
    bool should_adv = false;
    
    if(pm->mode == PowerModeNormal) {
        // Always advertise in normal mode
        should_adv = true;
    } else {
        // Duty cycling for low power modes
        uint32_t duty_cycle = (pm->mode == PowerModeLowPower) ? 
            DUTY_CYCLE_LOW_POWER : DUTY_CYCLE_ULTRA_LOW;
        
        uint32_t elapsed = now - pm->last_adv_time;
        
        if(elapsed >= interval) {
            pm->last_adv_time = now;
            pm->adv_active = true;
            should_adv = true;
        } else if(pm->adv_active) {
            // Check if we should stop advertising (duty cycle)
            uint32_t active_duration = (interval * duty_cycle) / 100;
            if(elapsed < active_duration) {
                should_adv = true;
            } else {
                pm->adv_active = false;
                should_adv = false;
            }
        }
    }
    
    furi_mutex_release(pm->mutex);
    
    return should_adv;
}

void power_manager_update(PowerManager* pm) {
    if(!pm) return;
    
    // Check battery level and adjust power mode if needed
    uint8_t battery = power_manager_get_battery_level();
    
    furi_mutex_acquire(pm->mutex, FuriWaitForever);
    
    PowerMode old_mode = pm->mode;
    PowerMode new_mode = pm->mode;
    
    // Auto-adjust power mode based on battery
    if(battery < BATTERY_ULTRA_LOW_THRESHOLD) {
        new_mode = PowerModeUltraLow;
    } else if(battery < BATTERY_LOW_POWER_THRESHOLD) {
        new_mode = PowerModeLowPower;
    }
    // Note: Don't auto-upgrade to normal, let user decide
    
    if(new_mode != old_mode) {
        pm->mode = new_mode;
        FURI_LOG_W(TAG, "Power mode auto-adjusted to %d due to battery level %d%%", new_mode, battery);
    }
    
    furi_mutex_release(pm->mutex);
}

uint8_t power_manager_get_battery_level(void) {
    // Get battery percentage from Flipper HAL
    uint8_t charge = furi_hal_power_get_pct();
    return charge;
}
