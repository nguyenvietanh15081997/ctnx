#include "dao/IOT_DaoMgmt.h"
#include "IOT_Log.h"
#include "IOT_Esp32Drivers.h"
#include "network/IOT_NetworkMgmt.h"
#include "IOT_Platform.h"
#include "ble/IOT_BleMgmt.h"
#if CONFIG_IOT_LOG_TCP_ENABLED
#include "log/IOT_LogTcp.h"
#endif

#if !defined(CONFIG_BT_ENABLED) && !defined(CONFIG_IOT_TCP_SERVER_ENABLED)
#error "No provisioning channel: enable Bluetooth (menuconfig) or CONFIG_IOT_TCP_SERVER_ENABLED"
#endif

iot_err_t IOT_InitPlatform(void)
{
    log_init();
    IOT_Esp32DriversInit();

    iot_err_t err = IOT_DaoInit();
    if (err != IOT_OK)
    {
        IOT_LOGE("platform", "Failed to init dao manager");
        return err;
    }
    IOT_NetworkIface ifaces[] = {{.type = WIFI, .number = 0}, {.type = NETWORK_TYPE_NONE, .number = 0}};
    err = IOT_NwkInit(ifaces);
    if (err != IOT_OK)
    {
        IOT_LOGE("platform", "Failed to init network manager");
        return err;
    }
#if CONFIG_IOT_LOG_TCP_ENABLED
    IOT_LogTcpInit(); /* register IP_EVENT handler; server starts on GOT_IP */
#endif
    err = IOT_BleInit();
    if (err != IOT_OK)
    {
        IOT_LOGE("platform", "Failed to init ble manager");
        return err;
    }
    IOT_LOGI("Platform", "Platform initialized successfully");
    return IOT_OK;
}

iot_err_t IOT_DeinitPlatform(void)
{
    IOT_BleDeinit();
    IOT_NetworkIface ifaces[] = {{.type = WIFI, .number = 0}, {.type = NETWORK_TYPE_NONE, .number = 0}};
    IOT_NwkDeinit(ifaces);
    IOT_DaoDeinit();
    log_cleanup();
    return IOT_OK;
}