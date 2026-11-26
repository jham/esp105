// The heat pump as a Matter Room Air Conditioner: On/Off, Thermostat and Fan Control
// clusters on one endpoint, kept in step with the indoor unit over CN105.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
//
// How the unit maps onto Matter:
//
//   Unit power          On/Off OnOff, and Thermostat SystemMode Off while powered off
//   Unit mode           SystemMode Heat, Cool, Dry, FanOnly or Auto
//   Unit setpoint       OccupiedHeatingSetpoint in HEAT, OccupiedCoolingSetpoint in COOL/DRY.
//                       In AUTO the unit has one setpoint; Matter has a heat/cool range, so the
//                       unit gets the middle of the range, and a setpoint change made on the unit
//                       shifts the range without changing its width.
//   Room temperature    LocalTemperature; outside temperature (when reported) OutdoorTemperature
//   Operating flag      ThermostatRunningState (heat, cool, fan)
//   Fan                 Fan Control with 5 speeds (1 = quiet, 2..5 = speeds 1..4) plus Auto
//   Vertical vane       Fan Control rocking: RockUpDown = swing, otherwise the unit's AUTO vane
//
// Changes made by a Matter controller arrive here on the Matter thread and are queued for the
// CN105 task. State read from the unit arrives on the CN105 task and is reported to Matter.
// Attribute callbacks that the reporting itself triggers run on the CN105 task and are ignored,
// so a report never loops back to the unit as a command.

#include "heat_pump_device.h"

#include <cmath>
#include <cstdlib>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <app/clusters/fan-control-server/CodegenIntegration.h>
#include <app/clusters/fan-control-server/FanControlCluster.h>
#include <app/clusters/fan-control-server/fan-control-delegate.h>

#include "board.h"
#include "cn105.h"
#include "status_leds.h"

using namespace esp_matter;
using namespace chip::app::Clusters;
using chip::Protocols::InteractionModel::Status;

namespace heat_pump_device {

namespace {

const char *TAG = "heat_pump";

// Thermostat SystemModeEnum values (Matter spec)
constexpr uint8_t kSystemOff = 0;
constexpr uint8_t kSystemAuto = 1;
constexpr uint8_t kSystemCool = 3;
constexpr uint8_t kSystemHeat = 4;
constexpr uint8_t kSystemFanOnly = 7;
constexpr uint8_t kSystemDry = 8;
constexpr uint8_t kControlSequenceCoolingAndHeating = 4;

// ThermostatRunningStateBitmap
constexpr uint16_t kRunningHeat = 0x0001;
constexpr uint16_t kRunningCool = 0x0002;
constexpr uint16_t kRunningFan = 0x0004;

// Temperatures are in 0.01 C. The dead band attribute is in 0.1 C.
constexpr int8_t kDeadBandAttr = 10;
constexpr int16_t kDeadBand = kDeadBandAttr * 10;
constexpr int16_t kUnitMin = cn105::kMinSetpointC * 100;
constexpr int16_t kUnitMax = cn105::kMaxSetpointC * 100;
// The heat limits start one dead band lower and the cool limits end one higher, so the
// other setpoint never stops the active one reaching the unit's full 16..31 C range.
constexpr int16_t kHeatMin = kUnitMin - kDeadBand;
constexpr int16_t kHeatMax = kUnitMax;
constexpr int16_t kCoolMin = kUnitMin;
constexpr int16_t kCoolMax = kUnitMax + kDeadBand;

constexpr uint8_t kFanSpeeds = 5;
constexpr float kLedDim = 0.3f;

uint16_t s_endpoint = 0;
TaskHandle_t s_device_task = nullptr; // the CN105 task, while it reports to Matter

// What we know of the unit, plus the Matter setpoints. Guarded by s_mutex.
SemaphoreHandle_t s_mutex = nullptr;
bool s_have_settings = false;
bool s_power = false;
cn105::Mode s_mode = cn105::Mode::kAuto;
bool s_half_degrees = false;
int16_t s_heat_sp = 2000;
int16_t s_cool_sp = 2400;

struct Lock {
    Lock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_mutex); }
};

bool on_device_task()
{
    return s_device_task != nullptr && xTaskGetCurrentTaskHandle() == s_device_task;
}

