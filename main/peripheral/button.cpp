#include "button.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <iostream>

#define TICK_INTERVAL 10 // 10ms

#define TICKS_TIME_PRESS_DEFAULT (100 / TICK_INTERVAL)
#define TICKS_TIME_KEEP_DEFAULT (1000 / TICK_INTERVAL)
#define TICKS_TIME_LONG_KEEP_DEFAULT (5000 / TICK_INTERVAL)

#define TIME_TO_TICKS(time, tick_default) ((0 == (time)) ? tick_default : (((time) / TICK_INTERVAL) < tick_default) ? tick_default \
                                                                                                                    : ((time) / TICK_INTERVAL))

static inline uint8_t button_gpio_get_key_level(void *gpio_num)
{
    return gpio_get_level(static_cast<gpio_num_t>(reinterpret_cast<uintptr_t>(gpio_num)));
}

Button::Button(gpio_num_t pin, uint8_t activeLevel, bool isPull, uint16_t pressTimeMs, uint16_t keepTimeMs) : _pin(pin),
                                                                                                              _activeLevel(activeLevel),
                                                                                                              _isPull(isPull)
{
    _pressTick = TIME_TO_TICKS(pressTimeMs, TICKS_TIME_PRESS_DEFAULT);
    _keepTick = TIME_TO_TICKS(keepTimeMs, TICKS_TIME_KEEP_DEFAULT);
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << _pin);
    if (!_isPull)
    { // disable pull-up and pull-down
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    }
    else
    {
        if (_activeLevel)
        {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        }
        else
        {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        }
    }
    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE("Button", "Failed to configure GPIO");
    }
    _button_dev.hal_button_get_Level = button_gpio_get_key_level;
    _button_dev.hardware_data = (void *)(uintptr_t)_pin;
    _button_dev.event = ButtonEvent::BUTTON_EVENT_NONE_PRESS;
    _button_dev.ticks = 0;
}

esp_err_t Button::registerCallback(Observer *obs, void *usr_data)
{
    if (_keepTick <= _pressTick)
    {
        ESP_LOGE("Button", "keep time is less than press time");
        return ESP_FAIL;
    }

    button_cb_info_t cb_info = {
        .usr_data = usr_data};
    _button_dev.cbInfos = cb_info;
    auto isExits = [obs](const std::vector<Observer *> &obsList)
    {
        return std::find(obsList.begin(), obsList.end(), obs) != obsList.end();
    };
    if (!isExits(_observers))
    {
        _observers.push_back(obs);
    }
    return ESP_OK;
}

esp_err_t Button::unregisterCallback(Observer *obs)
{
    _button_dev.cbInfos.usr_data = nullptr;
    _observers.erase(
        std::remove(_observers.begin(), _observers.end(), obs),
        _observers.end());
    return ESP_OK;
}

void Button::notify(ButtonEvent event, void *usr_data)
{
    for (auto &observer : _observers)
    {
        observer->btnPostEvt(event, usr_data);
    }
}

void Button::handleEvent()
{
    uint8_t level = _button_dev.hal_button_get_Level(_button_dev.hardware_data);
    if (level == _activeLevel)
    {
        _button_dev.ticks++;
    }
    else
    {
        _button_dev.ticks = 0;
    }
    if (_button_dev.event == ButtonEvent::BUTTON_EVENT_NONE_PRESS && _button_dev.ticks >= _pressTick)
    {
        _button_dev.event = ButtonEvent::BUTTON_EVENT_PRESS;
    }
    else if (_button_dev.event == ButtonEvent::BUTTON_EVENT_PRESS && _button_dev.ticks >= _keepTick)
    {
        _button_dev.event = ButtonEvent::BUTTON_EVENT_KEEPING;
        // cb keeping
        notify(ButtonEvent::BUTTON_EVENT_KEEPING, _button_dev.cbInfos.usr_data);
    }
    else if (_button_dev.ticks == 0)
    {
        if (_button_dev.event == ButtonEvent::BUTTON_EVENT_KEEPING)
        {
            _button_dev.event = ButtonEvent::BUTTON_EVENT_RELEASE_KEEPING;
            // cb release keeping
            notify(ButtonEvent::BUTTON_EVENT_RELEASE_KEEPING, _button_dev.cbInfos.usr_data);
        }
        else if (_button_dev.event == ButtonEvent::BUTTON_EVENT_PRESS)
        {
            // cb press
            notify(ButtonEvent::BUTTON_EVENT_PRESS, _button_dev.cbInfos.usr_data);
        }
        _button_dev.event = ButtonEvent::BUTTON_EVENT_NONE_PRESS;
    }
}

/*======================================================
/                   BUTTON MANAGER                     /
======================================================*/
ESP_EVENT_DEFINE_BASE(BUTTON_EVENT_BASE);

ButtonManager *ButtonManager::instance = nullptr;
static esp_event_loop_handle_t btn_event_loop;

ButtonManager::ButtonManager(uint8_t numBtn)
{
    // cấp phát sẵn, tránh phân mảnh
    _buttons.reserve(numBtn);
    _btnInfos.reserve(numBtn);
    g_button_timer_handle = NULL;
    _btnIndexCheckPair = 0xff;
    _tickStartPairK9b = 0;
}

ButtonManager::~ButtonManager()
{
    // dùng _buttons.clear() chỉ xóa toàn bộ phần tử nhưng RAM vẫn bị giữ
    // ví dụ sau khi clear: _buttons.size() = 0 nhưng _buttons.capacity() vẫn = numBtn
    std::vector<Button>().swap(_buttons);
    std::vector<button_info_t>().swap(_btnInfos);
    g_button_timer_handle = NULL;
}

