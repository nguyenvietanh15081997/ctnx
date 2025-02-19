#include "BleProtocol.h"
#include <stdlib.h>
#include <thread>
#include <functional>
#include <byteswap.h>
#include "Log.h"
#include <Util.h>
#include <string.h>
#include <algorithm>
#include <Db.h>
#include "BleOpCode.h"
#include "DeviceBle.h"
#include "AES.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "esp_ble.h"

extern "C" esp_ble_mesh_key_t prov_key;
extern "C" esp_ble_mesh_client_t config_client;
// extern "C" esp_ble_mesh_client_t onoff_client;

extern "C" esp_err_t bluetooth_init(void);
extern "C" esp_err_t ble_mesh_init(void);

BleProtocol *bleProtocol = NULL;

static uint8_t keyAes[] = {0x44, 0x69, 0x67, 0x69, 0x74, 0x61, 0x6c, 0x40, 0x32, 0x38, 0x31, 0x31, 0x32, 0x38, 0x30, 0x34};
static uint8_t plaintext[] = {0x24, 0x02, 0x28, 0x04, 0x28, 0x11, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// static uint8_t appKey[] = {0x60, 0x96, 0x47, 0x71, 0x73, 0x4f, 0xbd, 0x76, 0xe3, 0xb4, 0x05, 0x19, 0xd1, 0xd9, 0x4a, 0x48};

static scan_device_t scan_device;
static uint16_t opcode_binding = 0;

BleProtocol::BleProtocol()
{
	nextAddr = 0;
	isAdding = false;
}

BleProtocol::~BleProtocol()
{
	isAdding = false;
}

static esp_err_t example_ble_mesh_set_msg_common(esp_ble_mesh_client_common_param_t *common,
																								 uint16_t unicast,
																								 esp_ble_mesh_model_t *model, uint32_t opcode)
{
	if (!common || !unicast || !model)
	{
		return ESP_ERR_INVALID_ARG;
	}

	common->opcode = opcode;
	common->model = model;
	common->ctx.net_idx = prov_key.net_idx;
	common->ctx.app_idx = prov_key.app_idx;
	common->ctx.addr = unicast;
	common->ctx.send_ttl = MSG_SEND_TTL;
	common->ctx.send_rel = MSG_SEND_REL;
	common->msg_timeout = MSG_TIMEOUT;
	common->msg_role = MSG_ROLE;

	return ESP_OK;
}

static esp_err_t prov_complete(int node_idx, const esp_ble_mesh_octet16_t uuid,
															 uint16_t unicast, uint8_t elem_num, uint16_t net_idx)
{
	esp_ble_mesh_client_common_param_t common = {0};
	esp_ble_mesh_cfg_client_get_state_t get_state = {0};
	char name[11] = {0};
	int err;

	LOGI("node index: 0x%x, unicast address: 0x%02x, element num: %d, netkey index: 0x%02x",
					 node_idx, unicast, elem_num, net_idx);
	LOGI("device uuid: %s", bt_hex(uuid, 16));

	sprintf(name, "%s%d", "NODE-", node_idx);
	err = esp_ble_mesh_provisioner_set_node_name(node_idx, name);
	if (err)
	{
		LOGE("%s: Set node name failed", __func__);
		return ESP_FAIL;
	}

	memcpy(scan_device.uuid, uuid, 16);
	scan_device.unicast = unicast;
	scan_device.elem_num = elem_num;

	example_ble_mesh_set_msg_common(&common, unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET);
	get_state.comp_data_get.page = COMP_DATA_PAGE_0;
	err = esp_ble_mesh_config_client_get_state(&common, &get_state);
	if (err)
	{
		LOGE("%s: Send config comp data get failed", __func__);
		return ESP_FAIL;
	}

	return ESP_OK;
}

/**
 * @brief binding all
 *
 * @return true binding all done
 * @return false waiting binding callback
 */
static bool binding_all()
{
	int err;
	for (int i = 0; i < scan_device.elem_num; i++)
	{
		for (int j = 0; j < scan_device.elements[i].nums; j++)
		{
			if (!scan_device.elements[i].sig_models[j].is_binded)
			{
				opcode_binding = scan_device.elements[i].sig_models[j].model_id;
				esp_ble_mesh_client_common_param_t common = {0};
				esp_ble_mesh_cfg_client_set_state_t set_state = {0};
				scan_device.elements[i].sig_models[j].is_binded = 1;
				LOGI("Binding Element %d SIG Model ID 0x%04x", i, opcode_binding);
				example_ble_mesh_set_msg_common(&common, scan_device.unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);
				set_state.model_app_bind.element_addr = scan_device.unicast + i;
				set_state.model_app_bind.model_app_idx = prov_key.app_idx;
				set_state.model_app_bind.model_id = opcode_binding;
				set_state.model_app_bind.company_id = ESP_BLE_MESH_CID_NVAL;
				err = esp_ble_mesh_config_client_set_state(&common, &set_state);
				if (err)
				{
					LOGE("%s: Config Model App Bind failed", __func__);
				}
				return false;
			}
		}
		for (int j = 0; j < scan_device.elements[i].numv; j++)
		{
			if (!scan_device.elements[i].vendor_models[j].is_binded)
			{
				opcode_binding = scan_device.elements[i].vendor_models[j].model_id;
				esp_ble_mesh_client_common_param_t common = {0};
				esp_ble_mesh_cfg_client_set_state_t set_state = {0};
				scan_device.elements[i].vendor_models[j].is_binded = 1;
				LOGI("Binding Element %d Vendor Model ID 0x%04x", i, opcode_binding);
				example_ble_mesh_set_msg_common(&common, scan_device.unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);
				set_state.model_app_bind.element_addr = scan_device.unicast + i;
				set_state.model_app_bind.model_app_idx = prov_key.app_idx;
				set_state.model_app_bind.model_id = opcode_binding;
				set_state.model_app_bind.company_id = CID_RD;
				err = esp_ble_mesh_config_client_set_state(&common, &set_state);
				if (err)
				{
					LOGE("%s: Config Model App Bind failed", __func__);
				}
				return false;
			}
		}
	}
	return true;
}

static void print_device()
{
	LOGI("********************** Print Device Info Start **********************");
	LOGI("unicast address: 0x%02x, element num: %d", scan_device.unicast, scan_device.elem_num);
	LOGI("device uuid: %s, mac: %s", bt_hex(scan_device.uuid, 16), bt_hex(scan_device.mac, BD_ADDR_LEN));
	LOGI("* CID 0x%04x, PID 0x%04x, VID 0x%04x, CRPL 0x%04x, Features 0x%04x *", scan_device.cid, scan_device.pid, scan_device.vid, scan_device.crpl, scan_device.feat);
	for (int i = 0; i < scan_device.elem_num; i++)
	{
		LOGI("* Loc 0x%04x, NumS 0x%02x, NumV 0x%02x *", scan_device.elements[i].loc, scan_device.elements[i].nums, scan_device.elements[i].numv);
		for (int j = 0; j < scan_device.elements[i].nums; j++)
		{
			LOGI("* SIG Model ID 0x%04x *", scan_device.elements[i].sig_models[j].model_id);
		}
		for (int j = 0; j < scan_device.elements[i].numv; j++)
		{
			LOGI("* Vendor Model ID 0x%04x, Company ID 0x%04x *", scan_device.elements[i].vendor_models[j].model_id, scan_device.elements[i].vendor_models[j].company_id);
		}
	}
	LOGI("*********************** Print Device Info End ***********************");
}

static void prov_link_open(esp_ble_mesh_prov_bearer_t bearer)
{
	LOGI("%s link open", bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
}

static void prov_link_close(esp_ble_mesh_prov_bearer_t bearer, uint8_t reason)
{
	LOGI("%s link close, reason 0x%02x",
					 bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT", reason);
}

static void recv_unprov_adv_pkt(uint8_t dev_uuid[16], uint8_t addr[BD_ADDR_LEN],
																esp_ble_mesh_addr_type_t addr_type, uint16_t oob_info,
																uint8_t adv_type, esp_ble_mesh_prov_bearer_t bearer)
{
	esp_ble_mesh_unprov_dev_add_t add_dev = {0};
	int err;

	/* Due to the API esp_ble_mesh_provisioner_set_dev_uuid_match, Provisioner will only
	 * use this callback to report the devices, whose device UUID starts with 0xdd & 0xdd,
	 * to the application layer.
	 */

	LOGI("address: %s, address type: %d, adv type: %d", bt_hex(addr, BD_ADDR_LEN), addr_type, adv_type);
	LOGI("device uuid: %s", bt_hex(dev_uuid, 16));
	LOGI("oob info: %d, bearer: %s", oob_info, (bearer & ESP_BLE_MESH_PROV_ADV) ? "PB-ADV" : "PB-GATT");

	memset(&scan_device, 0, sizeof(scan_device));
	memcpy(scan_device.mac, addr, BD_ADDR_LEN);
	memcpy(add_dev.addr, addr, BD_ADDR_LEN);
	add_dev.addr_type = (uint8_t)addr_type;
	memcpy(add_dev.uuid, dev_uuid, 16);
	add_dev.oob_info = oob_info;
	add_dev.bearer = (esp_ble_mesh_prov_bearer_t)bearer;
	/* Note: If unprovisioned device adv packets have not been received, we should not add
					 device with ADD_DEV_START_PROV_NOW_FLAG set. */
	err = esp_ble_mesh_provisioner_add_unprov_dev(&add_dev,
																								ADD_DEV_RM_AFTER_PROV_FLAG | ADD_DEV_START_PROV_NOW_FLAG | ADD_DEV_FLUSHABLE_DEV_FLAG);
	if (err)
	{
		LOGE("%s: Add unprovisioned device into queue failed", __func__);
	}

	return;
}

static void provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
														esp_ble_mesh_prov_cb_param_t *param)
{
	switch (event)
	{
	case ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT:
		LOGI("ESP_BLE_MESH_PROVISIONER_PROV_ENABLE_COMP_EVT, err_code %d", param->provisioner_prov_enable_comp.err_code);
		break;
	case ESP_BLE_MESH_PROVISIONER_PROV_DISABLE_COMP_EVT:
		LOGI("ESP_BLE_MESH_PROVISIONER_PROV_DISABLE_COMP_EVT, err_code %d", param->provisioner_prov_disable_comp.err_code);
		break;
	case ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT:
		LOGI("ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT");
		recv_unprov_adv_pkt(param->provisioner_recv_unprov_adv_pkt.dev_uuid, param->provisioner_recv_unprov_adv_pkt.addr,
												param->provisioner_recv_unprov_adv_pkt.addr_type, param->provisioner_recv_unprov_adv_pkt.oob_info,
												param->provisioner_recv_unprov_adv_pkt.adv_type, param->provisioner_recv_unprov_adv_pkt.bearer);
		break;
	case ESP_BLE_MESH_PROVISIONER_PROV_LINK_OPEN_EVT:
		prov_link_open(param->provisioner_prov_link_open.bearer);
		break;
	case ESP_BLE_MESH_PROVISIONER_PROV_LINK_CLOSE_EVT:
		prov_link_close(param->provisioner_prov_link_close.bearer, param->provisioner_prov_link_close.reason);
		break;
	case ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT:
		prov_complete(param->provisioner_prov_complete.node_idx, param->provisioner_prov_complete.device_uuid,
									param->provisioner_prov_complete.unicast_addr, param->provisioner_prov_complete.element_num,
									param->provisioner_prov_complete.netkey_idx);
		break;
	case ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT:
		LOGI("ESP_BLE_MESH_PROVISIONER_ADD_UNPROV_DEV_COMP_EVT, err_code %d", param->provisioner_add_unprov_dev_comp.err_code);
		break;
	case ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT:
		LOGI("ESP_BLE_MESH_PROVISIONER_SET_DEV_UUID_MATCH_COMP_EVT, err_code %d", param->provisioner_set_dev_uuid_match_comp.err_code);
		break;
	case ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT:
	{
		LOGI("ESP_BLE_MESH_PROVISIONER_SET_NODE_NAME_COMP_EVT, err_code %d", param->provisioner_set_node_name_comp.err_code);
		if (param->provisioner_set_node_name_comp.err_code == ESP_OK)
		{
			const char *name = NULL;
			name = esp_ble_mesh_provisioner_get_node_name(param->provisioner_set_node_name_comp.node_index);
			if (!name)
			{
				LOGE("Get node name failed");
				return;
			}
			LOGI("Node %d name is: %s", param->provisioner_set_node_name_comp.node_index, name);
		}
		break;
	}
	case ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT:
	{
		LOGI("ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT, err_code %d", param->provisioner_add_app_key_comp.err_code);
		if (param->provisioner_add_app_key_comp.err_code == ESP_OK)
		{
			esp_err_t err = 0;
			prov_key.app_idx = param->provisioner_add_app_key_comp.app_idx;
			err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(PROV_OWN_ADDR, prov_key.app_idx,
																																 ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI, ESP_BLE_MESH_CID_NVAL);
			if (err != ESP_OK)
			{
				LOGE("Provisioner bind local model appkey failed");
				return;
			}
		}
		break;
	}
	case ESP_BLE_MESH_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT:
		LOGI("ESP_BLE_MESH_PROVISIONER_BIND_APP_KEY_TO_MODEL_COMP_EVT, err_code %d", param->provisioner_bind_app_key_to_model_comp.err_code);
		break;
	default:
		break;
	}

	return;
}

static void example_ble_mesh_parse_node_comp_data(const uint8_t *data, uint16_t length)
{
	uint16_t offset;
	int i = 0, j;

	scan_device.cid = COMP_DATA_2_OCTET(data, 0);
	scan_device.pid = COMP_DATA_2_OCTET(data, 2);
	scan_device.vid = COMP_DATA_2_OCTET(data, 4);
	scan_device.crpl = COMP_DATA_2_OCTET(data, 6);
	scan_device.feat = COMP_DATA_2_OCTET(data, 8);
	offset = 10;

	for (; offset < length;)
	{
		scan_device.elements[i].loc = COMP_DATA_2_OCTET(data, offset);
		scan_device.elements[i].nums = COMP_DATA_1_OCTET(data, offset + 2);
		scan_device.elements[i].numv = COMP_DATA_1_OCTET(data, offset + 3);
		offset += 4;
		for (j = 0; j < scan_device.elements[i].nums; j++)
		{
			scan_device.elements[i].sig_models[j].model_id = COMP_DATA_2_OCTET(data, offset);
			offset += 2;
		}
		for (j = 0; j < scan_device.elements[i].numv; j++)
		{
			scan_device.elements[i].vendor_models[j].company_id = COMP_DATA_2_OCTET(data, offset);
			scan_device.elements[i].vendor_models[j].model_id = COMP_DATA_2_OCTET(data, offset + 2);
			offset += 4;
		}
		i++;
	}
}

static void config_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
														 esp_ble_mesh_cfg_client_cb_param_t *param)
{
	esp_ble_mesh_client_common_param_t common = {0};
	uint32_t opcode;
	uint16_t addr;
	int err;

	opcode = param->params->opcode;
	addr = param->params->ctx.addr;
	uint8_t e_index = param->params->model->element_idx;

	LOGI("%s, e_index: %d", __func__, e_index);

	LOGI("%s, error_code = 0x%02x, event = 0x%02x, addr: 0x%04x, opcode: 0x%04x",
					 __func__, param->error_code, event, param->params->ctx.addr, opcode);

	if (param->error_code)
	{
		LOGE("Send config client message failed, opcode 0x%04x", opcode);
		return;
	}

	if (scan_device.unicast != addr)
	{
		LOGE("%s: Device addr not match with scanning device", __func__);
		return;
	}

	switch (event)
	{
	case ESP_BLE_MESH_CFG_CLIENT_GET_STATE_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET:
		{
			LOGI("thinpv ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET");
			LOGI("composition data %s", bt_hex(param->status_cb.comp_data_status.composition_data->data, param->status_cb.comp_data_status.composition_data->len));
			example_ble_mesh_parse_node_comp_data(param->status_cb.comp_data_status.composition_data->data, param->status_cb.comp_data_status.composition_data->len);
			print_device();
			esp_ble_mesh_cfg_client_set_state_t set_state = {0};
			example_ble_mesh_set_msg_common(&common, scan_device.unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD);
			set_state.app_key_add.net_idx = prov_key.net_idx;
			set_state.app_key_add.app_idx = prov_key.app_idx;
			memcpy(set_state.app_key_add.app_key, prov_key.app_key, 16);
			err = esp_ble_mesh_config_client_set_state(&common, &set_state);
			if (err)
			{
				LOGE("%s: Config AppKey Add failed", __func__);
				return;
			}
			break;
		}
		default:
			break;
		}
		break;
	case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
		{
			LOGI("thinpv ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
			if (binding_all())
			{
				LOGI("Binding all done");
			}
			break;
		}
		case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
		{
			LOGI("thinpv ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND: %d - %d", scan_device.unicast, scan_device.elem_num);
			if (binding_all())
			{
				LOGI("Binding all done");
				bleProtocol->addScanDevice(&scan_device);
			}
			break;
		}
		default:
			break;
		}
		break;
	case ESP_BLE_MESH_CFG_CLIENT_PUBLISH_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_STATUS:
			ESP_LOG_BUFFER_HEX("composition data %s", param->status_cb.comp_data_status.composition_data->data,
												 param->status_cb.comp_data_status.composition_data->len);
			break;
		case ESP_BLE_MESH_MODEL_OP_APP_KEY_STATUS:
			break;
		default:
			break;
		}
		break;
	case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET:
		{
			LOGI("thinpv timeout ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET");
			esp_ble_mesh_cfg_client_get_state_t get_state = {0};
			example_ble_mesh_set_msg_common(&common, scan_device.unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET);
			get_state.comp_data_get.page = COMP_DATA_PAGE_0;
			err = esp_ble_mesh_config_client_get_state(&common, &get_state);
			if (err)
			{
				LOGE("%s: Config Composition Data Get failed", __func__);
				return;
			}
			break;
		}
		case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
		{
			LOGI("thinpv timeout ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
			esp_ble_mesh_cfg_client_set_state_t set_state = {0};
			example_ble_mesh_set_msg_common(&common, scan_device.unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD);
			set_state.app_key_add.net_idx = prov_key.net_idx;
			set_state.app_key_add.app_idx = prov_key.app_idx;
			memcpy(set_state.app_key_add.app_key, prov_key.app_key, 16);
			err = esp_ble_mesh_config_client_set_state(&common, &set_state);
			if (err)
			{
				LOGE("%s: Config AppKey Add failed", __func__);
				return;
			}
			break;
		}
		case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
		{
			LOGI("thinpv timeout ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
			esp_ble_mesh_cfg_client_set_state_t set_state = {0};
			example_ble_mesh_set_msg_common(&common, scan_device.unicast, config_client.model, ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND);
			set_state.model_app_bind.element_addr = scan_device.unicast;
			set_state.model_app_bind.model_app_idx = prov_key.app_idx;
			set_state.model_app_bind.model_id = opcode_binding;
			set_state.model_app_bind.company_id = ESP_BLE_MESH_CID_NVAL;
			err = esp_ble_mesh_config_client_set_state(&common, &set_state);
			if (err)
			{
				LOGE("%s: Config Model App Bind failed", __func__);
				return;
			}
			break;
		}
		default:
			break;
		}
		break;
	default:
		LOGE("Not a config client status message event");
		break;
	}
}

