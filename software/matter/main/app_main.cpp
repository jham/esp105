// Matter-over-Thread firmware for the esp105 CN105 adapter, board 0.3.0.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include <cstring>

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_openthread_types.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <platform/ESP32/OpenthreadLauncher.h>

#include "boot_button.h"
#include "heat_pump_device.h"
#include "status_leds.h"

using namespace esp_matter;

namespace {

const char *TAG = "app_main";

constexpr auto kCommissioningWindowSeconds = 300;

// Works out what the PWR LED should show. Runs on the Matter thread.
void update_network_led()
{
    chip::Server &server = chip::Server::GetInstance();
    status_leds::Network network;
    if (server.GetCommissioningWindowManager().IsCommissioningWindowOpen()) {
        network = status_leds::Network::kCommissioning;
    } else if (server.GetFabricTable().FabricCount() == 0) {
        network = status_leds::Network::kNotCommissioned;
    } else if (chip::DeviceLayer::ConnectivityMgr().IsThreadAttached()) {
        network = status_leds::Network::kConnected;
    } else {
        network = status_leds::Network::kDisconnected;
    }
    status_leds::set_network(network);
}

void app_event_cb(const ChipDeviceEvent *event, intptr_t)
{
    namespace DeviceEventType = chip::DeviceLayer::DeviceEventType;
    switch (event->Type) {
    case DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Commissioning failed: fail-safe timer expired");
        break;
    case DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;
    case DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;
    case DeviceEventType::kThreadStateChange:
        ESP_LOGI(TAG, "Thread %s", chip::DeviceLayer::ConnectivityMgr().IsThreadAttached() ? "attached" : "detached");
        break;
    case DeviceEventType::kFabricRemoved: {
        ESP_LOGI(TAG, "Fabric removed");
        // With no fabric left, open a window so the board can be paired again without a reset.
        chip::CommissioningWindowManager &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0 && !mgr.IsCommissioningWindowOpen()) {
            CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(kCommissioningWindowSeconds),
                                                              chip::CommissioningWindowAdvertisement::kDnssdOnly);
            if (err != CHIP_NO_ERROR) {
                ESP_LOGE(TAG, "Could not open a commissioning window: %" CHIP_ERROR_FORMAT, err.Format());
            }
        }
        break;
    }
    default:
        break;
    }
    update_network_led();
}

esp_err_t identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                            uint8_t effect_variant, void *)
{
    ESP_LOGI(TAG, "Identify: type %u, endpoint %u, effect %u", type, endpoint_id, effect_id);
    switch (type) {
    case identification::callback_type_t::START:
        status_leds::identify(kCommissioningWindowSeconds); // until STOP
        break;
    case identification::callback_type_t::STOP:
        status_leds::identify(0);
        break;
    case identification::callback_type_t::EFFECT:
        status_leds::identify(3);
        break;
    }
    return ESP_OK;
}

esp_err_t attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                              uint32_t attribute_id, esp_matter_attr_val_t *val, void *)
{
    if (type == attribute::PRE_UPDATE) {
        return heat_pump_device::on_attribute_update(endpoint_id, cluster_id, attribute_id, val);
    }
    return ESP_OK;
}

} // namespace

extern "C" void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(status_leds::init());
    ESP_ERROR_CHECK(boot_button::init());

    node::config_t node_config;
    std::strncpy(node_config.root_node.basic_information.node_label, "ESP105",
                 sizeof(node_config.root_node.basic_information.node_label) - 1);
    node_t *node = node::create(&node_config, attribute_update_cb, identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Could not create the Matter node");
        return;
    }
    ESP_ERROR_CHECK(heat_pump_device::create(node));

    // The C6's own 802.15.4 radio, no host processor
    esp_openthread_platform_config_t ot_config = {};
    ot_config.radio_config.radio_mode = RADIO_MODE_NATIVE;
    ot_config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
    ot_config.port_config.storage_partition_name = "nvs";
    ot_config.port_config.netif_queue_size = 10;
    ot_config.port_config.task_queue_size = 10;
    set_openthread_platform_config(&ot_config);

    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter did not start: %s", esp_err_to_name(err));
        return;
    }
    {
        lock::ScopedChipStackLock lock(portMAX_DELAY);
        update_network_led();
    }

    ESP_ERROR_CHECK(heat_pump_device::start());

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
#endif
}