uint8_t mode_to_system(cn105::Mode mode)
{
    switch (mode) {
    case cn105::Mode::kHeat:
        return kSystemHeat;
    case cn105::Mode::kCool:
        return kSystemCool;
    case cn105::Mode::kDry:
        return kSystemDry;
    case cn105::Mode::kFan:
        return kSystemFanOnly;
    case cn105::Mode::kAuto:
        return kSystemAuto;
    }
    return kSystemAuto;
}

bool system_to_mode(uint8_t system_mode, cn105::Mode *mode)
{
    switch (system_mode) {
    case kSystemHeat:
        *mode = cn105::Mode::kHeat;
        return true;
    case kSystemCool:
        *mode = cn105::Mode::kCool;
        return true;
    case kSystemDry:
        *mode = cn105::Mode::kDry;
        return true;
    case kSystemFanOnly:
        *mode = cn105::Mode::kFan;
        return true;
    case kSystemAuto:
        *mode = cn105::Mode::kAuto;
        return true;
    default:
        return false; // EmergencyHeat, Precooling, Sleep: nothing on the unit matches
    }
}

float to_celsius(int16_t centi)
{
    return centi / 100.0f;
}

int16_t to_centi(float c)
{
    return static_cast<int16_t>(std::lround(c * 100.0f));
}

// The unit's setpoint for a mode, from the Matter setpoints. Call with s_mutex held.
bool unit_setpoint_for(cn105::Mode mode, float *celsius)
{
    switch (mode) {
    case cn105::Mode::kHeat:
        *celsius = to_celsius(s_heat_sp);
        return true;
    case cn105::Mode::kCool:
    case cn105::Mode::kDry:
        *celsius = to_celsius(s_cool_sp);
        return true;
    case cn105::Mode::kAuto: {
        float mid = to_celsius((s_heat_sp + s_cool_sp) / 2);
        // Round to the unit's step; the protocol layer clamps to its range.
        *celsius = s_half_degrees ? std::round(mid * 2.0f) / 2.0f : std::round(mid);
        return true;
    }
    case cn105::Mode::kFan:
        return false;
    }
    return false;
}

// --- Matter -> unit -------------------------------------------------------------------------

void power_on_change(cn105::Change *c)
{
    c->set_power = true;
    c->power = true;
    c->set_mode = true;
    c->mode = s_mode;
}

class FanDelegate : public FanControl::Delegate {
public:
    Status HandleStep(FanControl::StepDirectionEnum direction, bool wrap, bool lowest_off) override
    {
        cn105::State st = cn105::get_state();
        int speed = st.have_settings ? cn105::fan_to_speed(st.settings.fan) : 0;
        if (speed == 0) {
            speed = 3; // stepping from Auto starts in the middle
        }
        const int lowest = lowest_off ? 0 : 1;
        if (direction == FanControl::StepDirectionEnum::kIncrease) {
            speed = speed < kFanSpeeds ? speed + 1 : (wrap ? lowest : kFanSpeeds);
        } else {
            speed = speed > lowest ? speed - 1 : (wrap ? kFanSpeeds : lowest);
        }
        cn105::Change c;
        if (speed == 0) {
            c.set_power = true;
            c.power = false;
        } else {
            Lock lock;
            power_on_change(&c);
            c.set_fan = true;
            c.fan = cn105::speed_to_fan(speed);
        }
        cn105::request(c);
        return Status::Success;
    }

    void OnFanDriveStateChanged(const FanControl::FanDriveState &state) override
    {
        if (on_device_task()) {
            return; // our own report of the unit's fan
        }
        cn105::Change c;
        using FanControl::FanModeEnum;
        if (state.mode == FanModeEnum::kOff) {
            c.set_power = true;
            c.power = false;
        } else {
            Lock lock;
            power_on_change(&c);
            c.set_fan = true;
            if (state.mode == FanModeEnum::kAuto || state.mode == FanModeEnum::kSmart) {
                c.fan = cn105::Fan::kAuto;
            } else if (!state.speedSetting.IsNull() && state.speedSetting.Value() > 0) {
                c.fan = cn105::speed_to_fan(state.speedSetting.Value());
            } else if (state.mode == FanModeEnum::kLow) {
                c.fan = cn105::Fan::kSpeed1;
            } else if (state.mode == FanModeEnum::kMedium) {
                c.fan = cn105::Fan::kSpeed2;
            } else {
                c.fan = cn105::Fan::kSpeed4; // High, On
            }
        }
        cn105::request(c);
    }

