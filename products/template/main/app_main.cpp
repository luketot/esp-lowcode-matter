// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include <system.h>
#include <low_code.h>

#include "app_priv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#define GARAGE_DOOR_TRIGGER_PIN GPIO_NUM_10

#define TRIGGER_PULSE_MS 200

#define LOW_CODE_FEATURE_ID_BARRIER_CONTROL 7001
#define LOW_CODE_FEATURE_ID_BARRIER_CURRENT_POSITION 7002

static const char *TAG = "app_main";

static bool s_door_state_closed = true;

static int app_driver_gpio_init()
{
    // Configure the GPIO pin as a push-pull output
    gpio_reset_pin(GARAGE_DOOR_TRIGGER_PIN);
    gpio_set_direction(GARAGE_DOOR_TRIGGER_PIN, GPIO_MODE_OUTPUT);

    // Set the initial state to inactive (low)
    gpio_set_level(GARAGE_DOOR_TRIGGER_PIN, 0);

    printf("%s: Initialized GPIO %d for trigger.\n", TAG, GARAGE_DOOR_TRIGGER_PIN);
    return 0;
}

static void trigger_momentary_pulse()
{
    printf("%s: Starting momentary trigger pulse.\n", TAG);

    // 1. Set the pin HIGH to activate the relay
    gpio_set_level(GARAGE_DOOR_TRIGGER_PIN, 1);

    // 2. Wait for the defined pulse duration (200ms)
    vTaskDelay(pdMS_TO_TICKS(TRIGGER_PULSE_MS));

    // 3. Set the pin LOW to deactivate the relay
    gpio_set_level(GARAGE_DOOR_TRIGGER_PIN, 0);

    printf("%s: Trigger pulse finished.\n", TAG);
}

static void setup()
{
    /* Register callbacks */
    low_code_register_callbacks(feature_update_from_system, event_from_system);

    /* Initialize driver */
    app_driver_init();

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
        if (feature_id == LOW_CODE_FEATURE_ID_BARRIER_CONTROL) {  // TargetPosition
            // The value received is the target position (0 for closed, 100 for open).
            uint8_t target_position_value = *(uint8_t *)data->value.value;
            printf("%s: Feature update: Barrier Target Position: %d. Triggering pulse.\n", TAG, target_position_value);

            // Trigger the actual remote pulse
            trigger_momentary_pulse();

            // Toggle the local assumed state
            s_door_state_closed = !s_door_state_closed;

            // Report the new current position back to the system to simulate movement.
            // Barrier Control CurrentPosition attribute: 0 = Open, 100 = Closed.
            uint8_t current_position = s_door_state_closed ? 100 : 0;
            printf("%s: Reporting new assumed state: %s (%d)\n", TAG, 
                   s_door_state_closed ? "Closed" : "Open", current_position);

            low_code_send_feature_update_to_system(endpoint_id, 
                                                  LOW_CODE_FEATURE_ID_BARRIER_CURRENT_POSITION, 
                                                  &current_position);

            return 0; // Return success after triggering the pulse
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
