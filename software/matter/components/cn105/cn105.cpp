// CN105 heat pump link: UART task that keeps the connection up, polls the unit and
// sends requested changes.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include "cn105.h"

#include <cstring>

#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace cn105 {

namespace {

const char *TAG = "cn105";

constexpr TickType_t kResponseTimeout = pdMS_TO_TICKS(1000);
constexpr TickType_t kConnectRetry = pdMS_TO_TICKS(3000);
// The unit takes a moment to apply a change before its settings block shows it.
constexpr TickType_t kSettleAfterSet = pdMS_TO_TICKS(1000);
// Requests in a row without a valid answer before the link counts as lost.
constexpr int kMaxMisses = 6;

Config s_config;
StateCallback s_callback = nullptr;
void *s_callback_ctx = nullptr;
TaskHandle_t s_task = nullptr;

SemaphoreHandle_t s_mutex = nullptr; // guards s_state and s_pending
State s_state;
Change s_pending;

Parser s_parser;

bool same_settings(const Settings &a, const Settings &b)
{
    return a.power == b.power && a.mode == b.mode && a.isee == b.isee && a.setpoint_c == b.setpoint_c &&
           a.half_degrees == b.half_degrees && a.fan == b.fan && a.vane == b.vane && a.wide_vane == b.wide_vane;
}

bool same_room(const RoomTemp &a, const RoomTemp &b)
{
    return a.room_c == b.room_c && a.has_outside == b.has_outside && a.outside_c == b.outside_c;
}

void notify(const State &state)
{
    if (s_callback) {
        s_callback(state, s_callback_ctx);
    }
}

void write_packet(const uint8_t *packet, size_t len)
{
    uart_flush_input(static_cast<uart_port_t>(s_config.uart_num));
    s_parser.reset();
    uart_write_bytes(static_cast<uart_port_t>(s_config.uart_num), packet, len);
    ESP_LOG_BUFFER_HEX_LEVEL("cn105 tx", packet, len, ESP_LOG_DEBUG);
}

// Waits for a packet of the given type. Returns false on timeout.
bool read_packet(PacketType want, TickType_t timeout)
{
    TickType_t start = xTaskGetTickCount();
    uint8_t byte;
    while (xTaskGetTickCount() - start < timeout) {
        int n = uart_read_bytes(static_cast<uart_port_t>(s_config.uart_num), &byte, 1, pdMS_TO_TICKS(50));
        if (n != 1) {
            continue;
        }
        if (!s_parser.feed(byte)) {
            continue;
        }
        ESP_LOG_BUFFER_HEX_LEVEL("cn105 rx", s_parser.packet(), s_parser.length(), ESP_LOG_DEBUG);
        if (s_parser.type() == want) {
            return true;
        }
        ESP_LOGD(TAG, "Ignoring packet type 0x%02x", static_cast<unsigned>(s_parser.type()));
    }
    return false;
}

bool connect()
{
    uint8_t packet[kRequestLen];
    size_t len = build_connect(packet);
    write_packet(packet, len);
    return read_packet(PacketType::kConnectAck, kResponseTimeout);
}

// Applies one info response to the shared state. Returns true if anything changed.
bool apply_info(const uint8_t *data, size_t len, State *state)
{
    switch (static_cast<InfoCode>(data[0])) {
    case InfoCode::kSettings: {
        Settings s;
        if (!decode_settings(data, len, &s)) {
            ESP_LOGW(TAG, "Unreadable settings block");
            return false;
        }
        bool changed = !state->have_settings || !same_settings(s, state->settings);
        state->have_settings = true;
        state->settings = s;
        return changed;
    }
    case InfoCode::kRoomTemp: {
        RoomTemp r;
        if (!decode_room_temp(data, len, &r)) {
            return false;
        }
        bool changed = !state->have_room || !same_room(r, state->room);
        state->have_room = true;
        state->room = r;
        return changed;
    }
    case InfoCode::kStatus: {
        Status st;
        if (!decode_status(data, len, &st)) {
            return false;
        }
        bool changed = !state->have_status || st.compressor_hz != state->status.compressor_hz ||
                       st.operating != state->status.operating;
        state->have_status = true;
        state->status = st;
        return changed;
    }
    }
    return false;
}

// Asks for one info block and applies the answer. Returns false if the unit did not answer.
bool poll(InfoCode code)
{
    uint8_t packet[kRequestLen];
    size_t len = build_info_request(code, packet);
    write_packet(packet, len);
    if (!read_packet(PacketType::kInfoResponse, kResponseTimeout)) {
        return false;
    }
    if (s_parser.data()[0] != static_cast<uint8_t>(code)) {
        ESP_LOGD(TAG, "Asked for 0x%02x, got 0x%02x", static_cast<unsigned>(code), s_parser.data()[0]);
        return true; // the link is alive; the block simply was not the one we asked for
    }

    State snapshot;
    bool changed = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (code == InfoCode::kSettings && !s_pending.empty()) {
        // A change was requested after this poll went out. The reply still shows the old
        // settings; reporting them would briefly undo the change in the controller's app.
        xSemaphoreGive(s_mutex);
        return true;
    }
    changed = apply_info(s_parser.data(), s_parser.data_length(), &s_state);
    snapshot = s_state;
    xSemaphoreGive(s_mutex);
    if (changed) {
        notify(snapshot);
    }
    return true;
}

// Sends the pending change, if any. Returns false if the unit did not acknowledge it.
bool send_pending(bool *sent)
{
    Change change;
    bool half_degrees;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    change = s_pending;
    s_pending = Change();
    half_degrees = s_state.settings.half_degrees;
    xSemaphoreGive(s_mutex);

    *sent = !change.empty();
    if (!*sent) {
        return true;
    }
    uint8_t packet[kRequestLen];
    size_t len = build_set(change, half_degrees, packet);
    ESP_LOGI(TAG, "Sending change: power %d/%d mode %d/0x%02x setpoint %d/%.1f fan %d/0x%02x vane %d/0x%02x",
             change.set_power, change.power, change.set_mode, static_cast<unsigned>(change.mode),
             change.set_setpoint, change.setpoint_c, change.set_fan, static_cast<unsigned>(change.fan),
             change.set_vane, static_cast<unsigned>(change.vane));
    write_packet(packet, len);
    if (read_packet(PacketType::kSetAck, kResponseTimeout)) {
        return true;
    }
    ESP_LOGW(TAG, "No acknowledgement for the change; will retry");
    // Put it back under anything requested since, so newer values still win.
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    change.merge(s_pending);
    s_pending = change;
    xSemaphoreGive(s_mutex);
    return false;
}

void set_connected(bool connected)
{
    State snapshot;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool changed = s_state.connected != connected;
    s_state.connected = connected;
    snapshot = s_state;
    xSemaphoreGive(s_mutex);
    if (changed) {
        ESP_LOGI(TAG, "%s", connected ? "Connected to the heat pump" : "Lost the heat pump connection");
        notify(snapshot);
    }
}

void task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(s_config.start_delay_ms));
    const InfoCode kPollOrder[] = {InfoCode::kSettings, InfoCode::kRoomTemp, InfoCode::kStatus};

    while (true) {
        if (!connect()) {
            ESP_LOGW(TAG, "No answer to the handshake; check the cable");
            set_connected(false);
            ulTaskNotifyTake(pdTRUE, kConnectRetry);
            continue;
        }
        set_connected(true);

        int misses = 0;
        while (misses < kMaxMisses) {
            TickType_t round_start = xTaskGetTickCount();
            for (InfoCode code : kPollOrder) {
                bool sent = false;
                if (!send_pending(&sent)) {
                    misses++;
                } else if (sent) {
                    misses = 0;
                    vTaskDelay(kSettleAfterSet);
                }
                if (poll(code)) {
                    misses = 0;
                } else {
                    misses++;
                }
            }
            // Wait out the rest of the round, waking early when a change is requested.
            TickType_t elapsed = xTaskGetTickCount() - round_start;
            TickType_t interval = pdMS_TO_TICKS(s_config.poll_interval_ms);
            if (elapsed < interval) {
                ulTaskNotifyTake(pdTRUE, interval - elapsed);
            }
        }
        set_connected(false);
    }
}

} // namespace

esp_err_t start(const Config &config, StateCallback callback, void *ctx)
{
    if (s_task) {
        return ESP_ERR_INVALID_STATE;
    }
    s_config = config;
    s_callback = callback;
    s_callback_ctx = ctx;
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }

    const uart_config_t uart_config = {
        .baud_rate = 2400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    uart_port_t port = static_cast<uart_port_t>(config.uart_num);
    esp_err_t err = uart_driver_install(port, 256, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(port, &uart_config);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(port, config.tx_pin, config.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(task, "cn105", 4096, nullptr, 5, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void request(const Change &change)
{
    if (!s_mutex) {
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_pending.merge(change);
    xSemaphoreGive(s_mutex);
    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

State get_state()
{
    State snapshot;
    if (s_mutex) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        snapshot = s_state;
        xSemaphoreGive(s_mutex);
    }
    return snapshot;
}

} // namespace cn105