static void generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
															esp_ble_mesh_generic_client_cb_param_t *param)
{
	esp_ble_mesh_client_common_param_t common = {0};
	uint32_t opcode;
	uint16_t addr;
	int err;

	opcode = param->params->opcode;
	addr = param->params->ctx.addr;

	LOGI("%s, error_code = 0x%02x, event = 0x%02x, addr: 0x%04x, opcode: 0x%04x",
					 __func__, param->error_code, event, param->params->ctx.addr, opcode);

	if (param->error_code)
	{
		LOGE("Send generic client message failed, opcode 0x%04x", opcode);
		return;
	}

	DeviceBle *deviceBle = gateway->getDeviceBleFromAddr(addr);
	if (!deviceBle)
	{
		LOGE("%s: Get deviceBle failed", __func__);
		return;
	}

	switch (event)
	{
	case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
		{
			// esp_ble_mesh_generic_client_set_state_t set_state = {0};
			// node->onoff = param->status_cb.onoff_status.present_onoff;
			// LOGI("ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET onoff: 0x%02x", node->onoff);
			// /* After Generic OnOff Status for Generic OnOff Get is received, Generic OnOff Set will be sent */
			// example_ble_mesh_set_msg_common(&common, node->unicast, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET);
			// set_state.onoff_set.op_en = false;
			// set_state.onoff_set.onoff = !node->onoff;
			// set_state.onoff_set.tid = 0;
			// err = esp_ble_mesh_generic_client_set_state(&common, &set_state);
			// if (err)
			// {
			// 	LOGE("%s: Generic OnOff Set failed", __func__);
			// 	return;
			// }
			break;
		}
		default:
			break;
		}
		break;
	case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
			// node->onoff = param->status_cb.onoff_status.present_onoff;
			// LOGI("ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET onoff: 0x%02x", node->onoff);
			break;
		default:
			break;
		}
		break;
	case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS:
			// node->onoff = param->status_cb.onoff_status.present_onoff;
			LOGI("ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS onoff: 0x%02x", param->status_cb.onoff_status.present_onoff);
			LOGD("Device: %s", deviceBle->GetMac().c_str());
			break;
		default:
			break;
		}
		break;
	case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
		/* If failed to receive the responses, these messages will be resend */
		switch (opcode)
		{
		case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
		{
			// esp_ble_mesh_generic_client_get_state_t get_state = {0};
			// example_ble_mesh_set_msg_common(&common, node->unicast, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET);
			// err = esp_ble_mesh_generic_client_get_state(&common, &get_state);
			// if (err)
			// {
			// 	LOGE("%s: Generic OnOff Get failed", __func__);
			// 	return;
			// }
			break;
		}
		case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
		{
			// esp_ble_mesh_generic_client_set_state_t set_state = {0};
			// node->onoff = param->status_cb.onoff_status.present_onoff;
			// LOGI("ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET onoff: 0x%02x", node->onoff);
			// example_ble_mesh_set_msg_common(&common, node->unicast, onoff_client.model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET);
			// set_state.onoff_set.op_en = false;
			// set_state.onoff_set.onoff = !node->onoff;
			// set_state.onoff_set.tid = 0;
			// err = esp_ble_mesh_generic_client_set_state(&common, &set_state);
			// if (err)
			// {
			// 	LOGE("%s: Generic OnOff Set failed", __func__);
			// 	return;
			// }
			break;
		}
		default:
			break;
		}
		break;
	default:
		LOGE("Not a generic client status message event");
		break;
	}
}