    void OnRockSettingChanged(chip::BitMask<FanControl::RockBitmap> value) override
    {
        if (on_device_task()) {
            return;
        }
        cn105::Change c;
        c.set_vane = true;
        c.vane = value.Has(FanControl::RockBitmap::kRockUpDown) ? cn105::Vane::kSwing : cn105::Vane::kAuto;
        cn105::request(c);
    }
};

FanDelegate s_fan_delegate;

// --- Unit -> Matter -------------------------------------------------------------------------

void report(uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t val)
{
    attribute::report(s_endpoint, cluster_id, attribute_id, &val);
}

// New Matter setpoints for what the unit reported. Call with s_mutex held.
void setpoints_for_unit(const cn105::Settings &s, int16_t *heat, int16_t *cool)
{
    const int16_t t = to_centi(s.setpoint_c);
    *heat = s_heat_sp;
    *cool = s_cool_sp;
    switch (s.mode) {
    case cn105::Mode::kHeat:
        *heat = t;
        if (*cool < t + kDeadBand) {
            *cool = t + kDeadBand;
        }
        break;
    case cn105::Mode::kCool:
    case cn105::Mode::kDry:
        *cool = t;
        if (*heat > t - kDeadBand) {
            *heat = t - kDeadBand;
        }
        break;
    case cn105::Mode::kAuto: {
        float current;
        unit_setpoint_for(cn105::Mode::kAuto, &current);
        if (to_centi(current) != t) {
            // Changed on the unit (or first contact): move the range, keep its width.
            int half = (s_cool_sp - s_heat_sp) / 2;
            if (half < kDeadBand) {
                half = kDeadBand;
            }
            *heat = static_cast<int16_t>(t - half);
            *cool = static_cast<int16_t>(t + half);
        }
        break;
    }
    case cn105::Mode::kFan:
        break;
    }
    if (*heat < kHeatMin) {
        *heat = kHeatMin;
    }
    if (*cool > kCoolMax) {
        *cool = kCoolMax;
    }
}

void report_setpoints(int16_t heat, int16_t cool)
{
    int16_t old_heat, old_cool;
    {
        Lock lock;
        old_heat = s_heat_sp;
        old_cool = s_cool_sp;
        s_heat_sp = heat;
        s_cool_sp = cool;
    }
    (void)old_cool;
    // Order the writes so the pair never breaks the dead band in between.
    if (heat > old_heat) {
        report(Thermostat::Id, Thermostat::Attributes::OccupiedCoolingSetpoint::Id, esp_matter_int16(cool));
        report(Thermostat::Id, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, esp_matter_int16(heat));
    } else {
        report(Thermostat::Id, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, esp_matter_int16(heat));
        report(Thermostat::Id, Thermostat::Attributes::OccupiedCoolingSetpoint::Id, esp_matter_int16(cool));
    }
}

uint16_t running_state(const cn105::State &st)
{
    if (!st.have_settings || !st.settings.power) {
        return 0;
    }
    const cn105::Settings &s = st.settings;
    if (s.mode == cn105::Mode::kFan) {
        return kRunningFan;
    }
    if (!st.have_status || !st.status.operating) {
        return 0;
    }
    switch (s.mode) {
    case cn105::Mode::kHeat:
        return kRunningHeat | kRunningFan;
    case cn105::Mode::kCool:
    case cn105::Mode::kDry:
        return kRunningCool | kRunningFan;
    case cn105::Mode::kAuto:
        // The unit does not say which way AUTO is working; guess from the room temperature.
        if (st.have_room) {
            return (st.room.room_c < s.setpoint_c ? kRunningHeat : kRunningCool) | kRunningFan;
        }
        return kRunningFan;
    case cn105::Mode::kFan:
        break;
    }
    return 0;
}

void update_leds(const cn105::State &st)
{
    float heat = 0.0f, cool = 0.0f;
    if (st.have_settings && st.settings.power) {
        uint16_t running = running_state(st);
        if (running & kRunningHeat) {
            heat = 1.0f;
        }
        if (running & kRunningCool) {
            cool = 1.0f;
        }
        switch (st.settings.mode) {
        case cn105::Mode::kHeat:
            if (heat == 0.0f) {
                heat = kLedDim;
            }
            break;
        case cn105::Mode::kCool:
        case cn105::Mode::kDry:
            if (cool == 0.0f) {
                cool = kLedDim;
            }
            break;
        case cn105::Mode::kAuto:
            if (heat == 0.0f && cool == 0.0f) {
                heat = cool = kLedDim;
            }
            break;
        case cn105::Mode::kFan:
            break;
        }
    }
    status_leds::set_climate(heat, cool);
}

