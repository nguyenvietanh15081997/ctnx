#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"

using namespace std;

class Uart
{
private:
	uart_port_t num;
	int txPin;
	int rxPin;
	int baudrate;

	QueueHandle_t queue;

public:
	Uart(uart_port_t num, int txPin, int rxPin, int baudrate);

	uart_port_t getNum();
	QueueHandle_t getQueue();
	void init();

	int Open(int baudrate);
	int Close();
	int ChangeBaudrate(int baudrate);
	// ssize_t Read(void *buf, size_t count);
	ssize_t Write(const void *buf, size_t count);

	virtual int OnMessage(unsigned char *data, int len);
};