static void sensor_client_cb(esp_ble_mesh_sensor_client_cb_event_t event,
														 esp_ble_mesh_sensor_client_cb_param_t *param)
{
	esp_ble_mesh_node_t *node = NULL;

	LOGI("Sensor client, event %u, addr 0x%04x", event, param->params->ctx.addr);
}

void BleProtocol::addScanDevice(scan_device_t *scan_device)
{
	string mac = Util::ConvertU32ToHexString(scan_device->mac, sizeof(scan_device->mac));
	Device *device = gateway->getDevice(mac);
	if (device)
	{
		LOGI("Update device");
		device->SetAddr(scan_device->unicast);
		database->DeviceUpdate(device);
	}
	else
	{
		// TODO: get device type and version
		uint16_t version = 0;
		uint32_t deviceType = BLE_SWITCH_4;
		device = gateway->AddNewDevice(string(bt_hex(scan_device->uuid, 16)), Device::ConvertDeviceTypeToName(deviceType), mac, arrayToString844412((uint8_t *)deviceKey), scan_device->unicast, deviceType, version, true, true);
		if (device)
		{
			gateway->AddDeviceToScanList(device);
		}
		else
		{
			ResetDev(nextAddr);
		}
	}
}

void BleProtocol::init()
{
	// addDeviceFunc = bind(&BleProtocol::AddDevice, this, placeholders::_1);
	// while (GetNetKey())
	// {
	// 	sleep(5);
	// }
	// GetAppKey();
	// database->GatewayRead();

	ESP_ERROR_CHECK(bluetooth_init());

	esp_ble_mesh_register_prov_callback(provisioning_cb);
	esp_ble_mesh_register_config_client_callback(config_client_cb);
	esp_ble_mesh_register_generic_client_callback(generic_client_cb);
	esp_ble_mesh_register_sensor_client_callback(sensor_client_cb);

	ble_mesh_init();
}

void BleProtocol::CheckOpcodeException(message_rsp_st *message_rsp)
{
	// LOGD("CheckOpcodeException");
	switch (message_rsp->opcode)
	{
	case HCI_GATEWAY_CMD_UPDATE_MAC:
		if (isAdding)
		{
			isAdding = false;
			memcpy(&scanDeviceMessage, message_rsp->data, sizeof(scan_device_message_t));
			thread addDeviceThread(addDeviceFunc, &scanDeviceMessage);
			addDeviceThread.detach();
		}
		break;

	case HCI_GATEWAY_RSP_OP_CODE:
	{
		typedef struct
		{
			uint16_t dev_addr;
			uint16_t gw_addr;
			uint8_t data[100];
		} data_message_t;
		data_message_t *data_message = (data_message_t *)message_rsp->data;
		LOGD("Device addr 0x%04X", data_message->dev_addr);
		DeviceBle *deviceBle = gateway->getDeviceBleFromAddr(data_message->dev_addr);
		if (deviceBle)
		{
			LOGD("Have device mac 0x%s type: 0x%08X", deviceBle->GetMac().c_str(), deviceBle->GetType());
			deviceBle->DeviceInputData(data_message->data, message_rsp->len - 6, data_message->dev_addr);
		}
		else
		{
			LOGW("Not found device addr: 0x%04X", data_message->dev_addr);
		}
		break;
	}
	case HCI_GATEWAY_CMD_SEND_NODE_INFO:
	{
		for (int i = 0; i < 16; i++)
		{
			deviceKey[i] = message_rsp->data[i + 4];
		}
		break;
	}

	default:
		break;
	}
}

