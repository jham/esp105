# A fake Mitsubishi indoor unit on a UART, for testing the CN105 firmware without a heat pump.
# Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_OUTSIDE_TEMPERATURE = "outside_temperature"
CONF_HALF_DEGREES = "half_degrees"
CONF_UPDATE_INTERVAL = "update_interval"

cn105_sim_ns = cg.esphome_ns.namespace("cn105_sim")
Cn105Sim = cn105_sim_ns.class_("Cn105Sim", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Cn105Sim),
            # Where the room settles when the unit is off or idle
            cv.Optional(CONF_ROOM_TEMPERATURE, default=21.0): cv.float_range(min=10, max=35),
            cv.Optional(CONF_OUTSIDE_TEMPERATURE, default=8.0): cv.float_range(min=-20, max=45),
            # Report and accept 0.5 degree setpoints, as newer units do
            cv.Optional(CONF_HALF_DEGREES, default=True): cv.boolean,
            # How often the simulated room temperature moves by 0.5 degrees
            cv.Optional(CONF_UPDATE_INTERVAL, default="20s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "cn105_sim",
    baud_rate=2400,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_room_temperature(config[CONF_ROOM_TEMPERATURE]))
    cg.add(var.set_outside_temperature(config[CONF_OUTSIDE_TEMPERATURE]))
    cg.add(var.set_half_degrees(config[CONF_HALF_DEGREES]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