ButtonManager *ButtonManager::getInstance()
{
    if (instance == nullptr)
    {
        instance = new ButtonManager(BUTTON_NUM);
    }
    return instance;
}

void ButtonManager::deleteInstance()
{
    delete instance;
    instance = nullptr;
}

esp_err_t ButtonManager::init()
{
    addButton(BUTTON1_GPIO, 0, true, CONFIG_PRESS_TIME_MS, CONFIG_KEEP_TIME_MS, BUTTON_INDEX_1, "Button_1");
    addButton(BUTTON2_GPIO, 0, true, CONFIG_PRESS_TIME_MS, CONFIG_KEEP_TIME_MS, BUTTON_INDEX_2, "Button_2");
    addButton(BUTTON3_GPIO, 0, true, CONFIG_PRESS_TIME_MS, CONFIG_KEEP_TIME_MS, BUTTON_INDEX_3, "Button_3");
    addButton(BUTTON4_GPIO, 0, true, CONFIG_PRESS_TIME_MS, CONFIG_KEEP_TIME_MS, BUTTON_INDEX_4, "Button_4");
    if (!g_button_timer_handle)
    {
        esp_timer_create_args_t button_timer = {0};
        button_timer.arg = this;
        button_timer.callback = handleEvents;
        button_timer.dispatch_method = ESP_TIMER_TASK;
        button_timer.name = "button_timer";
        esp_timer_create(&button_timer, &g_button_timer_handle);
    }
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    return ESP_OK;
}

void ButtonManager::addButton(gpio_num_t pin, uint8_t activeLevel, bool isPull, uint16_t pressTimeMs, uint16_t keepTimeMs, uint8_t btnIndex, const char *name)
{
    Button button(pin, activeLevel, isPull, pressTimeMs, keepTimeMs);
    _buttons.push_back(button);
    // _buttons.emplace_back(in, activeLevel, isPull, pressTime, keepTime);

    button_info_t btn_info = {
        .btnIndex = btnIndex,
        .is_keeping = 0,
        .is_long_keeping = 0};
    strncpy(btn_info.name, name, sizeof(btn_info.name) - 1);
    btn_info.name[sizeof(btn_info.name) - 1] = '\0';
    _btnInfos.push_back(btn_info);
    button_info_t *m_btnTemp = &_btnInfos.back();
    _buttons.back().registerCallback(this, (void *)(m_btnTemp));
}

void ButtonManager::removeButton(gpio_num_t pin)
{
    auto it = std::find_if(_buttons.begin(), _buttons.end(), [pin](const Button &btn)
                           { return btn.getPin() == pin; });
    if (it != _buttons.end())
    {
        _buttons.erase(it);
    }
}

void ButtonManager::handleEvents(void *args)
{
    ButtonManager *btnManager = static_cast<ButtonManager *>(args);
    for (auto &button : btnManager->_buttons)
    {
        button.handleEvent();
    }
    btnManager->handle();
}

void ButtonManager::handle(){
    if (_btnIndexCheckPair != 0xff && (xTaskGetTickCount() - _tickStartPairK9b) > pdMS_TO_TICKS(CLOCK_TIME_OUT_MS_PAIR_K9B))
    {
        // timeout
        _btnIndexCheckPair = 0xff;
        std::cout<<"Time out pair k9b"<< std::endl;
    }
}

void ButtonManager::btnPostEvt(ButtonEvent event, void *usr_data){
    button_info_t *btn =  static_cast<button_info_t *>(usr_data);
    switch (event)
    {
    case ButtonEvent::BUTTON_EVENT_PRESS:{
        std::cout << btn->name << " pressed" << std::endl;
        esp_event_post(BUTTON_EVENT_BASE, EVENT_BUTTON_PRESS, &btn->btnIndex, 1, pdMS_TO_TICKS(10));
        if(_btnIndexCheckPair == btn->btnIndex)
        {
            _btnIndexCheckPair = 0xff;
            esp_event_post(BUTTON_EVENT_BASE, EVENT_BUTTON_DELETE_ALL_K9B, &btn->btnIndex, 1, pdMS_TO_TICKS(10));
        }
        break;
    }
    case ButtonEvent::BUTTON_EVENT_KEEPING:{
        std::cout << btn->name << " kept" << std::endl;
        _btnIndexCheckPair = btn->btnIndex;
        _tickStartPairK9b = xTaskGetTickCount();
        esp_event_post(BUTTON_EVENT_BASE, EVENT_BUTTON_PAIR_K9B, &btn->btnIndex, 1, pdMS_TO_TICKS(10));
        break;
    }
    case ButtonEvent::BUTTON_EVENT_RELEASE_KEEPING:{
        std::cout << btn->name << " released after being kept" << std::endl;
        esp_event_post(BUTTON_EVENT_BASE, EVENT_BUTTON_CONFIG_WIFI, &btn->btnIndex, 1, pdMS_TO_TICKS(10));
        break;
    }
    case ButtonEvent::BUTTON_EVENT_LONG_KEEPING:{
        std::cout << btn->name << " long keeping" << std::endl;
        break;
    }
    case ButtonEvent::BUTTON_EVENT_RELEASE_LONG_KEEPING:
        std::cout << btn->name << "release long keeping" << std::endl;
        break;
    
    default:
        break;
    }
}

esp_err_t ButtonManager::startListening()
{
    return esp_timer_start_periodic(g_button_timer_handle, 10 * 1000U); // 10ms
}

esp_err_t ButtonManager::stopListening()
{
    return esp_timer_stop(g_button_timer_handle);
}