int BleProtocol::OnMessage(unsigned char *data, int len)
{
	LOGD("OnMessage len: %d", len);
	uint8_t *d = data;
	int l = len;
	message_rsp_st *message_rsp = NULL;
	message_rsp_st *old_message_rsp = NULL;
	bool is_dupplicate = false;
	bool match;
	Util::LedBle(false);
	Util::LedServiceLock();
	while (l >= 4)
	{
		message_rsp = (message_rsp_st *)d;
		is_dupplicate = false;
		if (old_message_rsp && message_rsp->len == old_message_rsp->len)
		{
			is_dupplicate = true;
			for (int i = 0; i < message_rsp->len; i++)
			{
				if (message_rsp->data[i] != old_message_rsp->data[i])
				{
					is_dupplicate = false;
					break;
				}
			}
		}
		if (!is_dupplicate)
		{
			if (message_rsp->len >= 2 && message_rsp->len <= l - 2)
			{
				LOGD("onMessage opcode: 0x%02X, len: %d", message_rsp->opcode, message_rsp->len);
				for (auto &messageResp : messageRespList)
				{
					if (message_rsp->opcode == messageResp->opcode)
					{
						match = true;
						if (messageResp->compare_data)
						{
							for (int i = 0; i < messageResp->compare_len; i++)
							{
								if (message_rsp->data[messageResp->compare_position + i] != messageResp->compare_data[i])
									match = false;
							}
						}
						if (match)
						{
							messageResp->status = true;
							if (messageResp->len)
							{
								*(messageResp->len) = message_rsp->len - 2;
								if (messageResp->data)
									memcpy(messageResp->data, message_rsp->data, *messageResp->len);
							}
						}
					}
				}
				CheckOpcodeException(message_rsp);
			}
			else
			{
				break;
			}
		}
		else
		{
			// LOGW("dupplicate");
		}
		old_message_rsp = message_rsp;
		l -= message_rsp->len + 2;
		d += message_rsp->len + 2;
	}
	Util::LedBle(true);
	Util::LedServiceUnlock();
	return l;
}

int BleProtocol::SendMessage(uint16_t opReq, uint8_t *dataReq, int lenReq, uint8_t opRsp, uint8_t *dataRsp, int *lenRsp, uint32_t timeout, uint8_t *compare_data, int compare_position, int compare_len)
{
	int rs = 0;
	message_rsp_list_st message_rsp_list = {
			.status = false,
			.opcode = opRsp,
			.len = lenRsp,
			.data = dataRsp,
			.compare_data = compare_data,
			.compare_position = compare_position,
			.compare_len = compare_len};
	if (opRsp)
	{
		messageRespList.push_back(&message_rsp_list);
	}

	// message_req_st message_req = {
	// 		.opcode = opReq};
	// for (int i = 0; i < lenReq; i++)
	// {
	// 	message_req.data[i] = dataReq[i];
	// }

	// Write((uint8_t *)&message_req, lenReq + 2);

	if (opRsp)
	{
		while (!message_rsp_list.status && timeout--)
		{
			usleep(1000);
		}
		if (message_rsp_list.status)
		{
		}
		else
			rs = -1;
		messageRespList.erase(remove(messageRespList.begin(), messageRespList.end(), &message_rsp_list), messageRespList.end());
	}
	else
	{
		usleep(1000 * timeout);
	}
	return rs;
	// return Write(dataReq, lenReq);
}

int BleProtocol::GetAppKey()
{
	string appkeyStr = gateway->getBleAppKey();
	if (appkeyStr.compare("") == 0)
	{
		LOGD("Appkey null");
		srand((int)time(0));
		for (int i = 0; i < 16; i++)
		{
			appKey[i] = rand() % 256;
		}
		string appkey = arrayToString844412((uint8_t *)appKey);
		LOGD("New ble_appkey: %s", appkey.c_str());
		database->GatewayUpdateAppKey(gateway, appkey);
	}
	else
	{
		LOGD("Appkey: %s", appkeyStr.c_str());
		appkeyStr.erase(appkeyStr.begin() + 8, appkeyStr.begin() + 9);
		appkeyStr.erase(appkeyStr.begin() + 12, appkeyStr.begin() + 13);
		appkeyStr.erase(appkeyStr.begin() + 16, appkeyStr.begin() + 17);
		appkeyStr.erase(appkeyStr.begin() + 20, appkeyStr.begin() + 21);
		char *ak = new char[appkeyStr.length() + 1];
		strcpy(ak, appkeyStr.c_str());
		uint8_t temp[17] = {0};
		for (int i = 0; i < 16; i++)
		{
			sscanf((char *)ak + i * 2, "%2x", (unsigned int *)&temp[i]);
			appKey[i] = temp[i];
		}
	}
	return 0;
}

int BleProtocol::GetNetKey()
{
	LOGD("GetNetKey");
	uint8_t d = HCI_GATEWAY_CMD_GET_PRO_SELF_STS;
	uint8_t dataRsp[100];
	int lenRsp;
	int rs = SendMessage(SYSTEM_REQ, &d, 1, HCI_GATEWAY_CMD_PRO_STS_RSP, dataRsp, &lenRsp, 2000);
	if (rs == 0)
	{
		typedef struct
		{
			uint8_t rev1;
			uint8_t netKey[16];
			uint8_t rev2[3];
			uint8_t magic[4];
			uint8_t addr[2];
		} data_message_t;
		data_message_t *data_message = (data_message_t *)dataRsp;

		uint32_t ivIndex = (data_message->magic[0] << 24) | (data_message->magic[1] << 16) | (data_message->magic[2] << 8) | (data_message->magic[3]);
		if ((ivIndex == 0x11223344) || (ivIndex == 0))
		{
			for (int i = 0; i < 16; i++)
			{
				netKey[i] = data_message->netKey[i];
			}
			nextAddr = data_message->addr[0] | (data_message->addr[1] << 8);
			if (nextAddr == 0)
				nextAddr = 2;
			LOGW("nextAddr: 0x%04X - %d", nextAddr, nextAddr);
		}
		else
		{
			srand((int)time(0));
			for (int i = 0; i < 16; i++)
			{
				netKey[i] = rand() % 256;
				gwKey[i] = rand() % 256;
			}
			SetNetKey();
			SetGwKey();
			string netkeyStr = arrayToString844412((uint8_t *)netKey);
			LOGD("New ble_netkey: %s", netkeyStr.c_str());
			database->GatewayUpdateNetKey(gateway, netkeyStr);

			string devicekeyGwStr = arrayToString844412((uint8_t *)gwKey);
			LOGD("New ble_devicekeyGw: %s", devicekeyGwStr.c_str());
			database->GatewayUpdateDeviceKey(gateway, devicekeyGwStr);

			rs = 1;
		}
	}
	else
	{
		LOGE("Send GetNetKey error, rs: %d", rs);
		rs = 1;
	}
	return rs;
}

int BleProtocol::SetNetKey()
{
	LOGD("SetNetKey");
	typedef struct
	{
		uint8_t opcode;
		uint8_t netKey[16];
		uint8_t rev[3];
		uint8_t magic[4];
		uint8_t addr[2];
	} set_netkey_message_t;
	set_netkey_message_t set_netkey_message;
	memset(&set_netkey_message, 0x00, sizeof(set_netkey_message));
	set_netkey_message.opcode = HCI_GATEWAY_CMD_SET_PRO_PARA;
	for (int i = 0; i < 16; i++)
	{
		set_netkey_message.netKey[i] = netKey[i];
	}
	set_netkey_message.magic[0] = 0x11;
	set_netkey_message.magic[1] = 0x22;
	set_netkey_message.magic[2] = 0x33;
	set_netkey_message.magic[3] = 0x44;
	set_netkey_message.addr[0] = 0x01;
	set_netkey_message.addr[1] = 0x00;
	uint16_t adrGw = set_netkey_message.addr[0] | (set_netkey_message.addr[1] << 8);
	gateway->setBleUnicast(adrGw);
	database->GatewayUpdateUnicast(gateway, adrGw);
	return SendMessage(SYSTEM_REQ, (uint8_t *)&set_netkey_message, 26, HCI_GATEWAY_CMD_SEND_IVI, 0, 0, 1000);
}

int BleProtocol::SetGwKey()
{
	LOGD("SetGwKey");
	typedef struct
	{
		uint8_t opcode;
		uint8_t addr[2];
		uint8_t gwKey[16];
	} set_gwkey_message_t;
	set_gwkey_message_t set_gwkey_message;
	set_gwkey_message.opcode = 0x0D;
	set_gwkey_message.addr[0] = 0x01;
	set_gwkey_message.addr[1] = 0x00;
	for (int i = 0; i < 16; i++)
	{
		set_gwkey_message.gwKey[i] = gwKey[i];
	}
	return SendMessage(SYSTEM_REQ, (uint8_t *)&set_gwkey_message, 19, 0, 0, 0, 1000);
}

