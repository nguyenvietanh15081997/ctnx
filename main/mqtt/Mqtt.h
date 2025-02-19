#pragma once

// #include "esp_mqtt.hpp"
// #include "esp_mqtt_client_config.hpp"
#include "esp_event.h"
#include "mqtt_client.h"

#include "ActionCallback.h"

using namespace std;

class Mqtt
{
private:
	string host;
	int port;
	string clientId;
	string username;
	string password;
	int keepalive;
	bool useTls;
	string willsetTopic;
	string willsetPayload;
	esp_mqtt_client_handle_t client;

protected:
	int connected;

	vector<string> topicList;
	vector<ActionCallback> actionCallbacks;
	// vector<MQTTPubSub *> mqttSubscribes;

public:
	// using mqtt::Client::Client;
	Mqtt(string host, int port, string clientId, string username, string password, int keepalive, bool useTls = false);

	void init();
	void SetWillset(string willset_topic, string willset_payload);

	int Connect(int timeout = 10);

	virtual void OnConnect(bool isConnected, bool isReconnect) {}

	int Subscribe(string topic);
	int Unsubscribe(string topic);
	int Publish(string topic, string payload);
	int Publish(string topic, char *payload, int payloadLen);
	bool isConnected();
	void ResubcribeAllTopic();

	void addActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic);
	void addActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic);
	void addActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic);
	void addActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic);
	int findActionCallbackFuncFromTopic(string topic, ActionCallback **actionCallback);

	void on_connected(esp_mqtt_event_handle_t const event);
	void on_disconnected(esp_mqtt_event_handle_t const event);
	void on_subscribed(esp_mqtt_event_handle_t const event);
	void on_unsubscribed(esp_mqtt_event_handle_t const event);
	void on_data(esp_mqtt_event_handle_t const event);
};
