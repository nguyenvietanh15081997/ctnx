#include "MqttBroker.h"

#include <thread>
#include "ErrorCode.h"
#include "Util.h"
#include "AES.h"
#include "Log.h"
#include "Wifi.h"
#include "Led.h"

#define DEBUG_MQTT_PUB 0

// http://username:password@host:port/path

// A list of client, held in memory
struct client *s_clients = NULL;

// A list of subscription, held in memory
struct sub *s_subs = NULL;

// A list of will topic & message, held in memory
struct will *s_wills = NULL;

#if 0
// Wildcard(#) support version
int _mg_strcmp(const struct mg_str str1, const struct mg_str str2) {
	size_t i = 0;
	while (i < str1.len && i < str2.len) {
		int c1 = str1.ptr[i];
		int c2 = str2.ptr[i];
		//printf("c2=%x\n",c2);
		if (c2 == '#') return 0;
		if (c1 < c2) return -1;
		if (c1 > c2) return 1;
		i++;
	}
	if (i < str1.len) return 1;
	if (i < str2.len) return -1;
	return 0;
}
#endif

// Wildcard(#/+) support version
int _mg_strcmp(const struct mg_str str1, const struct mg_str str2)
{
	size_t i1 = 0;
	size_t i2 = 0;
	while (i1 < str1.len && i2 < str2.len)
	{
		int c1 = str1.ptr[i1];
		int c2 = str2.ptr[i2];
		// printf("c1=%c c2=%c\n",c1, c2);

		// str2=[/hoge/#]
		if (c2 == '#')
			return 0;

		// str2=[/hoge/+/123]
		// Search next slash
		if (c2 == '+')
		{
			// str1=[/hoge//123]
			// str2=[/hoge/+/123]
			if (c1 == '/')
			{
				i2++;
				// str1=[/hoge/123/123]
				// str2=[/hoge/+/123]
			}
			else
			{
				for (i1 = i1; i1 + 1 < str1.len; i1++)
				{
					int c3 = str1.ptr[i1 + 1];
					// printf("i1=%ld c3=%c\n", i1, c3);
					if (c3 == '/')
						break;
				}
				i1++;
				i2++;
			}
		}
		else
		{
			if (c1 < c2)
				return -1;
			if (c1 > c2)
				return 1;
			i1++;
			i2++;
		}
	}
	if (i1 < str1.len)
		return 1;
	if (i2 < str2.len)
		return -1;
	return 0;
}

// void _mg_mqtt_dump(char *tag, struct mg_mqtt_message *msg)
// {
// 	unsigned char *buf = (unsigned char *)msg->dgram.ptr;
// 	LOGI("%s=%x %x", tag, buf[0], buf[1]);
// 	int length = buf[1] + 2;
// 	ESP_LOG_BUFFER_HEXDUMP(tag, buf, length, ESP_LOG_INFO);
// }

#define WILL_FLAG 0x04
#define WILL_QOS 0x18
#define WILL_RETAIN 0x20

int _mg_mqtt_parse_header(struct mg_mqtt_message *msg, struct mg_str *client, struct mg_str *topic,
						  struct mg_str *payload, uint8_t *qos, uint8_t *retain)
{
	client->len = 0;
	topic->len = 0;
	payload->len = 0;
	unsigned char *buf = (unsigned char *)msg->dgram.ptr;
	int Protocol_Name_length = buf[2] << 8 | buf[3];
	int Connect_Flags_position = Protocol_Name_length + 5;
	uint8_t Connect_Flags = buf[Connect_Flags_position];
	LOGD("Connect_Flags=%x", Connect_Flags);
	uint8_t Will_Flag = (Connect_Flags & WILL_FLAG) >> 2;
	*qos = (Connect_Flags & WILL_QOS) >> 3;
	*retain = (Connect_Flags & WILL_RETAIN) >> 5;
	LOGD("Will_Flag=%d *qos=%x *retain=%x", Will_Flag, *qos, *retain);
	client->len = buf[Connect_Flags_position + 3] << 8 | buf[Connect_Flags_position + 4];
	client->ptr = (char *)&buf[Connect_Flags_position + 5];
	LOGD("client->len=%d", client->len);
	if (Will_Flag == 0)
		return 0;

#if 0
	int Client_Identifier_length = buf[Connect_Flags_position+3] << 8 | buf[Connect_Flags_position+4];
	LOGD("Client_Identifier_length=%d", Client_Identifier_length);
	int Will_Topic_position = Protocol_Name_length + Client_Identifier_length + 10;
#endif