int BleProtocol::StartScan()
{
	LOGD("StartScan BLE");
	return esp_ble_mesh_provisioner_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
}

int BleProtocol::StopScan()
{
	LOGD("StopScan");
	return esp_ble_mesh_provisioner_prov_disable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
}

int BleProtocol::ResetFactory()
{
	LOGD("ResetFactory");
	uint8_t d = HCI_GATEWAY_CMD_RESET;
	int rs = SendMessage(SYSTEM_REQ, &d, 1, 0, 0, 0, 5000);
	if (rs)
	{
		LOGE("Send reset factory error, rs: %d", rs);
		return 1;
	}
	return 0;
}

string BleProtocol::uuidToStr(uuid_t *uuid)
{
	char buf[100];
	uint8_t *u8Uuid = (uint8_t *)uuid;
	sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
					u8Uuid[0], u8Uuid[1], u8Uuid[2], u8Uuid[3],
					u8Uuid[4], u8Uuid[5], u8Uuid[6], u8Uuid[7],
					u8Uuid[8], u8Uuid[9], u8Uuid[10], u8Uuid[11],
					u8Uuid[12], u8Uuid[13], u8Uuid[14], u8Uuid[15]);
	buf[36] = '\0';
	return string(buf);
}

string BleProtocol::arrayToString844412(uint8_t *array)
{
	char buf[100];
	sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
					array[0], array[1], array[2], array[3],
					array[4], array[5], array[6], array[7],
					array[8], array[9], array[10], array[11],
					array[12], array[13], array[14], array[15]);
	buf[36] = '\0';
	return string(buf);
}

static uint32_t convertDeviceType(uint32_t type)
{
	uint8_t *arr = (uint8_t *)&type;
	return (arr[0] + (arr[1] * 1000) + (arr[2] * 10000));
}

bool BleProtocol::AddDevice(scan_device_message_t *scan_device_message)
{
	LOGD("AddDevice");
	uint16_t version = 0;
	uint32_t deviceType = 0;
	uuid_t *uuid = (uuid_t *)scan_device_message->uuid;
	string mac = Util::ConvertU32ToHexString(scan_device_message->mac, sizeof(scan_device_message->mac));
	LOGI("Scan device mac 0x%s, rssi: %i", mac.c_str(), scan_device_message->rssi);
	if (!SelectMac(scan_device_message->mac) && isProvisioning)
	{
		if (!GetNetKey() && isProvisioning)
		{
			if (!Provision(nextAddr) && isProvisioning)
			{
				if (!BindingAll() && isProvisioning)
				{
					if (!SetGwAddr(nextAddr) && isProvisioning)
					{
						Device *device = gateway->getDevice(mac);
						if (device)
						{
							LOGI("Update device");
							device->SetAddr(nextAddr);
							database->DeviceUpdate(device);
						}
						else
						{
							if (!GetDeviceType(scan_device_message->mac, nextAddr, deviceType, version) && isProvisioning)
							{
								deviceType = convertDeviceType(deviceType);
								if (deviceType == BLE_DOWNLIGHT_SMT ||
										deviceType == BLE_DOWNLIGHT_COB_GOC_RONG ||
										deviceType == BLE_DOWNLIGHT_COB_GOC_HEP ||
										deviceType == BLE_DOWNLIGHT_COB_TRANG_TRI ||
										deviceType == BLE_DOWNLIGHT_RGBCW ||
										deviceType == BLE_PANEL_TRON ||
										deviceType == BLE_PANEL_VUONG ||
										deviceType == BLE_LED_OP_TRAN ||
										deviceType == BLE_LED_OP_TUONG ||
										deviceType == BLE_LED_CHIEU_TRANH ||
										deviceType == BLE_TRACKLIGHT ||
										deviceType == BLE_LED_THA_TRAN ||
										deviceType == BLE_LED_CHIEU_GUONG ||
										deviceType == BLE_LED_DAY_LINEAR ||
										deviceType == BLE_LED_TUBE_M16 ||
										deviceType == BLE_DEN_BAN ||
										deviceType == BLE_LED_FLOOD ||
										deviceType == BLE_LED_DAY_RGB ||
										deviceType == BLE_LED_DAY_RGBCW ||
										deviceType == BLE_LED_BULB ||
										deviceType == BLE_LED_OP_TRAN_LOA ||
										deviceType == BLE_SWITCH_4 ||
										deviceType == BLE_DC_SCENE_CONTACT ||
										deviceType == BLE_TEMP_HUM_SENSOR)
								{
									device = gateway->AddNewDevice(uuidToStr(uuid), Device::ConvertDeviceTypeToName(deviceType), mac, arrayToString844412((uint8_t *)deviceKey), nextAddr, deviceType, version, true, true);
									if (device)
									{
										gateway->AddDeviceToScanList(device);
										isAdding = true;
										StartScan();
										return false;
									}
									else
									{
										ResetDev(nextAddr);
										isAdding = true;
										StartScan();
										return false;
									}
								}
								else
								{
									LOGW("Ble device type 0x%04X not support", deviceType);
									ResetDev(nextAddr);
									isAdding = true;
									StartScan();
									return false;
								}
							}
							else
							{
								ResetDev(nextAddr);
								if (!isProvisioning)
								{
									isAdding = false;
									return false;
								}
								else
								{
									LOGW("GetDeviceType false");
									isAdding = true;
									StartScan();
									return false;
								}
							}
						}
					}
					else
					{
						ResetDev(nextAddr);
						if (!isProvisioning)
						{
							isAdding = false;
							return false;
						}
						else
						{
							LOGW("SetGwAddr false");
							isAdding = true;
							StartScan();
							return false;
						}
					}
				}
				else
				{
					ResetDev(nextAddr);
					if (!isProvisioning)
					{
						isAdding = false;
						return false;
					}
					else
					{
						LOGW("BindingAll false");
						isAdding = true;
						StartScan();
						return false;
					}
				}
			}
			else
			{
				ResetDev(nextAddr);
				if (!isProvisioning)
				{
					isAdding = false;
					return false;
				}
				else
				{
					LOGW("Provision false");
					isAdding = true;
				}
			}
		}
		else
		{
			if (!isProvisioning)
			{
				isAdding = false;
				return false;
			}
			else
			{
				LOGW("Get NWK false");
				isAdding = true;
			}
		}
	}
	else
	{
		if (!isProvisioning)
		{
			isAdding = false;
			return false;
		}
		else
		{
			LOGW("Select Mac false");
			isAdding = true;
		}
	}
	isAdding = false;
	return false;
}

int BleProtocol::SelectMac(uint8_t *mac)
{
	LOGD("SelectMac");
	uint8_t data[7];
	data[0] = HCI_GATEWAY_CMD_SET_ADV_FILTER;
	for (int i = 0; i < 6; i++)
	{
		data[i + 1] = mac[i];
	}
	return SendMessage(SYSTEM_REQ, data, 7, 0, 0, 0, 1000);
}

int BleProtocol::Provision(uint16_t deviceAddr)
{
	LOGD("Provision");
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t opcode;
		uint8_t netKey[16];
		uint8_t rev[3];
		uint8_t magic[4];
		uint8_t addr[2];
	} provision_message_t;
	provision_message_t provision_message;
	memset(&provision_message, 0x00, sizeof(provision_message));
	provision_message.opcode = HCI_GATEWAY_CMD_SET_NODE_PARA;
	for (int i = 0; i < 16; i++)
	{
		provision_message.netKey[i] = netKey[i];
	}
	provision_message.magic[0] = 0x11;
	provision_message.magic[1] = 0x22;
	provision_message.magic[2] = 0x33;
	provision_message.magic[3] = 0x44;
	provision_message.addr[0] = deviceAddr & 0xFF;
	provision_message.addr[1] = (deviceAddr >> 8) & 0xFF;
	int rs = SendMessage(SYSTEM_REQ, (uint8_t *)&provision_message, 26, HCI_GATEWAY_CMD_PROVISION_EVT, dataRsp, &lenRsp, 15000);
	if (rs == 0)
	{
		typedef struct
		{
			uint8_t status;
			uint8_t data[24];
		} provision_rsp_message_t;
		provision_rsp_message_t *provision_rsp_message = (provision_rsp_message_t *)dataRsp;
		if (provision_rsp_message->status)
		{
			LOGD("Provision OK");
			return 0;
		}
		else
		{
			LOGW("Must hard reset Ble module");
		}
	}
	LOGW("Provision err");
	return -1;
}

