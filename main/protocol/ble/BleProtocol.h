#pragma once

#include <stdint.h>
#include <vector>
#include <atomic>
#include <functional>
#include "esp_ble_mesh_defs.h"

#define SYSTEM_REQ 0xFFE9
#define APP_REQ 0xFFE8
#define RAL_MAGIC 0x0428

enum
{
	// send cmd part
	HCI_GATEWAY_CMD_START = 0x00,
	HCI_GATEWAY_CMD_STOP = 0x01,
	HCI_GATEWAY_CMD_RESET = 0x02,
	HCI_GATEWAY_CMD_CLEAR_NODE_INFO = 0x06,
	HCI_GATEWAY_CMD_SET_ADV_FILTER = 0x08,
	HCI_GATEWAY_CMD_SET_PRO_PARA = 0x09,
	HCI_GATEWAY_CMD_SET_NODE_PARA = 0x0a,
	HCI_GATEWAY_CMD_START_KEYBIND = 0x0b,
	HCI_GATEWAY_CMD_GET_PRO_SELF_STS = 0x0c,
	HCI_GATEWAY_CMD_SET_DEV_KEY = 0x0d,
	HCI_GATEWAY_CMD_GET_SNO = 0x0e,
	HCI_GATEWAY_CMD_SET_SNO = 0x0f,
	HCI_GATEWAY_CMD_GET_UUID_MAC = 0x10,
	HCI_GATEWAY_CMD_DEL_VC_NODE_INFO = 0x11,
	HCI_GATEWAY_CMD_SEND_VC_NODE_INFO = 0x12,

	// rsp cmd part
	HCI_GATEWAY_RSP_UNICAST = 0x80,
	HCI_GATEWAY_RSP_OP_CODE = 0X81,
	HCI_GATEWAY_KEY_BIND_RSP = 0x82,
	HCI_GATEWAY_CMD_STATIC_OOB_RSP = 0x87, // HCI send back the static oob information
	HCI_GATEWAY_CMD_UPDATE_MAC = 0x88,
	HCI_GATEWAY_CMD_PROVISION_EVT = 0x89,
	HCI_GATEWAY_CMD_KEY_BIND_EVT = 0x8a,
	HCI_GATEWAY_CMD_PRO_STS_RSP = 0x8b,
	HCI_GATEWAY_CMD_SEND_ELE_CNT = 0x8c,
	HCI_GATEWAY_CMD_SEND_NODE_INFO = 0x8d,
	HCI_GATEWAY_CMD_SEND_CPS_INFO = 0x8e,
	HCI_GATEWAY_CMD_HEARTBEAT = 0x8f,
	HCI_GATEWAY_CMD_SEND_MESH_OTA_STS = 0x98,
	HCI_GATEWAY_CMD_SEND_UUID = 0x99,
	HCI_GATEWAY_CMD_SEND_IVI = 0x9a,
	HCI_GATEWAY_CMD_SEND_SRC_CMD = 0x9c,
	HCI_GATEWAY_CMD_SEND_SNO_RSP = 0xa0,
	HCI_GATEWAY_CMD_SEND = 0xb1,
	HCI_GATEWAY_DEV_RSP = 0xb2,
	HCI_GATEWAY_CMD_LINK_OPEN = 0xb3,
	HCI_GATEWAY_CMD_LINK_CLS = 0xb4,
	HCI_GATEWAY_CMD_SEND_BACK_VC = 0xb5,
	HCI_GATEWAY_CMD_LOG_STRING = 0xb6,
	HCI_GATEWAY_CMD_LOG_BUF = 0xb7,
};

#define CONNECT_DEVICE_TIMEOUT 40 // seconds

#define DEVICE_SIG_MODEL_MAX 32
#define DEVICE_VENDOR_MODEL_MAX 16

using namespace std;

typedef struct
{
	int16_t cid;
	int16_t pid;
	int16_t vid;
	int16_t crpl;
	int16_t features;

	int16_t all_models;
	uint8_t sig_models;
	uint8_t vnd_models;
} composition_head_t;

typedef struct
{
	uint16_t model_id;
	uint8_t is_binded;
} sig_model_t;

typedef struct
{
	uint16_t model_id;
	uint16_t company_id;
	uint8_t is_binded;
} vender_model_t;

typedef struct
{
	uint16_t loc;

	// reserve space for up to 20 SIG models
	sig_model_t sig_models[DEVICE_SIG_MODEL_MAX];
	uint8_t nums;

	// reserve space for up to 4 vendor models
	vender_model_t vendor_models[DEVICE_VENDOR_MODEL_MAX];
	uint8_t numv;
} elenment_t;

typedef struct
{
	uint8_t mac[BD_ADDR_LEN];
	uint8_t uuid[16];
	uint16_t unicast;
	uint8_t elem_num;

	uint16_t cid;
	uint16_t pid;
	uint16_t vid;
	uint16_t crpl;
	uint16_t feat;
	elenment_t elements[6];
} scan_device_t;

