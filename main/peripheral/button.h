#pragma once

#include <string.h>
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_timer.h"
#include <vector>
#include <stdbool.h>
#include "Utils.h"

#define BUTTON1_GPIO GPIO_NUM_6
#define BUTTON2_GPIO GPIO_NUM_7
#define BUTTON3_GPIO GPIO_NUM_38
#define BUTTON4_GPIO GPIO_NUM_18

#define BUTTON_NUM 4

ESP_EVENT_DECLARE_BASE(BUTTON_EVENT_BASE);

enum class ButtonEvent
{
    BUTTON_EVENT_PRESS = 0,            // Button press
    BUTTON_EVENT_KEEPING,              // Button is being held
    BUTTON_EVENT_RELEASE_KEEPING,      // Release after holding
    BUTTON_EVENT_LONG_KEEPING,         // Long press
    BUTTON_EVENT_RELEASE_LONG_KEEPING, // Release after long press
    BUTTON_EVENT_MAX,                  // Maximum number of events
    BUTTON_EVENT_NONE_PRESS            // No button press
};

enum ButtonEventPost{
    EVENT_BUTTON_PRESS = 1,  // press key
    EVENT_BUTTON_PAIR_K9B,   // pair CT2C 
    EVENT_BUTTON_DELETE_ALL_K9B, // delete all CT2C with one button
    EVENT_BUTTON_CONFIG_WIFI, // config Wifi
    EVENT_BUTTON_KICK_OUT,  // hard reset
    EVENT_BUTTON_INC_COUNT_KICK_OUT, 
} ;

#define CONFIG_PRESS_TIME_MS     100
#define CONFIG_KEEP_TIME_MS      3000

// #define CLOCK_TIME_MS_SET_PAIR_K9B  (3*1000 - CONFIG_KEEP_TIME_MS)
#define CLOCK_TIME_OUT_MS_KICK_OUT  (10*1000 - CONFIG_KEEP_TIME_MS)
#define CLOCK_TIME_OUT_MS_PAIR_K9B  (10*1000 - CONFIG_KEEP_TIME_MS)

struct button_cb_info_t
{
    void *usr_data = nullptr;
};

struct button_dev_t
{
    uint16_t ticks;
    uint8_t (*hal_button_get_Level)(void *hardware_data);
    void *hardware_data;
    ButtonEvent event;
    button_cb_info_t cbInfos;
};

class Button
{
public:
    class Observer
    {
    public:
        virtual void btnPostEvt(ButtonEvent event, void *usr_data) {};
        virtual ~Observer() = default;
    };
    Button(gpio_num_t pin, uint8_t activeLevel, bool isPull, uint16_t pressTimeMs, uint16_t keepTimeMs);
    ~Button() {}

    esp_err_t registerCallback(Observer *obs, void *usr_data);
    esp_err_t unregisterCallback(Observer *obs);
    void handleEvent();
    gpio_num_t getPin() const noexcept { return _pin; };

private:
    gpio_num_t _pin;
    uint8_t _activeLevel;
    bool _isPull;
    uint16_t _pressTick;
    uint16_t _keepTick;
    button_dev_t _button_dev;
    std::vector<Observer *> _observers;
    void notify(ButtonEvent event, void *usr_data);
};

struct button_info_t
{
    uint8_t btnIndex;
    uint8_t is_keeping;
    uint8_t is_long_keeping;
    char name[16];
};

class ButtonManager : public Button::Observer
{
private:
    static ButtonManager *instance;
    uint8_t _btnIndexCheckPair;
    TickType_t _tickStartPairK9b;
    std::vector<Button> _buttons;
    std::vector<button_info_t> _btnInfos;
    esp_timer_handle_t g_button_timer_handle;
    static void handleEvents(void *args);
    void handle();
    void btnPostEvt(ButtonEvent event, void *usr_data) override;
    ButtonManager(uint8_t numBtn);
    ~ButtonManager();

public:
    static ButtonManager *getInstance();
    static void deleteInstance();
    esp_err_t init();
    void addButton(gpio_num_t pin, uint8_t activeLevel, bool isPull, uint16_t pressTimeMs, uint16_t keepTimeMs, uint8_t btnIndex, const char *name);
    void removeButton(gpio_num_t pin);
    esp_err_t startListening();
    esp_err_t stopListening();
};