int BleProtocol::BindingAll()
{
	LOGD("BindingAll");
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t opcode;
		uint8_t rev[3];
		uint8_t appKey[16];
	} binding_all_message_t;
	binding_all_message_t binding_all_message;
	memset(&binding_all_message, 0x00, sizeof(binding_all_message));
	binding_all_message.opcode = HCI_GATEWAY_CMD_START_KEYBIND;
	for (int i = 0; i < 16; i++)
	{
		binding_all_message.appKey[i] = appKey[i];
	}
	int rs = SendMessage(SYSTEM_REQ, (uint8_t *)&binding_all_message, 20, HCI_GATEWAY_CMD_KEY_BIND_EVT, dataRsp, &lenRsp, 30000);
	if (rs == 0)
	{
		if (lenRsp == 1 && dataRsp[0] == 1)
		{
			LOGD("BindingAll OK");
			return 0;
		}
	}
	LOGW("BindingAll err");
	return -1;
}

int BleProtocol::SetGwAddr(uint16_t devAddr, uint16_t gwAddr2)
{
	LOGD("SetGwAddr");
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t setGwAddrHeader[] = {0xe1, 0x11, 0x02, 0x02, 0x00};
	typedef struct
	{
		uint8_t rev[4];
		uint16_t header;
		uint16_t devAddr;
		uint8_t data[13];
	} set_gw_addr_message_t;
	set_gw_addr_message_t set_gw_addr_message;
	memset(&set_gw_addr_message, 0x00, sizeof(set_gw_addr_message));
	set_gw_addr_message.header = 0;
	set_gw_addr_message.devAddr = devAddr;
	set_gw_addr_message.data[0] = 0xE0;
	set_gw_addr_message.data[1] = 0x11;
	set_gw_addr_message.data[2] = 0x02;
	set_gw_addr_message.data[3] = 0xE1;
	set_gw_addr_message.data[4] = 0x00;
	set_gw_addr_message.data[5] = 0x02;
	int rs = SendMessage(APP_REQ, (uint8_t *)&set_gw_addr_message, 21, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 5000, setGwAddrHeader, 4, 5);
	if (rs == 0)
	{
		typedef struct
		{
			uint16_t devAddr;
			uint16_t gwAddr;
			uint8_t opcode[3];
			uint8_t header[2];
			uint8_t rev[6];
		} set_gw_addr_rsp_message_t;
		set_gw_addr_rsp_message_t *set_gw_addr_rsp_message = (set_gw_addr_rsp_message_t *)dataRsp;
		if (set_gw_addr_rsp_message->devAddr == devAddr)
		{
			if (set_gw_addr_rsp_message->opcode[0] == 0xE1 && set_gw_addr_rsp_message->opcode[1] == 0x11 && set_gw_addr_rsp_message->opcode[2] == 0x02 && set_gw_addr_rsp_message->header[0] == 0x02 && set_gw_addr_rsp_message->header[1] == 0x00)
			{
				LOGD("SetGwAddr OK");
				return 0;
			}
		}
	}
	LOGW("SetGwAddr err");
	return -1;
}

/**
 * @brief Gen Ble security key (6 bytes)
 *
 * @param mac mac of destination device
 * @param devAddr unicast addr of destination device
 * @param out out buffer to write key
 */
static void genSecurityKey(uint8_t *mac, uint16_t devAddr, uint8_t *out)
{
	AES aes(AESKeyLength::AES_128);
	memcpy(plaintext + 8, mac, 6);
	memcpy(plaintext + 14, (uint8_t *)&devAddr, 2);
	for (int n = 0; n < 16; n++)
	{
		printf("%02x ", plaintext[n]);
	}
	printf("\n");
	unsigned char *outAes = aes.EncryptECB(plaintext, 32, keyAes);
	for (int j = 0; j < 32; j++)
	{
		printf("%02x ", outAes[j]);
	}
	printf("\n");
	for (int i = 0; i < 6; i++)
	{
		out[i] = outAes[i + 10];
	}
	free(outAes);
}

int BleProtocol::GetDeviceType(uint8_t *mac, uint16_t devAddr, uint32_t &deviceType, uint16_t &deviceVersion)
{
	LOGD("GetDeviceType");
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t checkTypeHeader[] = {0xe1, 0x11, 0x02, 0x03, 0x00};
	typedef struct
	{
		uint8_t rev[4];
		uint16_t header;
		uint16_t addr;
		uint8_t data[13];
	} check_type_message_t;
	check_type_message_t check_type_message;
	memset(&check_type_message, 0x00, sizeof(check_type_message));
	check_type_message.header = 0;
	check_type_message.addr = devAddr;
	check_type_message.data[0] = 0xE0;
	check_type_message.data[1] = 0x11;
	check_type_message.data[2] = 0x02;
	check_type_message.data[3] = 0xE1;
	check_type_message.data[4] = 0x00;
	check_type_message.data[5] = 0x03;
	genSecurityKey(mac, devAddr, &check_type_message.data[7]);
	int rs = SendMessage(APP_REQ, (uint8_t *)&check_type_message, 21, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 5000, checkTypeHeader, 4, 5);
	if (rs == 0)
	{
		typedef struct
		{
			uint16_t devAddr;
			uint16_t gwAddr;
			uint8_t opcode[3];
			uint8_t header[2];
			uint8_t deviceType[3];
			uint8_t magic;
			uint8_t version[2];
		} check_type_rsp_message_t;
		check_type_rsp_message_t *check_type_rsp_message = (check_type_rsp_message_t *)dataRsp;
		if (check_type_rsp_message->devAddr == devAddr)
		{
			if (check_type_rsp_message->opcode[0] == 0xE1 && check_type_rsp_message->opcode[1] == 0x11 && check_type_rsp_message->opcode[2] == 0x02 && check_type_rsp_message->header[0] == 0x03 && check_type_rsp_message->header[1] == 0x00)
			{
				deviceType = (check_type_rsp_message->deviceType[0] << 16) | (check_type_rsp_message->deviceType[1] << 8) | check_type_rsp_message->deviceType[2];
				deviceVersion = (check_type_rsp_message->version[0] << 8) | (check_type_rsp_message->version[1]);
				LOGD("GetDeviceType OK, deviceType: 0x%04X, version: %d", deviceType, deviceVersion);
				return 0;
			}
		}
	}
	LOGW("GetDeviceType err");
	return -1;
}

int BleProtocol::ResetDev(uint16_t devAddr)
{
	LOGD("Reset dev addr: 0x%04X", devAddr);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
	} reset_message_t;
	reset_message_t reset_message = {0};
	memset(&reset_message, 0x00, sizeof(reset_message));
	uint8_t resetHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x80, 0x4a};
	reset_message.addr = devAddr;
	reset_message.opcode = 0x4980;
	int rs = SendMessage(APP_REQ, (uint8_t *)&reset_message, 10, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, resetHeader, 0, 6);
	if (rs == 0)
	{
		return 0;
	}
	return -1;
}

int BleProtocol::SetOnOffLight(uint16_t devAddr, uint8_t onoff, uint16_t transition, bool ack)
{
	LOGD("Set OnOff addr: 0x%04X value %d", devAddr, onoff);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint8_t onoff;
		uint8_t rev2;
		uint8_t transition[2];
	} onoff_message_t;
	onoff_message_t onoff_message = {0};
	memset(&onoff_message, 0x00, sizeof(onoff_message));
	if (ack)
	{
		uint8_t turnOnOffHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x04, onoff};
		onoff_message.addr = devAddr;
		onoff_message.opcode = 0x0282;
		onoff_message.onoff = onoff;
		onoff_message.rev2 = 0;
		onoff_message.transition[0] = transition & 0xFF;
		onoff_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&onoff_message, 14, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, turnOnOffHeader, 0, 7);
		if (rs == 0)
		{
			typedef struct
			{
				uint16_t devAddr;
				uint16_t gwAddr;
				uint16_t opcode;
				uint8_t data[3];
			} onoff_rsp_message_t;
			onoff_rsp_message_t *onoff_rsp_message = (onoff_rsp_message_t *)dataRsp;
			if (onoff_rsp_message->opcode == 0x0482)
			{
				if (lenRsp == 7)
				{
					if (onoff == onoff_rsp_message->data[0])
						return 0;
				}
				else
				{
					if (onoff == onoff_rsp_message->data[1])
						return 0;
				}
				LOGW("Onoff resp state not match with input control");
			}
		}
	}
	else
	{
		onoff_message.addr = devAddr;
		onoff_message.opcode = 0x0382;
		onoff_message.onoff = onoff;
		onoff_message.rev2 = 0;
		onoff_message.transition[0] = transition & 0xFF;
		onoff_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&onoff_message, 14, 0, dataRsp, &lenRsp, 1000);
		if (rs == 0)
		{
			return 0;
		}
	}
	LOGW("SetOnOff err");
	return -1;
}

