#pragma once

#include <string>
#include <mutex>
#include "mongoose.h"
#include "freertos/queue.h"

#include "ActionCallback.h"

using namespace std;

// A list of client, held in memory
struct client
{
	struct client *next;
	struct mg_connection *c;
	struct mg_str cid;
};

// A list of subscription, held in memory
struct sub
{
	struct sub *next;
	struct mg_connection *c;
	struct mg_str topic;
	uint8_t qos;
};

// A list of will topic & message, held in memory
struct will
{
	struct will *next;
	struct mg_connection *c;
	struct mg_str topic;
	struct mg_str payload;
	uint8_t qos;
	uint8_t retain;
};

typedef struct
{
	char *topic;
	int topic_len;
	char *payload;
	int payload_len;
} on_message_t;

class MqttBroker
{

private:
	vector<string> topicList;
	vector<ActionCallback> actionCallbacks;

public:
	mutex mtx;
	QueueHandle_t queue;

	MqttBroker();

	void init();

	int Connect(int timeout = 10);
	int Subscribe(string topic);
	int Unsubscribe(string topic);
	int Publish(string topic, string payload);
	int Publish(string topic, char *payload, int payloadLen);
	bool isConnected();
	void ResubcribeAllTopic();

	virtual void OnConnect(bool isConnected, bool isReconnect) {}
	virtual void OnMessage(string topic, char *payload, int payloadLen);

	void addActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic);
	void addActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic);
	void addActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic);
	void addActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic);
	int findActionCallbackFuncFromTopic(string topic, ActionCallback **actionCallback);

	void on_data(const char *topic, int topic_len, char *data, int data_len);
};
