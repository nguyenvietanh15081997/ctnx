#include <stdio.h>
#include "Mqtt.h"
#include "Log.h"

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	LOGV("Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	Mqtt *mqtt = (Mqtt *)handler_args;
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		LOGI("MQTT_EVENT_CONNECTED");
		mqtt->on_connected(event);
		break;
	case MQTT_EVENT_DISCONNECTED:
		LOGV("MQTT_EVENT_DISCONNECTED");
		mqtt->on_disconnected(event);
		break;
	case MQTT_EVENT_SUBSCRIBED:
		LOGV("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		mqtt->on_subscribed(event);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		LOGV("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		mqtt->on_unsubscribed(event);
		break;
	case MQTT_EVENT_PUBLISHED:
		LOGV("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		LOGV("MQTT_EVENT_DATA");
		mqtt->on_data(event);
		break;
	case MQTT_EVENT_ERROR:
		LOGV("MQTT_EVENT_ERROR");
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			// log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
			// log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
			// log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
			LOGI("Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
		}
		break;
	default:
		LOGV("Other event id:%d", event->event_id);
		break;
	}
}

Mqtt::Mqtt(string host, int port, string clientId, string username, string password, int keepalive, bool useTls)
	: host(host), port(port), clientId(clientId), username(username), password(password), keepalive(keepalive), useTls(useTls)
{
}

void Mqtt::init()
{
	esp_mqtt_client_config_t mqtt_cfg = {0};
	// mqtt_cfg.broker.address.uri = host.c_str();
	mqtt_cfg.broker.address.hostname = host.c_str();
	mqtt_cfg.broker.address.port = (uint32_t)port;
	mqtt_cfg.broker.address.transport = useTls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
	mqtt_cfg.credentials.client_id = clientId.c_str();
	mqtt_cfg.credentials.username = username.c_str();
	mqtt_cfg.credentials.authentication.password = password.c_str();
	mqtt_cfg.session.last_will.topic = willsetTopic.c_str();
	mqtt_cfg.session.last_will.msg = willsetPayload.c_str();
	mqtt_cfg.session.last_will.msg_len = (int)willsetPayload.length();
	mqtt_cfg.session.keepalive = keepalive;
	mqtt_cfg.task.stack_size = 10 * 1024;
	mqtt_cfg.buffer.size = 20 * 1024;
	client = esp_mqtt_client_init(&mqtt_cfg);
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, this);
	esp_mqtt_client_start(client);
}

void Mqtt::SetWillset(string willsetTopic, string willsetPayload)
{
	this->willsetTopic = willsetTopic;
	this->willsetPayload = willsetPayload;
}

int Mqtt::Connect(int timeout)
{
	return 0;
}

int Mqtt::Subscribe(string topic)
{
	LOGD("Subscribe: %s", topic.c_str());
	esp_mqtt_client_subscribe(client, topic.c_str(), 0);
	return 0;
}

int Mqtt::Unsubscribe(string topic)
{
	LOGD("Unsubscribe: %s", topic.c_str());
	esp_mqtt_client_unsubscribe(client, topic.c_str());
	return 0;
}

int Mqtt::Publish(string topic, string payload)
{
	esp_mqtt_client_publish(client, topic.c_str(), payload.c_str(), payload.length(), 1, 0);
	return 0;
}

int Mqtt::Publish(string topic, char *payload, int payloadLen)
{
	esp_mqtt_client_publish(client, topic.c_str(), payload, payloadLen, 0, 0);
	return 0;
}

bool Mqtt::isConnected()
{
	return connected;
}

void Mqtt::ResubcribeAllTopic()
{
	for (auto &action : actionCallbacks)
	{
		Subscribe(action.getTopic());
	}
}

void Mqtt::addActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType1, topic);
	actionCallbacks.push_back(actionCallback);
	Subscribe(topic);
}

void Mqtt::addActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType2, topic);
	actionCallbacks.push_back(actionCallback);
	Subscribe(topic);
}

void Mqtt::addActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType3, topic);
	actionCallbacks.push_back(actionCallback);
	if (connected)
		Subscribe(topic);
	topicList.push_back(topic);
}

void Mqtt::addActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType4, topic);
	actionCallbacks.push_back(actionCallback);
	if (connected)
		Subscribe(topic);
	topicList.push_back(topic);
}

static vector<string> splitString(string str, char splitter)
{
	vector<string> result;
	string current = "";
	for (size_t i = 0; i < str.size(); i++)
	{
		if (str[i] == splitter)
		{
			if (current != "")
			{
				result.push_back(current);
				current = "";
			}
			continue;
		}
		current += str[i];
	}
	if (current.size() != 0)
		result.push_back(current);
	return result;
}

static int checkMqttTopic(string retrieveTopic, string registerTopic)
{
	vector<string> retrieveList = splitString(retrieveTopic, '/');
	vector<string> registerList = splitString(registerTopic, '/');
	for (size_t i = 0; i < retrieveList.size(); i++)
	{
		if (registerList.size() < i)
			return 0;
		if (registerList.at(i) == "#")
			return 1; // OK
		if (registerList.at(i) == "+")
			continue;
		if (registerList.at(i) != retrieveList.at(i))
			return 0;
	}
	if (registerList.size() == retrieveList.size())
		return 1; // OK
	return 0;
}

int Mqtt::findActionCallbackFuncFromTopic(string topic, ActionCallback **actionCallback)
{
	for (auto &action : actionCallbacks)
	{
		if (checkMqttTopic(topic, action.getTopic()))
		{
			*actionCallback = &action;
			return 1;
		}
	}
	return -1;
}

void Mqtt::on_connected(esp_mqtt_event_handle_t const event)
{
	ResubcribeAllTopic();
	connected = true;
	OnConnect(true, true);
}

void Mqtt::on_disconnected(esp_mqtt_event_handle_t const event)
{
	connected = false;
	OnConnect(false, true);
}

void Mqtt::on_subscribed(esp_mqtt_event_handle_t const event)
{
}

void Mqtt::on_unsubscribed(esp_mqtt_event_handle_t const event)
{
}

void Mqtt::on_data(esp_mqtt_event_handle_t const event)
{
	string topic = string(event->topic, event->topic + event->topic_len);
	LOGI("topic: %s", topic.c_str());
	ActionCallback *actionCallback;
	int getCallback = findActionCallbackFuncFromTopic(topic, &actionCallback);
	if (getCallback == 1)
	{
		if (actionCallback->getType() == 1)
		{
			string payloadStr = string(event->data, event->data_len);
			actionCallback->actionCallbackFuncType1(topic, payloadStr);
		}
		else if (actionCallback->getType() == 2)
		{
			actionCallback->actionCallbackFuncType2(topic, event->data, event->data_len);
		}
		else if (actionCallback->getType() == 3)
		{
			string payloadStr = string(event->data, event->data_len);
			actionCallback->actionCallbackFuncType3(topic, payloadStr);
		}
		else if (actionCallback->getType() == 4)
		{
			actionCallback->actionCallbackFuncType4(topic, event->data, event->data_len);
		}
	}
}