int BleProtocol::SetDimmingLight(uint16_t devAddr, uint16_t dim, uint16_t transition, bool ack)
{
	LOGD("Dimming addr: 0x%04X value %d", devAddr, dim);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t dim;
		uint8_t offset;
		uint8_t transition[2];
	} dim_message_t;
	dim_message_t dim_message;
	memset(&dim_message, 0x00, sizeof(dim_message));
	if (ack)
	{
		uint8_t dimmingHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x4E};

		dim_message.addr = devAddr;
		dim_message.opcode = 0x4C82;
		dim_message.dim = dim;
		dim_message.offset = 0;
		dim_message.transition[0] = transition & 0xFF;
		dim_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&dim_message, 15, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, dimmingHeader, 0, 6);
		if (rs == 0)
		{
			typedef struct
			{
				uint16_t devAddr;
				uint16_t gwAddr;
				uint16_t opcode;
				uint8_t data[8];
			} dim_rsp_message_t;
			dim_rsp_message_t *dim_rsp_message = (dim_rsp_message_t *)dataRsp;
			if (lenRsp == 8)
			{
				if ((dim_rsp_message->data[0] | dim_rsp_message->data[1] << 8) == dim)
				{
					return 0;
				}
			}
			else if (lenRsp > 8)
			{
				if ((dim_rsp_message->data[2] | dim_rsp_message->data[3] << 8) == dim)
				{
					return 0;
				}
			}
		}
	}
	else
	{
		dim_message.addr = devAddr;
		dim_message.opcode = 0x4d82;
		dim_message.dim = dim;
		dim_message.offset = 0;
		dim_message.transition[0] = transition & 0xFF;
		dim_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&dim_message, 15, 0, dataRsp, &lenRsp, 1000);
		if (rs == 0)
		{
			return 0;
		}
	}
	LOGW("Dimming err");
	return -1;
}

int BleProtocol::SetCctLight(uint16_t devAddr, uint16_t cct, uint16_t transition, bool ack)
{
	LOGD("Set Cct addr: 0x%04X value %d", devAddr, cct);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t cct;
		uint8_t offset[3];
		uint8_t transition[2];
	} cct_message_t;
	cct_message_t cct_message;
	memset(&cct_message, 0x00, sizeof(cct_message));
	if (ack)
	{
		uint8_t cctHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 00, 0x82, 0x66};
		cct_message.addr = devAddr;
		cct_message.opcode = 0x6482;
		cct_message.cct = cct;
		for (int count = 0; count < 3; count++)
		{
			cct_message.offset[count] = 0;
		}
		cct_message.transition[0] = transition & 0xFF;
		cct_message.transition[1] = (transition >> 8) & 0xFF;

		int rs = SendMessage(APP_REQ, (uint8_t *)&cct_message, 17, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, cctHeader, 0, 6);
		if (rs == 0)
		{
			typedef struct
			{
				uint16_t devAddr;
				uint16_t gwAddr;
				uint16_t opcode;
				uint8_t data[8];
			} cct_rsp_message_t;
			cct_rsp_message_t *cct_rsp_message = (cct_rsp_message_t *)dataRsp;
			if (lenRsp == 10)
			{
				if (cct == (cct_rsp_message->data[0] | (cct_rsp_message->data[1] << 8)))
				{
					return 0;
				}
			}
			else if (lenRsp > 10)
			{
				if (cct == (cct_rsp_message->data[4] | (cct_rsp_message->data[5] << 8)))
				{
					return 0;
				}
			}
		}
	}
	else if (ack == false)
	{
		cct_message.addr = devAddr;
		cct_message.opcode = 0x6582;
		cct_message.cct = cct;
		for (int count = 0; count < 3; count++)
		{
			cct_message.offset[count] = 0;
		}
		cct_message.transition[0] = transition & 0xFF;
		cct_message.transition[1] = (transition >> 8) & 0xFF;

		int rs = SendMessage(APP_REQ, (uint8_t *)&cct_message, 10, 0, dataRsp, &lenRsp, 1000);
		if (rs == 0)
		{
			return 0;
		}
	}
	LOGW("Cct err");
	return -1;
}

int BleProtocol::SetHSLLight(uint16_t devAddr, uint16_t H, uint16_t S, uint16_t L, uint16_t transition, bool ack)
{
	LOGD("HSL addr: 0x%04X value HSL: %d-%d-%d", devAddr, H, S, L);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t l;
		uint16_t h;
		uint16_t s;
		uint8_t offset;
		uint8_t transition[2];
	} hsl_message_t;
	hsl_message_t hsl_message = {0};
	memset(&hsl_message, 0x00, sizeof(hsl_message));
	if (ack)
	{
		uint8_t hslHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x78};
		hsl_message.addr = devAddr;
		hsl_message.opcode = 0x7682;
		hsl_message.l = L;
		hsl_message.h = H;
		hsl_message.s = S;
		hsl_message.offset = 0;
		hsl_message.transition[0] = transition & 0xFF;
		hsl_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&hsl_message, 19, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, hslHeader, 0, 6);
		if (rs == 0)
		{
			typedef struct
			{
				uint16_t devAddr;
				uint16_t gwAddr;
				uint16_t opcode;
				uint16_t l;
				uint16_t h;
				uint16_t s;
			} hsl_rsp_message_t;
			hsl_rsp_message_t *hsl_rsp_message = (hsl_rsp_message_t *)dataRsp;
			if (hsl_rsp_message->h == H && hsl_rsp_message->l == L && hsl_rsp_message->s == S)
			{
				return 0;
			}
			LOGW("hsl resp state not match with input control");
		}
	}
	else
	{
		hsl_message.addr = devAddr;
		hsl_message.opcode = 0x7782;
		hsl_message.l = L;
		hsl_message.h = H;
		hsl_message.s = S;
		hsl_message.offset = 0;
		hsl_message.transition[0] = transition & 0xFF;
		hsl_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&hsl_message, 19, 0, dataRsp, &lenRsp, 1000);
		if (rs == 0)
		{
			return 0;
		}
	}
	LOGW("Set hsl err");
	return -1;
}

int BleProtocol::SetCctDimLight(uint16_t devAddr, uint16_t cct, uint16_t dim, uint16_t transition, bool ack)
{
	LOGD("Set Dim cct addr: 0x%04X", devAddr);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t dim;
		uint16_t cct;
		uint8_t offset;
		uint8_t transition[2];
	} dimcct_message_t;
	dimcct_message_t dimcct_message = {0};
	memset(&dimcct_message, 0x00, sizeof(dimcct_message));
	if (ack)
	{
		uint8_t dimcctHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x60};
		dimcct_message.addr = devAddr;
		dimcct_message.opcode = 0x5e82;
		dimcct_message.dim = dim;
		dimcct_message.cct = cct;
		dimcct_message.offset = 0;
		dimcct_message.transition[0] = transition & 0xFF;
		dimcct_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&dimcct_message, 19, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, dimcctHeader, 0, 6);
		if (rs == 0)
		{
			typedef struct
			{
				uint16_t devAddr;
				uint16_t gwAddr;
				uint16_t opcode;
				uint16_t dim;
				uint16_t cct;
			} dimcct_rsp_message_t;
			dimcct_rsp_message_t *dimcct_rsp_message = (dimcct_rsp_message_t *)dataRsp;
			if (dimcct_rsp_message->dim == dim && dimcct_rsp_message->cct == cct)
			{
				return 0;
			}
			LOGW("dim cct resp state not match with input control");
		}
	}
	else
	{
		dimcct_message.addr = devAddr;
		dimcct_message.opcode = 0x5f82;
		dimcct_message.dim = dim;
		dimcct_message.cct = cct;
		dimcct_message.offset = 0;
		dimcct_message.transition[0] = transition & 0xFF;
		dimcct_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&dimcct_message, 19, 0, dataRsp, &lenRsp, 1000);
		if (rs == 0)
		{
			return 0;
		}
	}
	LOGW("Set dim cct err");
	return -1;
}

int BleProtocol::AddDev2Group(uint16_t devAddr, uint16_t element, uint16_t group)
{
	LOGD("Add dev addr: 0x%04X  with element: 0x%04x to group: 0x%04X", devAddr, element, group);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t addGroupHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x80, 0x1f};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t element;
		uint16_t group;
		uint8_t offset[2];
	} addgroup_message_t;
	addgroup_message_t addgroup_message = {0};
	memset(&addgroup_message, 0x00, sizeof(addgroup_message));
	addgroup_message.addr = devAddr;
	addgroup_message.opcode = 0x1b80;
	addgroup_message.element = element;
	addgroup_message.group = group;
	addgroup_message.offset[0] = 0;
	addgroup_message.offset[1] = 0x10;
	int rs = SendMessage(APP_REQ, (uint8_t *)&addgroup_message, 16, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, addGroupHeader, 0, 6);
	if (rs == 0)
	{
		typedef struct
		{
			uint16_t devAddr;
			uint16_t gwAddr;
			uint16_t opcode;
			uint8_t offset;
			uint8_t element[2];
			uint8_t group[2];
		} addgroup_rsp_message_t;
		addgroup_rsp_message_t *addgroup_rsp_message = (addgroup_rsp_message_t *)dataRsp;
		LOGD("adr : 0x%04x, gw:0x%04x, opcode:0x%04x, offset:0x%02x, element: 0x%04x, group: 0x%04x", addgroup_rsp_message->devAddr, addgroup_rsp_message->gwAddr, addgroup_rsp_message->opcode, addgroup_rsp_message->offset, addgroup_rsp_message->element[0] | (addgroup_rsp_message->element[1] << 8), addgroup_rsp_message->group[0] | (addgroup_rsp_message->group[1] << 8));
		if (element == (addgroup_rsp_message->element[0] | (addgroup_rsp_message->element[1] << 8)) && ((addgroup_rsp_message->group[0] | (addgroup_rsp_message->group[1] << 8)) == group))
		{
			return 0;
		}
		LOGW("add group resp state not match with input control");
	}
	LOGW("Add group err");
	return -1;
}

