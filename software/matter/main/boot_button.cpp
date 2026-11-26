// BOOT button: short press toggles the status LEDs, a 10 second hold factory-resets.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include "boot_button.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_timer.h>

#include "board.h"
#include "status_leds.h"

namespace boot_button {

namespace {

const char *TAG = "button";

constexpr int64_t kPollUs = 20 * 1000;
constexpr int kDebounceSamples = 2;          // 40 ms
constexpr int64_t kShortPressMaxUs = 1000 * 1000;
constexpr int64_t kResetHoldUs = 10 * 1000 * 1000;

esp_timer_handle_t s_timer = nullptr;
bool s_pressed = false;
int s_stable = 0;
int64_t s_pressed_at = 0;
bool s_reset_armed = false;

void poll(void *)
{
    bool level_pressed = gpio_get_level(static_cast<gpio_num_t>(board::kBootButton)) == 0;
    if (level_pressed != s_pressed) {
        if (++s_stable < kDebounceSamples) {
            return;
        }
        s_stable = 0;
        s_pressed = level_pressed;
        int64_t now = esp_timer_get_time();
        if (s_pressed) {
            s_pressed_at = now;
            return;
        }
        // Released
        int64_t held = now - s_pressed_at;
        if (s_reset_armed) {
            ESP_LOGW(TAG, "BOOT held for %lld s: factory reset", held / 1000000);
            esp_matter::factory_reset();
            return;
        }
        if (held <= kShortPressMaxUs) {
            status_leds::toggle_enabled();
        }
        return;
    }
    s_stable = 0;

    if (s_pressed && !s_reset_armed && esp_timer_get_time() - s_pressed_at >= kResetHoldUs) {
        // All LEDs light up; letting go now erases the commissioning data and restarts.
        s_reset_armed = true;
        status_leds::factory_reset_warning(true);
        ESP_LOGW(TAG, "Release BOOT to factory reset");
    }
}

} // namespace

esp_err_t init()
{
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << board::kBootButton;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE; // R12 on the board
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }

    const esp_timer_create_args_t args = {
        .callback = poll,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "button",
        .skip_unhandled_events = true,
    };
    err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(s_timer, kPollUs);
}

} // namespace boot_button
