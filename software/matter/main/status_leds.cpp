// PWR, HEAT and COOL status LEDs.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include "status_leds.h"

#include <atomic>
#include <cmath>

#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>

#include "board.h"

namespace status_leds {

namespace {

const char *TAG = "leds";
const char *kNvsNamespace = "esp105";
const char *kNvsKeyEnabled = "leds_on";

constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_bit_t kResolution = LEDC_TIMER_10_BIT;
constexpr uint32_t kMaxDuty = (1u << 10) - 1;
constexpr int64_t kTickUs = 50 * 1000;

enum Channel { kPwr = 0, kHeat, kCool, kCount };
constexpr int kPins[kCount] = {board::kLedPwr, board::kLedHeat, board::kLedCool};
constexpr ledc_channel_t kChannels[kCount] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2};

std::atomic<Network> s_network{Network::kNotCommissioned};
std::atomic<float> s_heat{0.0f};
std::atomic<float> s_cool{0.0f};
std::atomic<bool> s_enabled{true};
std::atomic<int64_t> s_identify_until_us{0};
std::atomic<bool> s_reset_warning{false};
esp_timer_handle_t s_timer = nullptr;
uint32_t s_ticks = 0;

void write(Channel ch, float brightness)
{
    if (brightness < 0.0f) {
        brightness = 0.0f;
    } else if (brightness > 1.0f) {
        brightness = 1.0f;
    }
    // Rough gamma correction so "dim" looks dim rather than nearly full.
    uint32_t duty = static_cast<uint32_t>(std::pow(brightness, 2.2f) * kMaxDuty + 0.5f);
    ledc_set_duty(kMode, kChannels[ch], duty);
    ledc_update_duty(kMode, kChannels[ch]);
}

void tick(void *)
{
    s_ticks++;
    const bool slow_on = (s_ticks / 10) % 2 == 0; // 1 Hz
    const bool fast_on = (s_ticks / 2) % 2 == 0;  // 5 Hz

    if (s_reset_warning) {
        write(kPwr, 1.0f);
        write(kHeat, 1.0f);
        write(kCool, 1.0f);
        return;
    }
    if (esp_timer_get_time() < s_identify_until_us) {
        float v = fast_on ? 1.0f : 0.0f;
        write(kPwr, v);
        write(kHeat, v);
        write(kCool, v);
        return;
    }
    if (!s_enabled) {
        write(kPwr, 0.0f);
        write(kHeat, 0.0f);
        write(kCool, 0.0f);
        return;
    }

    float pwr = 0.0f;
    switch (s_network.load()) {
    case Network::kConnected:
        pwr = 1.0f;
        break;
    case Network::kCommissioning:
        pwr = fast_on ? 1.0f : 0.0f;
        break;
    case Network::kNotCommissioned:
    case Network::kDisconnected:
        pwr = slow_on ? 1.0f : 0.0f;
        break;
    }
    write(kPwr, pwr);
    write(kHeat, s_heat);
    write(kCool, s_cool);
}

void load_enabled()
{
    nvs_handle_t h;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &h) != ESP_OK) {
        return; // nothing saved yet
    }
    uint8_t v = 1;
    if (nvs_get_u8(h, kNvsKeyEnabled, &v) == ESP_OK) {
        s_enabled = v != 0;
    }
    nvs_close(h);
}

void save_enabled(bool enabled)
{
    nvs_handle_t h;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "Could not save the LED setting");
        return;
    }
    nvs_set_u8(h, kNvsKeyEnabled, enabled ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

} // namespace

esp_err_t init()
{
    const ledc_timer_config_t timer = {
        .speed_mode = kMode,
        .duty_resolution = kResolution,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        return err;
    }
    for (int i = 0; i < kCount; i++) {
        ledc_channel_config_t ch = {};
        ch.gpio_num = kPins[i];
        ch.speed_mode = kMode;
        ch.channel = kChannels[i];
        ch.timer_sel = LEDC_TIMER_0;
        ch.duty = 0;
        err = ledc_channel_config(&ch);
        if (err != ESP_OK) {
            return err;
        }
    }
    load_enabled();

    const esp_timer_create_args_t args = {
        .callback = tick,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "leds",
        .skip_unhandled_events = true,
    };
    err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(s_timer, kTickUs);
}

void set_network(Network network)
{
    s_network = network;
}

void set_climate(float heat, float cool)
{
    s_heat = heat;
    s_cool = cool;
}

void identify(int seconds)
{
    s_identify_until_us = seconds > 0 ? esp_timer_get_time() + static_cast<int64_t>(seconds) * 1000000 : 0;
}

void factory_reset_warning(bool on)
{
    s_reset_warning = on;
}

bool enabled()
{
    return s_enabled;
}

void set_enabled(bool enabled)
{
    s_enabled = enabled;
    save_enabled(enabled);
    ESP_LOGI(TAG, "Status LEDs %s", enabled ? "on" : "off");
}

void toggle_enabled()
{
    set_enabled(!s_enabled);
}

} // namespace status_leds