int BleProtocol::DelDev2Group(uint16_t devAddr, uint16_t element, uint16_t group)
{
	LOGD("Del dev addr: 0x%04X  with element: 0x%04x to group: 0x%04X", devAddr, element, group);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t delGroupHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x80, 0x1f};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t element;
		uint16_t group;
		uint8_t offset[2];
	} delgroup_message_t;
	delgroup_message_t delgroup_message = {0};
	memset(&delgroup_message, 0x00, sizeof(delgroup_message));
	delgroup_message.addr = devAddr;
	delgroup_message.opcode = 0x1c80;
	delgroup_message.element = element;
	delgroup_message.group = group;
	delgroup_message.offset[0] = 0;
	delgroup_message.offset[1] = 0x10;
	int rs = SendMessage(APP_REQ, (uint8_t *)&delgroup_message, 16, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, delGroupHeader, 0, 6);
	if (rs == 0)
	{
		return 0;
	}
	LOGW("Del group err");
	return -1;
}

int BleProtocol::SetSceneLights(uint16_t devAddr, uint16_t scene, uint8_t modeRgb)
{
	LOGD("Set scene addr: 0x%04X to scene: 0x%04X", devAddr, scene);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t setSceneHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x45};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t scene;
		uint8_t modeRgb;
		uint8_t offset[2];
	} setscene_message_t;
	setscene_message_t setscene_message = {0};
	memset(&setscene_message, 0x00, sizeof(setscene_message));
	setscene_message.addr = devAddr;
	setscene_message.opcode = 0x4682;
	setscene_message.scene = scene;
	setscene_message.modeRgb = modeRgb;
	setscene_message.offset[0] = 0;
	setscene_message.offset[1] = 0;
	int rs = SendMessage(APP_REQ, (uint8_t *)&setscene_message, 15, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, setSceneHeader, 0, 6);
	if (rs == 0)
	{
		typedef struct
		{
			uint16_t devAddr;
			uint16_t gwAddr;
			uint16_t opcode;
			uint8_t offset;
			uint16_t scene;
		} setscene_rsp_message_t;
		setscene_rsp_message_t *setscene_rsp_message = (setscene_rsp_message_t *)dataRsp;
		if (setscene_rsp_message->scene == scene)
		{
			return 0;
		}
		LOGW("set scene resp state not match with input control");
	}
	LOGW("Set scene err");
	return -1;
}

// TODO: BelProtocol DelScene
int BleProtocol::DelSceneLights(uint16_t devAddr, uint16_t scene)
{
	LOGD("Del scene addr: 0x%04X to scene: 0x%04X", devAddr, scene);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t delSceneHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x45};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t scene;
	} delscene_message_t;
	delscene_message_t delscene_message = {0};
	memset(&delscene_message, 0x00, sizeof(delscene_message));
	delscene_message.addr = devAddr;
	delscene_message.opcode = 0x9e82;
	delscene_message.scene = scene;
	int rs = SendMessage(APP_REQ, (uint8_t *)&delscene_message, 12, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, delSceneHeader, 0, 6);
	if (rs == 0)
	{
		return 0;
	}
	LOGW("Del scene err");
	return -1;
}

// TODO: BelProtocol ActiveScene
// add delay time
int BleProtocol::CallScene(uint16_t devAddr, uint16_t scene, uint16_t transition, bool ack, int delayTime)
{
	LOGD("Call scene: 0x%04X", scene);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t callSceneHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x5e, 0x00};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint16_t scene;
		uint8_t offset;
		uint8_t transition[2];
	} callscene_message_t;
	callscene_message_t callscene_message = {0};
	memset(&callscene_message, 0x00, sizeof(callscene_message));
	if (ack)
	{
		callscene_message.addr = devAddr;
		callscene_message.opcode = 0x4282;
		callscene_message.scene = scene;
		callscene_message.offset = 0;
		callscene_message.transition[0] = transition;
		callscene_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&callscene_message, 15, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, callSceneHeader, 0, 6);
		if (rs == 0)
		{
			typedef struct
			{
				uint16_t devAddr;
				uint16_t gwAddr;
				uint16_t opcode;
				uint8_t data[8];
			} callscene_rsp_message_t;
			callscene_rsp_message_t *callscene_rsp_message = (callscene_rsp_message_t *)dataRsp;
			if (lenRsp == 11 || lenRsp == 13)
			{
				if (scene == (callscene_rsp_message->data[2] | (callscene_rsp_message->data[3] << 8)))
				{
					return 0;
				}
				else
				{
					LOGW("call scene resp state not match with input control");
				}
			}
			else
			{
				if (scene == (callscene_rsp_message->data[0] | (callscene_rsp_message->data[1] << 8)))
				{
					return 0;
				}
				else
				{
					LOGW("call scene resp state not match with input control");
				}
			}
		}
	}
	else
	{
		callscene_message.addr = devAddr;
		callscene_message.opcode = 0x4382;
		callscene_message.scene = scene;
		callscene_message.offset = 0;
		callscene_message.transition[0] = transition;
		callscene_message.transition[1] = (transition >> 8) & 0xFF;
		int rs = SendMessage(APP_REQ, (uint8_t *)&callscene_message, 15, 0, dataRsp, &lenRsp, 1000);
		if (rs == 0)
		{
			return 0;
		}
	}

	LOGW("Call scene err");
	return -1;
}

int BleProtocol::CallModeRgb(uint16_t devAddr, uint8_t modeRgb)
{
	LOGD("Call modeRgb: %d, addr: 0x%04X ", modeRgb, devAddr);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t modeRgbHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x52};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint8_t mode;
	} modergb_message_t;
	modergb_message_t modergb_message = {0};
	memset(&modergb_message, 0x00, sizeof(modergb_message));
	modergb_message.addr = devAddr;
	modergb_message.opcode = 0x0919;
	modergb_message.mode = modeRgb;
	int rs = SendMessage(APP_REQ, (uint8_t *)&modergb_message, 12, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, modeRgbHeader, 0, 6);
	if (rs == 0)
	{
		typedef struct
		{
			uint16_t devAddr;
			uint16_t gwAddr;
			uint16_t opcode;
			uint16_t header;
			uint8_t mode;
		} modergb_rsp_message_t;
		modergb_rsp_message_t *modergb_rsp_message = (modergb_rsp_message_t *)dataRsp;
		if (modergb_rsp_message->header == 0x0919)
		{
			if (modergb_rsp_message->mode == modeRgb)
			{
				return 0;
			}
			LOGW("call mode rgb resp state not match with input control");
		}
	}
	LOGW("call mode rgb err");
	return -1;
}

int BleProtocol::UpdateLights(uint16_t devAddr)
{
	LOGD("Update lights addr: 0x%04X ", devAddr);
	uint8_t dataRsp[100];
	int lenRsp;
	uint8_t updateHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 0, 0x82, 0x52};
	typedef struct
	{
		uint8_t rev[6];
		uint16_t addr;
		uint16_t opcode;
		uint8_t header;
		uint8_t data[7];
	} update_message_t;
	update_message_t update_message = {0};
	memset(&update_message, 0x00, sizeof(update_message));
	update_message.addr = devAddr;
	update_message.opcode = 0x5082;
	update_message.header = 0x02;
	int rs = SendMessage(APP_REQ, (uint8_t *)&update_message, 12, HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000, updateHeader, 0, 6);
	if (rs == 0)
	{
		typedef struct
		{
			uint16_t devAddr;
			uint16_t gwAddr;
			uint16_t opcode;
			uint8_t header;
		} update_rsp_message_t;
		update_rsp_message_t *update_rsp_message = (update_rsp_message_t *)dataRsp;
		if (update_rsp_message->header == 0x02)
		{
			return 0;
		}
		LOGW("update lights resp state not match with input control");
	}
	LOGW("update lights mode rgb err");
	return -1;
}
