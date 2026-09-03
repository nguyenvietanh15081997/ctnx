#include "relay.h"
#include "esp_log.h"
#include "esp_err.h"
#include <iostream>
#include <vector>

Relay::Relay(gpio_num_t gpio_num, uint8_t activeLevel, uint8_t index)
    : _gpio_num(gpio_num), _activeLevel(activeLevel), _index(index)
{
    this->_state = RelayState::NONE;
    esp_err_t ret = ESP_OK;
    gpio_config_t gpio_config_pin = {
        .pin_bit_mask = 1ULL << (this->_gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&gpio_config_pin);
    if (ret != ESP_OK)
    {
        ESP_LOGE("Relay", "config relay gpio FAIL");
    }
}

uint8_t Relay::getRealLevel(RelayState state)
{
    if (this->_activeLevel) // active level high
    {
        return state == RelayState::ON ? 1 : 0;
    }
    else // active level low
    {
        return state == RelayState::ON ? 0 : 1;
    }
}

void Relay::setHardware(RelayState state)
{
    std::cout << "set relay hardware state: " << (state == RelayState::ON ? "ON" : "OFF") << std::endl;
    uint8_t level = getRealLevel(state);
    gpio_set_level(this->_gpio_num, level);
}

void Relay::setState(RelayState state)
{
    if (this->_state != state)
    {
        this->_state = state;
        setHardware(state);
    }
}

namespace RelayMgmt {
    std::vector<Relay> relays;
}

void RelayMgmt::init()
{
    if (!relays.empty())
        return;
    relays.reserve(NUM_RELAYS);
    for (uint8_t i = 0; i < NUM_RELAYS; ++i)
    {
        relays.emplace_back(RELAY_GPIO_NUMS[i], RELAY_ACTIVE_LEVELS[i], i);
    }
}

void RelayMgmt::setRelayState(uint8_t index, RelayState state)
{
    for (auto &relay : relays)
    {
        if (relay.getIndex() == index)
        {
            relay.setState(state);
            break;
        }
    }
}