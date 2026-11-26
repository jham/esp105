// The heat pump as a Matter Room Air Conditioner: On/Off, Thermostat and Fan Control
// clusters on one endpoint, kept in step with the indoor unit over CN105.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include <esp_err.h>
#include <esp_matter.h>

namespace heat_pump_device {

// Adds the endpoint to the node. Call before esp_matter::start().
esp_err_t create(esp_matter::node_t *node);

// Starts the CN105 link. Call after esp_matter::start().
esp_err_t start();

uint16_t endpoint_id();

// Forwarded from the node's attribute callback for PRE_UPDATE. Turns writes from
// Matter controllers into heat pump commands; returns an error to reject a write.
esp_err_t on_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                              esp_matter_attr_val_t *val);

} // namespace heat_pump_device
