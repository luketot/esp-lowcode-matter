// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include <system.h> // Provides system_digital_write and pin_level_t enum
#include <low_code.h>

#include "app_priv.h"

// --- Includes for Software Timer ---
#include "sw_timer.h" 

// --- Define the GPIO pin for the remote trigger (using GPIO 10 for D10) ---
#define GARAGE_DOOR_TRIGGER_PIN 10 // Using the pin number directly for the system API

// --- Define the pulse duration in milliseconds (e.g., 200ms) ---
#define TRIGGER_PULSE_MS 200

// --- POWER FEATURE ID DEFINITION ---
// This ID is used for the On/Off cluster (Power control).
#define LOW_CODE_FEATURE_ID_POWER 1001 

// --- Global Software Timer Handle ---
static sw_timer_handle_t s_trigger_timer = NULL;

static const char *TAG = "app_main";

// --- Timer Callback: Sets the GPIO pin LOW when the pulse duration is complete ---
static void trigger_off_cb(void *arg)
{
    // 3. Set the pin LOW using the system API and the correct enum value
    system_digital_write(GARAGE_DOOR_TRIGGER_PIN, LOW);
    printf("%s: Trigger pulse finished (Software Timer complete).\n", TAG);
}

// --- Function to initialize the GPIO pin and the software timer ---
static int app_driver_gpio_init()
{
    // Set the initial state to inactive (low) using the system API and the correct enum value.
    system_digital_write(GARAGE_DOOR_TRIGGER_PIN, LOW);

    // Define the configuration structure for the software timer
    sw_timer_config_t config = {
        .timer_period_ms = TRIGGER_PULSE_MS,
        .auto_reload = false,
        .callback = trigger_off_cb,
        .arg = NULL,
    };

    // Initialize the one-shot software timer using the configuration struct
    s_trigger_timer = sw_timer_create(&config);
    if (!s_trigger_timer) {
        printf("%s: ERROR: Failed to create software timer!\n", TAG);
        return -1;
    }

    printf("%s: Initialized GPIO %d and Software Timer for trigger.\n", TAG, GARAGE_DOOR_TRIGGER_PIN);
    return 0;
}

// --- Function to generate a momentary pulse on the trigger pin ---
static void trigger_momentary_pulse()
{
    printf("%s: Starting momentary trigger pulse.\n", TAG);

    // 1. Set the pin HIGH using the system API and the correct enum value
    system_digital_write(GARAGE_DOOR_TRIGGER_PIN, HIGH);

    // 2. Start the timer to set the pin LOW after TRIGGER_PULSE_MS
    sw_timer_start(s_trigger_timer);
}


static void setup()
{
    /* Register callbacks */
    low_code_register_callbacks(feature_update_from_system, event_from_system);

    /* Initialize driver */
    app_driver_init();
    
    // Initialize the garage door trigger pin and software timer
    app_driver_gpio_init();
}

static void loop()
{
    /* The corresponding callbacks are called if data is received from system */
    low_code_get_feature_update_from_system();
    low_code_get_event_from_system();
}

int feature_update_from_system(low_code_feature_data_t *data)
{
    /* Get the feature updates */
    uint16_t endpoint_id = data->details.endpoint_id;
    uint32_t feature_id = data->details.feature_id;

    if (endpoint_id == 1) {
        if (feature_id == LOW_CODE_FEATURE_ID_POWER) {  // Power
            // Note: The extended character has been removed from this line
            bool power_value = *(bool *)data->value.value;
            printf("%s: Feature update: power: %d\n", TAG, power_value);
            
            // Only trigger the pulse when the switch is turned ON
            if (power_value) {
                // Trigger the actual remote pulse
                trigger_momentary_pulse();
                
                // Immediately report the state back as OFF (false)
                // This makes the Matter controller treat the device as a momentary button
                bool reset_value = false;
                low_code_send_feature_update_to_system(endpoint_id, 
                                                      LOW_CODE_FEATURE_ID_POWER, 
                                                      &reset_value);
            }
            
            return 0; 
        }
    }

    return 0;
}

int event_from_system(low_code_event_t *event)
{
    /* Handle the events from low_code_event_type_t */
    return app_driver_event_handler(event);
}

extern "C" int main()
{
    printf("%s: Starting low code\n", TAG);

    /* Pre-Initializations: This should be called first and should always be present */
    system_setup();
    setup();

    /* Loop */
    while (1) {
        system_loop();
        loop();
    }
    return 0;
}