	int Will_Topic_position = Protocol_Name_length + client->len + 10;
	topic->len = buf[Will_Topic_position] << 8 | buf[Will_Topic_position + 1];
	topic->ptr = (char *)&(buf[Will_Topic_position]) + 2;
	LOGD("topic->len=%d topic->ptr=[%.*s]", topic->len, topic->len, topic->ptr);
	int Will_Payload_position = Will_Topic_position + topic->len + 2;
	payload->len = buf[Will_Payload_position] << 8 | buf[Will_Payload_position + 1];
	payload->ptr = (char *)&(buf[Will_Payload_position]) + 2;
	LOGD("payload->len=%d payload->ptr=[%.*s]", payload->len, payload->len, payload->ptr);
	return 1;
}

int _mg_mqtt_status()
{
	for (struct client *client = s_clients; client != NULL; client = client->next)
	{
		LOGI("CLIENT(ALL) %p [%.*s]", client->c->fd, (int)client->cid.len, client->cid.ptr);
		for (struct will *will = s_wills; will != NULL; will = will->next)
		{
			if (client->c != will->c)
				continue;
			LOGI("WILL(ALL) %p [%.*s] [%.*s] %d %d",
				 will->c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
		{
			if (client->c != sub->c)
				continue;
			LOGI("SUB(ALL) %p [%.*s]", sub->c->fd, (int)sub->topic.len, sub->topic.ptr);
		}
	}

#if 0
	for (struct will *will = s_wills; will != NULL; will = will->next) {
		LOGI("WILL(ALL) %p [%.*s] [%.*s] %d %d", 
		will->c->fd, (int) will->topic.len, will->topic.ptr, (int) will->payload.len, will->payload.ptr, will->qos, will->retain);
	}
#endif

#if 0
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next) {
		LOGI("SUB(ALL) %p [%.*s]", sub->c->fd, (int) sub->topic.len, sub->topic.ptr);
	}
#endif

	return 0;
}

static size_t mg_mqtt_next_topic(struct mg_mqtt_message *msg,
								 struct mg_str *topic, uint8_t *qos,
								 size_t pos)
{
	unsigned char *buf = (unsigned char *)msg->dgram.ptr + pos;
	size_t new_pos;
	if (pos >= msg->dgram.len)
		return 0;

	topic->len = (size_t)(((unsigned)buf[0]) << 8 | buf[1]);
	topic->ptr = (char *)buf + 2;
	new_pos = pos + 2 + topic->len + (qos == NULL ? 0 : 1);
	if ((size_t)new_pos > msg->dgram.len)
		return 0;
	if (qos != NULL)
		*qos = buf[2 + topic->len];
	return new_pos;
}

size_t mg_mqtt_next_sub(struct mg_mqtt_message *msg, struct mg_str *topic,
						uint8_t *qos, size_t pos)
{
	uint8_t tmp;
	return mg_mqtt_next_topic(msg, topic, qos == NULL ? &tmp : qos, pos);
}

size_t mg_mqtt_next_unsub(struct mg_mqtt_message *msg, struct mg_str *topic,
						  size_t pos)
{
	return mg_mqtt_next_topic(msg, topic, NULL, pos);
}

#if MG_ENABLE_MBEDTLS
static const char *s_ssl_cert =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIBCTCBsAIJAK9wbIDkHnAoMAoGCCqGSM49BAMCMA0xCzAJBgNVBAYTAklFMB4X\n"
	"DTIzMDEyOTIxMjEzOFoXDTMzMDEyNjIxMjEzOFowDTELMAkGA1UEBhMCSUUwWTAT\n"
	"BgcqhkjOPQIBBggqhkjOPQMBBwNCAARzSQS5OHd17lUeNI+6kp9WYu0cxuEIi/JT\n"
	"jphbCmdJD1cUvhmzM9/phvJT9ka10Z9toZhgnBq0o0xfTQ4jC1vwMAoGCCqGSM49\n"
	"BAMCA0gAMEUCIQCe0T2E0GOiVe9KwvIEPeX1J1J0T7TNacgR0Ya33HV9VgIgNvdn\n"
	"aEWiBp1xshs4iz6WbpxrS1IHucrqkZuJLfNZGZI=\n"
	"-----END CERTIFICATE-----\n";

static const char *s_ssl_key =
	"-----BEGIN EC PRIVATE KEY-----\n"
	"MHcCAQEEICBz3HOkQLPBDtdknqC7k1PNsWj6HfhyNB5MenfjmqiooAoGCCqGSM49\n"
	"AwEHoUQDQgAEc0kEuTh3de5VHjSPupKfVmLtHMbhCIvyU46YWwpnSQ9XFL4ZszPf\n"
	"6YbyU/ZGtdGfbaGYYJwatKNMX00OIwtb8A==\n"
	"-----END EC PRIVATE KEY-----\n";
#endif // MG_ENABLE_MBEDTLS

// Event handler function
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
	MqttBroker *mqttBroker = (MqttBroker *)fn_data;
	mqttBroker->mtx.lock();
#if MG_ENABLE_MBEDTLS
	if (ev == MG_EV_ACCEPT && fn_data != NULL)
	{
		struct mg_tls_opts opts = {
			.cert = s_ssl_cert,
			.certkey = s_ssl_key};
		mg_tls_init(c, &opts);
	}
#endif // MG_ENABLE_MBEDTLS
	if (ev == MG_EV_MQTT_CMD)
	{
		struct mg_mqtt_message *mm = (struct mg_mqtt_message *)ev_data;
		LOGV("cmd %d qos %d", mm->cmd, mm->qos);
		switch (mm->cmd)
		{
		case MQTT_CMD_CONNECT:
		{
			LOGI("CONNECT");
			LOGD("total_size(MALLOC_CAP_8BIT):%d", heap_caps_get_total_size(MALLOC_CAP_8BIT));
			LOGD("total_size(MALLOC_CAP_32BIT):%d", heap_caps_get_total_size(MALLOC_CAP_32BIT));
			LOGD("free_size(MALLOC_CAP_8BIT):%d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
			LOGD("free_size(MALLOC_CAP_32BIT):%d", heap_caps_get_free_size(MALLOC_CAP_32BIT));

			// Parse the header to retrieve will information.
			//_mg_mqtt_dump("CONNECT", mm);
			struct mg_str cid;
			struct mg_str topic;
			struct mg_str payload;
			uint8_t qos;
			uint8_t retain;
			int willFlag = _mg_mqtt_parse_header(mm, &cid, &topic, &payload, &qos, &retain);
			LOGD("cid=[%.*s] willFlag=%d", cid.len, cid.ptr, willFlag);

			// Set tcp socket keepalive options
			// timeout = keepidle+(keepcnt*keepintvl)
			// timeout = 60+(1*10)=70
			int keepAlive = 1;
			setsockopt((int)c->fd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
			int keepIdle = 60; // default is 7200 Sec
			setsockopt((int)c->fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
			int keepInterval = 10; // default is 75 Sec
			setsockopt((int)c->fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
			int keepCount = 1; // default is 9 count
			setsockopt((int)c->fd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

#if 0
			// Client connects. Add to the client-id list
			struct client *client = calloc(1, sizeof(*client));
			client->c = c;
			client->cid = mg_strdup(cid);
			LIST_ADD_HEAD(struct client, &s_clients, client);
			LOGD("CLIENT ADD %p [%.*s]", c->fd, (int) client->cid.len, client->cid.ptr);
			LOGI("CLIENT ADD %p", client);
#endif

			// Client connects. Add to the will list
			if (willFlag == 1)
			{
				struct will *will = (struct will *)calloc(1, sizeof(*will));
				will->c = c;
				will->topic = mg_strdup(topic);
				will->payload = mg_strdup(payload);
				will->qos = qos;
				will->retain = retain;
				LIST_ADD_HEAD(struct will, &s_wills, will);
				LOGD("WILL ADD %p [%.*s] [%.*s] %d %d",
					 c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
			}
			_mg_mqtt_status();

			// Client connects. Return success, do not check user/password
			uint8_t response[] = {0, 0};
			mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
			mg_send(c, response, sizeof(response));
			break;
		}
		case MQTT_CMD_PUBREC: // MQTT5: 3.5.2-1 TODO(): variable header rc
		{
			uint16_t id = mg_htons(mm->id);
			mg_mqtt_send_header(c, MQTT_CMD_PUBREL, 2, sizeof(id));
			mg_send(c, &id, sizeof(id)); // MQTT5 3.6.1-1, flags = 2
			break;
		}
		case MQTT_CMD_PUBREL: // MQTT5: 3.6.2-1 TODO(): variable header rc
		{
			uint16_t id = mg_htons(mm->id);
			mg_mqtt_send_header(c, MQTT_CMD_PUBCOMP, 0, sizeof(id));
			mg_send(c, &id, sizeof(id));
			break;
		}
		case MQTT_CMD_SUBSCRIBE:
		{
			// Client subscribe
			LOGI("MQTT_CMD_SUBSCRIBE");
			int pos = 4; // Initial topic offset, where ID ends
			uint8_t qos, resp[256];
			struct mg_str topic;
			int num_topics = 0;
			while ((pos = mg_mqtt_next_sub(mm, &topic, &qos, pos)) > 0)
			{
				struct sub *sub = (struct sub *)calloc(1, sizeof(*sub));
				sub->c = c;
				sub->topic = mg_strdup(topic);
				sub->qos = qos;
				LIST_ADD_HEAD(struct sub, &s_subs, sub);
				LOGI("SUB ADD %p [%.*s]", c->fd, (int)sub->topic.len, sub->topic.ptr);
				resp[num_topics++] = qos;
			}
			mg_mqtt_send_header(c, MQTT_CMD_SUBACK, 0, num_topics + 2);
			uint16_t id = mg_htons(mm->id);
			mg_send(c, &id, 2);
			mg_send(c, resp, num_topics);
			_mg_mqtt_status();
			break;
		}
		case MQTT_CMD_UNSUBSCRIBE:
		{
			// Client unsubscribes. Remove from the subscription list
			LOGI("MQTT_CMD_UNSUBSCRIBE");
			//_mg_mqtt_dump("UNSUBSCRIBE", mm);
			int pos = 4; // Initial topic offset, where ID ends
			struct mg_str topic;
			while ((pos = mg_mqtt_next_unsub(mm, &topic, pos)) > 0)
			{
				LOGI("UNSUB %p [%.*s]", c->fd, (int)topic.len, topic.ptr);
				// Remove from the subscription list
				for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
				{
					LOGI("SUB[b] %p [%.*s]", sub->c->fd, (int)sub->topic.len, sub->topic.ptr);
				}
				for (struct sub *next, *sub = s_subs; sub != NULL; sub = next)
				{
					next = sub->next;
					LOGD("c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
					if (c != sub->c)
						continue;
					if (strncmp(topic.ptr, sub->topic.ptr, topic.len) != 0)
						continue;
					LOGI("DELETE SUB %p [%.*s]", c->fd, (int)sub->topic.len, sub->topic.ptr);
					free((void *)sub->topic.ptr);
					LIST_DELETE(struct sub, &s_subs, sub);
					free(sub);
				}
				for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
				{
					LOGI("SUB[a] %p [%.*s]", sub->c->fd, (int)sub->topic.len, sub->topic.ptr);
				}
			}
			_mg_mqtt_status();
			break;
		}
		case MQTT_CMD_PUBLISH:
		{
			// Client published message. Push to all subscribed channels
			if (isascii(mm->data.ptr[0]))
			{
				// LOGI("mm->data.ptr[0]=0x%x", mm->data.ptr[0]);
				// LOGI("PUB %p [%.*s] -> [%.*s]", c->fd, (int)mm->data.len, mm->data.ptr, (int)mm->topic.len, mm->topic.ptr);
				mqttBroker->on_data(mm->topic.ptr, (int)mm->topic.len, (char *)mm->data.ptr, (int)mm->data.len);
			}
			else
			{
				LOGI("mm->data.ptr[0]=0x%x", mm->data.ptr[0]);
				LOGI("PUB %p [BINARY] -> [%.*s]", c->fd, (int)mm->topic.len, mm->topic.ptr);
			}
			for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
			{
				if (mg_match(mm->topic, sub->topic, NULL))
				{
					struct mg_mqtt_opts pub_opts;
					memset(&pub_opts, 0, sizeof(pub_opts));
					pub_opts.topic = mm->topic;
					pub_opts.message = mm->data;
					pub_opts.qos = 1;
					pub_opts.retain = false;
					mg_mqtt_pub(sub->c, &pub_opts);
				}
			}
			break;
		}
		case MQTT_CMD_PINGREQ:
		{
			LOGV("PINGREQ %p", c->fd);
			mg_mqtt_pong(c); // Send PINGRESP
			break;
		}
		}
	}
	else if (ev == MG_EV_CLOSE)
	{
		LOGI("MG_EV_CLOSE %p", c->fd);

		LOGD("total_size(MALLOC_CAP_8BIT):%d", heap_caps_get_total_size(MALLOC_CAP_8BIT));
		LOGD("total_size(MALLOC_CAP_32BIT):%d", heap_caps_get_total_size(MALLOC_CAP_32BIT));
		LOGI("free_size(MALLOC_CAP_8BIT):%d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
		LOGI("free_size(MALLOC_CAP_32BIT):%d", heap_caps_get_free_size(MALLOC_CAP_32BIT));

		// Client disconnects. Remove from the client-id list
		for (struct client *client = s_clients; client != NULL; client = client->next)
		{
			LOGD("CLIENT(b) %p [%.*s]", client->c->fd, (int)client->cid.len, client->cid.ptr);
		}
		for (struct client *next, *client = s_clients; client != NULL; client = next)
		{
			next = client->next;
			LOGD("c->fd=%p client->c->fd=%p", c->fd, client->c->fd);
			if (c != client->c)
				continue;
			LOGD("CLIENT DEL %p [%.*s]", c->fd, (int)client->cid.len, client->cid.ptr);
			LOGI("CLIENT DEL %p", client);
			free((void *)client->cid.ptr);
			LIST_DELETE(struct client, &s_clients, client);
			free(client);
		}
		for (struct client *client = s_clients; client != NULL; client = client->next)
		{
			LOGD("CLIENT(a) %p [%.*s]", client->c->fd, (int)client->cid.len, client->cid.ptr);
		}

		// Client disconnects. Remove from the subscription list
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
		{
			LOGD("SUB[b] %p [%.*s]", sub->c->fd, (int)sub->topic.len, sub->topic.ptr);
		}
		for (struct sub *next, *sub = s_subs; sub != NULL; sub = next)
		{
			next = sub->next;
			LOGD("c->fd=%p sub->c->fd=%p", c->fd, sub->c->fd);
			if (c != sub->c)
				continue;
			LOGD("SUB DEL %p [%.*s]", c->fd, (int)sub->topic.len, sub->topic.ptr);
			free((void *)sub->topic.ptr);
			LIST_DELETE(struct sub, &s_subs, sub);
			free(sub);
		}

		// Judgment to send will
		for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
		{
			LOGD("SUB[a] %p [%.*s]", sub->c->fd, (int)sub->topic.len, sub->topic.ptr);
			for (struct will *will = s_wills; will != NULL; will = will->next)
			{
				LOGD("WILL(ALL) %p [%.*s] [%.*s] %d %d",
					 will->c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
				// if (c == will->c) continue;
				if (sub->c == will->c)
					continue;
				LOGD("WILL(CMP) %p [%.*s] [%.*s] %d %d",
					 will->c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
				if (_mg_strcmp(will->topic, sub->topic) != 0)
					continue;
				struct mg_mqtt_opts pub_opts;
				memset(&pub_opts, 0, sizeof(pub_opts));
				pub_opts.topic = will->topic;
				pub_opts.message = will->payload;
				pub_opts.qos = 1;
				pub_opts.retain = false;
				mg_mqtt_pub(sub->c, &pub_opts);
			}
		}

		// Client disconnects. Remove from the will list
		for (struct will *will = s_wills; will != NULL; will = will->next)
		{
			LOGD("WILL[b] %p [%.*s] [%.*s] %d %d",
				 will->c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		for (struct will *next, *will = s_wills; will != NULL; will = next)
		{
			next = will->next;
			LOGD("WILL %p [%.*s] [%.*s] %d %d",
				 c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
			if (c != will->c)
				continue;
			LOGD("WILL DEL %p [%.*s] [%.*s] %d %d",
				 c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
			free((void *)will->topic.ptr);
			LIST_DELETE(struct will, &s_wills, will);
			free(will);
		}
		for (struct will *will = s_wills; will != NULL; will = will->next)
		{
			LOGD("WILL[a] %p [%.*s] [%.*s] %d %d",
				 will->c->fd, (int)will->topic.len, will->topic.ptr, (int)will->payload.len, will->payload.ptr, will->qos, will->retain);
		}
		_mg_mqtt_status();
	} // MG_EV_CLOSE
	mqttBroker->mtx.unlock();
	(void)fn_data;
}

static string GenPassMqttBroker()
{
	string mac = Wifi::GetMacAddress();
	uint8_t key[16] = {0};
	string keyS = "RANGDONGRALSMART";
	memcpy(key, keyS.c_str(), 16);

	string plainText = "2804" + mac;
	uint8_t plt[16] = {0};
	memcpy(plt, plainText.c_str(), 16);

	AES aes(AESKeyLength::AES_128);

	unsigned char *outAes = aes.EncryptECB(plt, 32, key);
	unsigned char out[32] = {0};
	for (int i = 0; i < 32; i++)
	{
		out[i] = outAes[i];
	}
	char hexString[sizeof(out) * 2 + 1];
	sprintf(hexString, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7],
			out[8], out[9], out[10], out[11], out[12], out[13], out[14], out[15],
			out[16], out[17], out[18], out[19], out[20], out[21], out[22], out[23],
			out[24], out[25], out[26], out[27], out[28], out[29], out[30], out[31]);

	std::string b(hexString);
	delete[] outAes; // Remember to free the allocated memory
	return b;
}

static void MqttBrokerThread(void *pvParameters)
{
	/* Starting Broker */
	LOGI("MQTT broker started on Mongoose v%s", MG_VERSION);
	string str2 = GenPassMqttBroker();
#if MG_ENABLE_MBEDTLS
	string url = "mqtts://RD:" + GenPassMqttBroker() + "@0.0.0.0:8883";
#else
	string url = "mqtt://RD:" + GenPassMqttBroker() + "@0.0.0.0";
#endif // MG_ENABLE_MBEDTLS

	const char *s_listen_on = (const char *)url.c_str();
	LOGI("s_listen_on: %s", s_listen_on);

	struct mg_mgr mgr;
	mg_log_set(MG_LL_ERROR); // Set to log level to LL_ERROR
	// mg_log_set("3"); // Set to log level to LL_DEBUG
	mg_mgr_init(&mgr);
	mg_mqtt_listen(&mgr, s_listen_on, fn, pvParameters); // Create MQTT listener
	// LOGI("Starting Mongoose v%s MQTT Server", MG_VERSION);

	/* Processing events */
	while (1)
	{
		mg_mgr_poll(&mgr, 0);
		vTaskDelay(5);
	}

	// Never reach here
	LOGI("finish");
	mg_mgr_free(&mgr);
	vTaskDelete(NULL);
}

MqttBroker::MqttBroker()
{
}

static void OnMessageThread(void *data);
void MqttBroker::init()
{
	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	queue = xQueueCreate(10, sizeof(on_message_t *));
	if (xTaskCreate(MqttBrokerThread, "MqttBroker", 10240, this, 10, NULL) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
	vTaskDelay(10);

	if (xTaskCreate(OnMessageThread, "OnMessageThread", 20480, this, 5, NULL) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
	vTaskDelay(10);
}

int MqttBroker::Connect(int timeout)
{
	OnConnect(true, false);
	return 0;
}

int MqttBroker::Subscribe(string topic)
{
	return 0;
}

int MqttBroker::Unsubscribe(string topic)
{
	return 0;
}

int MqttBroker::Publish(string topic, string payload)
{
	return Publish(topic, (char *)payload.c_str(), payload.length());
}

int MqttBroker::Publish(string topic, char *payload, int payloadLen)
{
	struct mg_str tp;
	tp.ptr = topic.c_str();
	tp.len = topic.length();
	struct mg_str pl;
	pl.ptr = payload;
	pl.len = payloadLen;

	mtx.lock();
	for (struct sub *sub = s_subs; sub != NULL; sub = sub->next)
	{
		if (_mg_strcmp(tp, sub->topic) != 0)
			continue;
		struct mg_mqtt_opts pub_opts;
		memset(&pub_opts, 0, sizeof(pub_opts));
		pub_opts.topic = tp;
		pub_opts.message = pl;
		pub_opts.qos = 1;
		pub_opts.retain = false;
		mg_mqtt_pub(sub->c, &pub_opts);
#if DEBUG_MQTT_PUB
		LOGI("Publish topic: %s, msg: %s", topic.c_str(), payload);
#endif
	}
	mtx.unlock();
	return 0;
}

void MqttBroker::ResubcribeAllTopic()
{
	for (auto &action : actionCallbacks)
	{
		Subscribe(action.getTopic());
	}
}

void MqttBroker::addActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType1, topic);
	actionCallbacks.push_back(actionCallback);
	Subscribe(topic);
}

void MqttBroker::addActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType2, topic);
	actionCallbacks.push_back(actionCallback);
	Subscribe(topic);
}

void MqttBroker::addActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType3, topic);
	actionCallbacks.push_back(actionCallback);
	Subscribe(topic);
}

void MqttBroker::addActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic)
{
	ActionCallback actionCallback(actionCallbackFuncType4, topic);
	actionCallbacks.push_back(actionCallback);
	Subscribe(topic);
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
	vector<string> retrieveList = Util::splitString(retrieveTopic, '/');
	vector<string> registerList = Util::splitString(registerTopic, '/');
	for (size_t i = 0; i < retrieveList.size(); i++)
	{
		if (registerList.size() < i)
			return CODE_ERROR;
		if (registerList.at(i) == "#")
			return CODE_OK; // OK
		if (registerList.at(i) == "+")
			continue;
		if (registerList.at(i) != retrieveList.at(i))
			return CODE_ERROR;
	}
	if (registerList.size() == retrieveList.size())
		return CODE_OK; // OK
	return CODE_ERROR;
}

int MqttBroker::findActionCallbackFuncFromTopic(string topic, ActionCallback **actionCallback)
{
	for (auto &action : actionCallbacks)
	{
		if (checkMqttTopic(topic, action.getTopic()) == CODE_OK)
		{
			*actionCallback = &action;
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

void MqttBroker::on_data(const char *topic, int topic_len, char *payload, int payload_len)
{
	LOGD("on_data topic: %s", topic);
	// on_message_t *on_message = (on_message_t *)malloc(sizeof(on_message_t));
	on_message_t *on_message = (on_message_t *)heap_caps_malloc_prefer(sizeof(on_message_t), 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
	if (on_message)
	{
		on_message->topic_len = topic_len;
		on_message->payload_len = payload_len;
		// on_message->topic = (char *)malloc(topic_len);
		on_message->topic = (char *)heap_caps_malloc_prefer(topic_len, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
		if (on_message->topic)
		{
			// on_message->payload = (char *)malloc(payload_len);
			on_message->payload = (char *)heap_caps_malloc_prefer(payload_len, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
			if (on_message->payload)
			{
				memcpy(on_message->topic, topic, topic_len);
				memcpy(on_message->payload, payload, payload_len);
				if (xQueueSend(queue, (void *)&on_message, (TickType_t)0) == pdPASS)
					return;
				free(on_message->payload);
			}
			free(on_message->topic);
		}
		free(on_message);
	}
}

static void OnMessageThread(void *data)
{
	LOGI("OnMessageThread Start");
	MqttBroker *mqttBroker = (MqttBroker *)data;
	on_message_t *on_message;
	while (1)
	{
		if (xQueueReceive(mqttBroker->queue, &on_message, (TickType_t)5) == pdPASS)
		{
			string topic = string(on_message->topic, on_message->topic_len);
			mqttBroker->OnMessage(topic, on_message->payload, on_message->payload_len);
			free(on_message->topic);
			free(on_message->payload);
			free(on_message);
		}
		else
			vTaskDelay(pdMS_TO_TICKS(200));
	}
	vTaskDelete(NULL);
}

void MqttBroker::OnMessage(string topic, char *payload, int payloadLen)
{
	ActionCallback *actionCallback;
	if (findActionCallbackFuncFromTopic(topic, &actionCallback) == CODE_OK)
	{
		if (actionCallback->getType() == 1)
		{
			string payloadStr = string(payload, payloadLen);
			LOGD("OnMessage topic: %s, payload: %s", topic.c_str(), payloadStr.c_str());
			actionCallback->actionCallbackFuncType1(topic, payloadStr);
		}
		else if (actionCallback->getType() == 2)
		{
			actionCallback->actionCallbackFuncType2(topic, payload, payloadLen);
		}
		else if (actionCallback->getType() == 3)
		{
			string payloadStr = string(payload, payloadLen);
			LOGD("OnMessage topic: %s, payload: %s", topic.c_str(), payloadStr.c_str());
			actionCallback->actionCallbackFuncType3(topic, payloadStr);
		}
		else if (actionCallback->getType() == 4)
		{
			actionCallback->actionCallbackFuncType4(topic, payload, payloadLen);
		}
		else
		{
			LOGW("Not found type: %d", actionCallback->getType());
		}
	}
	else
	{
		LOGW("Not found topic: %s", topic.c_str());
	}
}

bool MqttBroker::isConnected()
{
	return true;
}