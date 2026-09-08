#pragma once

#include <inttypes.h>
#include <stddef.h>
#include "IOT_ErrorManager.h"
#include "IOT_ProvisionTypes.h"
#include <stdbool.h>

/**
 * @brief Initialise the protocol engine
 *
 * @param modelId 16-character model ID, NUL-terminated, assigned by Rogo device
 *                management. Read during init only.
 */
iot_err_t IOT_CoreInit(const char *modelId);
iot_err_t IOT_CoreDeinit(void);

/**
 * @brief Set the provisioning mode.
 *
 * Call before IOT_CoreStartProvision() to select a non-default mode.
 * Currently only IOT_PROVISION_BLE_LAN is implemented; other modes
 * return IOT_ERR_NOT_SUPPORTED.
 *
 * @param mode  The provisioning mode to use
 * @return IOT_OK on success, IOT_ERR_NOT_SUPPORTED for unimplemented modes,
 *         IOT_ERR_INVALID_ARG for invalid enum values
 */
iot_err_t IOT_CoreSetProvisionMode(IOT_ProvisionMode_t mode);

/**
 * @brief Set and persist the device timezone.
 *
 * Applies the POSIX TZ string to the platform timer layer, then stores it in
 * the core config DAO so reconnects and reboots can restore the same local
 * time behavior. UTC timestamp APIs remain UTC; timezone is only for local
 * calendar/schedule logic.
 *
 * @param timeZone POSIX TZ string, e.g. "ICT-7"
 * @return IOT_OK on success, IOT_ERR_INVALID_ARG for invalid input, or a DAO/timer error
 */
iot_err_t IOT_CoreSetTimeZone(const char *timeZone);

/**
 * @brief Read the persisted device timezone from core config.
 *
 * @param outTimeZone Caller buffer for the null-terminated TZ string
 * @param outSize Size of outTimeZone in bytes
 * @return IOT_OK on success, IOT_ERR_DATA_NOT_FOUND if unset,
 *         IOT_ERR_INVALID_ARG/IOT_ERR_INVALID_SIZE for invalid output args
 */
iot_err_t IOT_CoreGetTimeZone(char *outTimeZone, size_t outSize);

/**
 * @brief Factory reset the device.
 *
 * Resets core-owned config (provision state, WiFi, MQTT, certs, etc.) and
 * dispatches DEV_EVENT_DEVICE_REMOVED (isRoot=true) so the SDK can reset
 * its own DAOs. The SDK event handler is expected to restart the chip.
 *
 * @return IOT_OK on success, IOT_ERR_DATA_NOT_FOUND if not provisioned
 */
iot_err_t IOT_CoreFactoryReset(void);

/**
 * @brief Load mesh credentials from config DAO and join the mesh network.
 *
 * Loads network key and app key from config DAO, decrypts them, builds
 * the root device key (from devId + EID), and calls IOT_MeshJoinNetwork().
 *
 * Called by SDK during init if device is already provisioned, or internally
 * by the MESH handler after the last app key is received from cloud.
 *
 * @return IOT_OK on success, IOT_ERR_DATA_NOT_FOUND if keys not yet stored
 */
/**
 * @brief Run @p handler on the core service task.
 *
 * Driver callbacks — a BLE-mesh or Zigbee stack task, a timer — run on stacks too
 * small for NVS writes or an MQTT publish, and must not be blocked. Copy what you
 * need, hand it here, and return immediately.
 *
 * Public because an SDK implementing a hub transport needs exactly this and should
 * not have to stand up a worker task of its own to get it.
 *
 * @return IOT_ERR_NO_MEM if the queue is full — the caller owns @p arg and must
 *         free it rather than leak it.
 */
iot_err_t IOT_CoreDeferWork(void (*handler)(void *), void *arg);

/**
 * @brief Root device mesh unicast address, as assigned at provisioning.
 */
iot_err_t IOT_CoreGetRootNwkAddr(uint16_t *addr);

/**
 * @brief Root device group address, as assigned at provisioning.
 */
iot_err_t IOT_CoreGetRootGroup(uint16_t *group);

/**
 * @brief Root device ID (12 bytes as provisioned).
 *
 * Copies at most @p outSize bytes and reports how many landed in @p outLen
 * (may be NULL). Truncates rather than failing on a short buffer.
 */
