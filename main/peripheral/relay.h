#pragma once

#include "driver/gpio.h"


enum class RelayState {
    OFF = 0,
    ON = 1,
    NONE = 0xff
};

class Relay {
public:
    Relay(gpio_num_t gpio_num, uint8_t activeLevel, uint8_t index);

    void setState(RelayState state);
    uint8_t getIndex() const { return _index; }

private:
    gpio_num_t _gpio_num;
    RelayState _state;
    uint8_t _activeLevel;
    uint8_t _index;

    uint8_t getRealLevel(RelayState state);

    void setHardware(RelayState state);
};

namespace RelayMgmt {
    const gpio_num_t RELAY_GPIO_NUMS[] = {GPIO_NUM_42, GPIO_NUM_39, GPIO_NUM_41, GPIO_NUM_40};
    const uint8_t RELAY_ACTIVE_LEVELS[] = {1, 1, 1, 1}; // 1 for active high, 0 for active low
    const uint8_t NUM_RELAYS = sizeof(RELAY_GPIO_NUMS) / sizeof(RELAY_GPIO_NUMS[0]);
    // extern std::vector<Relay> relays; // có thể dùng luôn inline, ko cần extern 
    void init();
    void setRelayState(uint8_t index, RelayState state);
}


