void report_fan(const cn105::Settings &s)
{
    using FanControl::FanModeEnum;
    lock::ScopedChipStackLock lock(portMAX_DELAY);
    FanControlCluster *fan = FanControl::FindClusterOnEndpoint(s_endpoint);
    if (!fan) {
        return;
    }
    if (!s.power) {
        fan->SetFanMode(FanModeEnum::kOff);
    } else if (s.fan == cn105::Fan::kAuto) {
        fan->SetFanMode(FanModeEnum::kAuto);
    } else {
        const uint8_t speed = static_cast<uint8_t>(cn105::fan_to_speed(s.fan));
        fan->SetSpeedSetting(chip::app::DataModel::MakeNullable(speed));
        fan->SetSpeedCurrent(speed);
        fan->SetPercentCurrent(static_cast<chip::Percent>(speed * 100 / kFanSpeeds));
    }
    chip::BitMask<FanControl::RockBitmap> rock;
    if (s.vane == cn105::Vane::kSwing) {
        rock.Set(FanControl::RockBitmap::kRockUpDown);
    }
    fan->SetRockSetting(rock);
}

void on_state(const cn105::State &st, void *)
{
    s_device_task = xTaskGetCurrentTaskHandle();
    update_leds(st);

    if (st.have_room) {
        report(Thermostat::Id, Thermostat::Attributes::LocalTemperature::Id,
               esp_matter_nullable_int16(nullable<int16_t>(to_centi(st.room.room_c))));
        if (st.room.has_outside) {
            report(Thermostat::Id, Thermostat::Attributes::OutdoorTemperature::Id,
                   esp_matter_nullable_int16(nullable<int16_t>(to_centi(st.room.outside_c))));
        }
    }
    report(Thermostat::Id, Thermostat::Attributes::ThermostatRunningState::Id,
           esp_matter_bitmap16(running_state(st)));

    if (!st.have_settings) {
        return;
    }
    const cn105::Settings &s = st.settings;
    int16_t heat, cool;
    {
        Lock lock;
        s_have_settings = true;
        s_power = s.power;
        s_mode = s.mode;
        s_half_degrees = s.half_degrees;
        setpoints_for_unit(s, &heat, &cool);
    }
    report_setpoints(heat, cool);
    report(OnOff::Id, OnOff::Attributes::OnOff::Id, esp_matter_bool(s.power));
    report(Thermostat::Id, Thermostat::Attributes::SystemMode::Id,
           esp_matter_enum8(s.power ? mode_to_system(s.mode) : kSystemOff));
    report_fan(s);
}

} // namespace

esp_err_t on_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                              esp_matter_attr_val_t *val)
{
    if (endpoint_id != s_endpoint || on_device_task()) {
        return ESP_OK;
    }
    cn105::Change c;
    Lock lock;

    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        if (val->val.b) {
            power_on_change(&c);
        } else {
            c.set_power = true;
            c.power = false;
        }
    } else if (cluster_id == Thermostat::Id) {
        switch (attribute_id) {
        case Thermostat::Attributes::SystemMode::Id: {
            if (val->val.u8 == kSystemOff) {
                c.set_power = true;
                c.power = false;
                break;
            }
            cn105::Mode mode;
            if (!system_to_mode(val->val.u8, &mode)) {
                ESP_LOGW(TAG, "System mode %u is not supported", val->val.u8);
                return ESP_ERR_NOT_SUPPORTED;
            }
            s_mode = mode;
            power_on_change(&c);
            if (unit_setpoint_for(mode, &c.setpoint_c)) {
                c.set_setpoint = true;
            }
            break;
        }
        case Thermostat::Attributes::OccupiedHeatingSetpoint::Id:
            s_heat_sp = val->val.i16;
            if (s_mode == cn105::Mode::kHeat || s_mode == cn105::Mode::kAuto) {
                c.set_setpoint = unit_setpoint_for(s_mode, &c.setpoint_c);
            }
            break;
        case Thermostat::Attributes::OccupiedCoolingSetpoint::Id:
            s_cool_sp = val->val.i16;
            if (s_mode == cn105::Mode::kCool || s_mode == cn105::Mode::kDry || s_mode == cn105::Mode::kAuto) {
                c.set_setpoint = unit_setpoint_for(s_mode, &c.setpoint_c);
            }
            break;
        default:
            break;
        }
    }

    if (!c.empty()) {
        cn105::request(c);
    }
    return ESP_OK;
}