iot_err_t IOT_CoreGetRootDeviceId(uint8_t *outDevId, uint16_t outSize, uint16_t *outLen);

/**
 * @brief Start provisioning mode (BLE advertising + LAN config)
 *
 * Call this to (re-)enter provisioning mode after IOT_CoreInit() has been called.
 * IOT_CoreInit() calls this automatically when the device is not yet provisioned.
 * Safe to call when already provisioning (returns IOT_OK as no-op).
 *
 * @return IOT_OK on success, IOT_ERR_INVALID_STATE if core is not initialized
 */
iot_err_t IOT_CoreStartProvision(void);

/**
 * @brief Stop provisioning mode (stop BLE advertising, tear down config listeners)
 *
 * Safe to call when not provisioning (no-op).
 *
 * @return IOT_OK on success
 */
iot_err_t IOT_CoreStopProvision(void);

/**
 * @brief Check if device is provisioned (non-blocking, cached in RAM).
 * @return true if provisioned, false otherwise
 */
bool IOT_CoreIsProvisioned(void);

/**
 * @brief Get root device Element ID.
 * @param eid Pointer to receive the root EID
 * @return IOT_OK on success, IOT_ERR_DATA_NOT_FOUND if not provisioned
 */
iot_err_t IOT_CoreGetRootEid(uint16_t *eid);

/**
 * @brief Start BLE advertisement in "change WiFi" mode (provisioned device only).
 *
 * Advertises with WILEDIRECT PROV UUID (00007269) and the device's own obfuscated
 * deviceId in manufacturer data, so a paired app can reconnect and update WiFi
 * credentials via SETTING_WIFI_SCAN / SETTING_WIFI_SSID_PWD without a factory reset.
 *
 * Only valid when the device is provisioned.
 *
 * @return IOT_OK on success, IOT_ERR_INVALID_STATE if not provisioned or core not init
 */
iot_err_t IOT_CoreStartChangeWifiBle(void);

/**
 * @brief Stop the change-WiFi BLE advertisement started by IOT_CoreStartChangeWifiBle.
 *
 * Safe to call even if change-WiFi BLE is not currently running (returns IOT_OK).
 * Call this when cloud connectivity is restored so the BLE advertisement is
 * cleaned up and the BLE stack can be released.
 *
 * @return IOT_OK on success
 */
iot_err_t IOT_CoreStopChangeWifiBle(void);

/**
 * @brief Configuration for the change-WiFi service.
 *
 * Pass to IOT_CoreConfigureChangeWifi() BEFORE calling IOT_CoreInit().
 * If not called, the defaults below apply.
 */
typedef struct {
    bool     bleFallbackEnabled;  /**< Auto-start BLE when WiFi is down. Default: true  */
    uint32_t bleFallbackDelayMs;  /**< ms to wait after WiFi drop before BLE. Default: 30000 */
} IOT_CoreChangeWifiConfig_t;

/**
 * @brief Configure the change-WiFi service (optional).
 *
 * Must be called before IOT_CoreInit(). If not called, defaults apply
 * (BLE fallback enabled, 30 s delay).
 *
 * @param config Pointer to config struct. Must not be NULL.
 * @return IOT_OK, IOT_ERR_INVALID_ARG if config is NULL,
 *         IOT_ERR_INVALID_STATE if called after IOT_CoreInit()
 */
iot_err_t IOT_CoreConfigureChangeWifi(const IOT_CoreChangeWifiConfig_t *config);

/**
 * @brief Get boot counter value (BLOCKING).
 * @param count Output: current boot counter value
 * @return IOT_OK on success
 */
iot_err_t IOT_CoreGetBootCounter(uint8_t *count);

/**
 * @brief Set boot counter value (BLOCKING).
 * @param count Boot counter value to store
 * @return IOT_OK on success
 */
iot_err_t IOT_CoreSetBootCounter(uint8_t count);

/**
 * @brief Set boot counter value asynchronously (NON-BLOCKING).
 * Safe to call from timer callbacks.
 * @param count Boot counter value to store
 * @return IOT_OK if queued successfully
 */
iot_err_t IOT_CoreSetBootCounterAsync(uint8_t count);