class BleProtocol
{
private:
	typedef struct
	{
		uint16_t opcode;
		uint8_t data[100];
	} message_req_st;

	typedef struct
	{
		uint16_t len;
		uint8_t magic;
		uint8_t opcode;
		uint8_t data[];
	} message_rsp_st;

	typedef struct
	{
		bool status;
		uint8_t opcode;
		int *len;
		uint8_t *data;
		uint8_t *compare_data;
		int compare_position;
		int compare_len;
	} message_rsp_list_st;

	typedef struct
	{
		uint8_t uuid[8];
		uint8_t deviceType[4];
		uint16_t fwVersion;
		uint16_t magic;
	} uuid_t;

	typedef struct
	{
		uint8_t mac[6];
		uint8_t len;
		uint8_t header_type;
		uint8_t beacon_type;
		uint8_t uuid[16];
		uint8_t uri_hash[4];
		uint8_t obb_info[2];
		int8_t rssi;
		uint8_t dc[2];
	} scan_device_message_t;

	// typedef function<void(scan_device_message_t *scan_device_message)> AddDeviceFunc;
	// AddDeviceFunc addDeviceFunc;
	scan_device_message_t scanDeviceMessage;

	vector<message_rsp_list_st *> messageRespList;

	// TODO: Add init state
	uint8_t netKey[16];
	uint8_t appKey[16];
	uint8_t gwKey[16];
	uint8_t deviceKey[16];
	uint16_t nextAddr;

	string uuidToStr(uuid_t *uuid);
	string arrayToString844412(uint8_t *array);

	void CheckOpcodeException(message_rsp_st *message);
	int OnMessage(unsigned char *data, int len);
	int SendMessage(uint16_t opReq, uint8_t *dataReq, int lenReq, uint8_t opRsp, uint8_t *dataRsp, int *lenRsp, uint32_t timeout, uint8_t *compare_data = 0, int compare_position = 0, int compare_len = 0);

public:
	BleProtocol();
	virtual ~BleProtocol();

	atomic<bool> isAdding;
	atomic<bool> isProvisioning;

	void addScanDevice(scan_device_t *scan_device);
	void init();
	int GetAppKey();
	int GetNetKey();
	int SetNetKey();
	int SetGwKey();

	int StartScan();
	int StopScan();
	int ResetFactory();

	bool AddDevice(scan_device_message_t *scan_device_message);
	int SelectMac(uint8_t *mac);
	int Provision(uint16_t deviceAddr);
	int BindingAll();
	int SetGwAddr(uint16_t devAddr, uint16_t gwAddr = 0x0002);
	int GetDeviceType(uint8_t *mac, uint16_t devAddr, uint32_t &deviceType, uint16_t &deviceVersion);

	int ResetDev(uint16_t devAddr);

	int SetOnOffLight(uint16_t devAddr, uint8_t onoff, uint16_t transition, bool ack);
	int GetOnoffLight(uint16_t devAddr);
	int SetDimmingLight(uint16_t devAddr, uint16_t dim, uint16_t transition, bool ack);
	int GetDimming(uint16_t devAddr);
	int SetCctLight(uint16_t devAddr, uint16_t cct, uint16_t transition, bool ack);
	int GetCct(uint16_t devAddr);
	int SetHSLLight(uint16_t devAddr, uint16_t H, uint16_t S, uint16_t L, uint16_t transition, bool ack);
	int GetHSL(uint16_t devAddr);
	int SetCctDimLight(uint16_t devAddr, uint16_t cct, uint16_t dim, uint16_t transition, bool ack);
	int GetCctDimLight(uint16_t devAddr);

	// group light
	int AddDev2Group(uint16_t devAddr, uint16_t element, uint16_t group);
	int DelDev2Group(uint16_t devAddr, uint16_t element, uint16_t group);

	/**
	 * @brief
	 *
	 * @param devAddr id device
	 * @param scene id scene
	 * @param modeRgb 0 normal scene, 1->6 id mode blink RGB light
	 * @return int 0 success, -1 error
	 */
	int SetSceneLights(uint16_t devAddr, uint16_t scene, uint8_t modeRgb);

	/**
	 * @brief
	 *
	 * @param devAddr id device
	 * @param scene id scene
	 * @return int 0 success, -1 error
	 */
	int DelSceneLights(uint16_t devAddr, uint16_t scene);
	int CallScene(uint16_t devAddr, uint16_t scene, uint16_t transition, bool ack, int delayTime);
	int CallModeRgb(uint16_t devAddr, uint8_t modeRgb);

	// update status lights
	int UpdateLights(uint16_t devAddr);
};

extern BleProtocol *bleProtocol;