esp_err_t create(node_t *node)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }

    endpoint::room_air_conditioner::config_t config;
    config.on_off.on_off = false;
    config.thermostat.control_sequence_of_operation = kControlSequenceCoolingAndHeating;
    config.thermostat.system_mode = kSystemOff;
    config.thermostat.feature_flags = cluster::thermostat::feature::heating::get_id() |
                                      cluster::thermostat::feature::cooling::get_id() |
                                      cluster::thermostat::feature::auto_mode::get_id();
    config.thermostat.features.heating.occupied_heating_setpoint = s_heat_sp;
    config.thermostat.features.cooling.occupied_cooling_setpoint = s_cool_sp;
    config.thermostat.features.auto_mode.min_setpoint_dead_band = kDeadBandAttr;

    endpoint_t *ep = endpoint::room_air_conditioner::create(node, &config, ENDPOINT_FLAG_NONE, nullptr);
    if (!ep) {
        ESP_LOGE(TAG, "Could not create the Room Air Conditioner endpoint");
        return ESP_FAIL;
    }
    s_endpoint = endpoint::get_id(ep);

    cluster_t *thermostat = cluster::get(ep, Thermostat::Id);
    if (!thermostat) {
        return ESP_FAIL;
    }
    namespace ta = cluster::thermostat::attribute;
    ta::create_abs_min_heat_setpoint_limit(thermostat, kHeatMin);
    ta::create_abs_max_heat_setpoint_limit(thermostat, kHeatMax);
    ta::create_abs_min_cool_setpoint_limit(thermostat, kCoolMin);
    ta::create_abs_max_cool_setpoint_limit(thermostat, kCoolMax);
    ta::create_min_heat_setpoint_limit(thermostat, kHeatMin);
    ta::create_max_heat_setpoint_limit(thermostat, kHeatMax);
    ta::create_min_cool_setpoint_limit(thermostat, kCoolMin);
    ta::create_max_cool_setpoint_limit(thermostat, kCoolMax);
    ta::create_outdoor_temperature(thermostat, nullable<int16_t>());
    ta::create_thermostat_running_state(thermostat, 0);

    cluster::fan_control::config_t fan_config;
    fan_config.fan_mode = static_cast<uint8_t>(FanControl::FanModeEnum::kOff);
    fan_config.fan_mode_sequence = static_cast<uint8_t>(FanControl::FanModeSequenceEnum::kOffLowMedHighAuto);
    fan_config.percent_setting = nullable<uint8_t>(0);
    fan_config.percent_current = 0;
    fan_config.delegate = &s_fan_delegate;
    cluster_t *fan = cluster::fan_control::create(ep, &fan_config, CLUSTER_FLAG_SERVER);
    if (!fan) {
        ESP_LOGE(TAG, "Could not create the Fan Control cluster");
        return ESP_FAIL;
    }
    cluster::fan_control::feature::multi_speed::config_t speeds;
    speeds.speed_max = kFanSpeeds;
    cluster::fan_control::feature::multi_speed::add(fan, &speeds);
    cluster::fan_control::feature::fan_auto::add(fan);
    cluster::fan_control::feature::rocking::config_t rocking;
    rocking.rock_support = static_cast<uint8_t>(FanControl::RockBitmap::kRockUpDown);
    cluster::fan_control::feature::rocking::add(fan, &rocking);

    ESP_LOGI(TAG, "Room Air Conditioner on endpoint %u", s_endpoint);
    return ESP_OK;
}

esp_err_t start()
{
    cn105::Config config;
    config.uart_num = board::kHeatPumpUart;
    config.tx_pin = board::kHeatPumpTx;
    config.rx_pin = board::kHeatPumpRx;
    return cn105::start(config, on_state, nullptr);
}

uint16_t endpoint_id()
{
    return s_endpoint;
}

} // namespace heat_pump_device