/* ============================================================================
 *  OTA — the SDK owns the download, iotcore owns the protocol
 * ==========================================================================*/

/**
 * @brief An OTA request iotcore has accepted and is holding for the SDK.
 *
 * Retrieved with IOT_CoreOtaGetPending(), which is the way to act on a request
 * AFTER the dispatch that announced it — typically on the next boot, when the
 * SDK has deliberately rebooted into a stripped-down mode to free contiguous
 * heap for the TLS download. Release with IOT_CoreOtaFreePending().
 *
 * `cert` is re-read from storage on every call, so the SDK never has to persist
 * it (and could not: it lives in an internal config namespace).
 */
typedef struct
{
    char *url;             /**< Full URL including scheme. */
    char *authToken;       /**< Bearer token, or NULL. May have expired if the
                                request sat across a long reboot — a 401 should be
                                reported as a failure so the cloud reissues. */
    uint16_t authTokenLen;
    char *cert;            /**< CA cert PEM, or NULL when none is provisioned. */
    uint16_t certLen;
    char targetVersion[32];/**< Empty until the download reaches the writing stage. */
    uint8_t autoUpdate;
} IOT_CoreOtaRequest_t;

/**
 * @brief Fetch the OTA request iotcore is holding.
 * @param out Filled on success. Free with IOT_CoreOtaFreePending().
 * @return IOT_OK, IOT_ERR_NOT_FOUND when no request is pending,
 *         IOT_ERR_INVALID_ARG when out is NULL.
 */
iot_err_t IOT_CoreOtaGetPending(IOT_CoreOtaRequest_t *out);

/**
 * @brief Release the buffers in a request returned by IOT_CoreOtaGetPending().
 */
void IOT_CoreOtaFreePending(IOT_CoreOtaRequest_t *req);

/**
 * @brief Decline the pending OTA and drop it.
 *
 * Also tells the cloud the device will not update, so the request is answered
 * rather than left hanging. Call this when the SDK decides not to update at all; do
 * NOT call it merely to defer, or the request is lost.
 *
 * @return IOT_OK, or IOT_ERR_NOT_FOUND when nothing was pending.
 */
iot_err_t IOT_CoreOtaClearPending(void);

/**
 * @brief Progress callback that emits every OTA protocol message for you.
 *
 * Hand this straight to the platform downloader and the SDK has no message
 * bookkeeping at all — iotcore maps each platform status to the right
 * notification, records the target version, marks the request applied so the
 * next boot verifies it, and drops the request on failure:
 *
 *     IOT_AppManagerStartUpdateProcess(req.url, req.cert, req.certLen,
 *                                      req.authToken, req.authTokenLen,
 *                                      NULL, IOT_CoreOtaPlatformProgressCb);
 *
 * Sending is best-effort: with MQTT down (an OTA-only boot, say) the durable
 * state is still updated and the outcome is reported after the next boot.
 * There is no "success" call — success is proven by the running version after
 * the reboot, not claimed by the downloader.
 */
void IOT_CoreOtaPlatformProgressCb(uint8_t status, uint8_t progress, void *user_data);

/**
 * @brief Pause/resume the change-WiFi BLE fallback around a download.
 *
 * The fallback would otherwise try to advertise on a BLE stack the SDK has torn
 * down to free heap for the download, which faults. Suspend before releasing
 * BLE; resume when the download ends without a reboot.
 */
iot_err_t IOT_CoreChangeWifiSuspendBle(void);
iot_err_t IOT_CoreChangeWifiResumeBle(void);

/**
 * @brief Tear the BLE stack down and reclaim its heap.
 *
 * Stops advertising first, then deinitialises the stack, so iotcore's own BLE
 * bookkeeping stays consistent — deinitialising the platform stack directly
 * would leave core believing BLE is still up.
 *
 * The usual reason to call this is to free contiguous heap before an OTA
 * download: BLE typically holds tens of KB, and a TLS download needs a single
 * ~16.7 KB contiguous block that a fragmented heap often cannot produce.
 * Suspend the change-WiFi fallback first (IOT_CoreChangeWifiSuspendBle) or a
 * WiFi drop mid-download will try to advertise on the stack you just removed.
 */
iot_err_t IOT_CoreBleDeinit(void);
