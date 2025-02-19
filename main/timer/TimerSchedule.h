#pragma once

#include <vector>
#include <mutex>
#include <functional>
#include "Rule.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

using namespace std;

typedef function<void()> TimerCallbackFunc;

class Timer
{
private:
	int index;
	int time;

public:
	TimerCallbackFunc timerCallbackFunc;
	Rule *rule;
	Timer(int index, int time, TimerCallbackFunc timerCallbackFunc);
	Timer(int index, int time, Rule *rule);

	int GetIndex();
	bool IsAtTime(int time);
};

class TimerSchedule
{
private:
	int index;

public:
	mutex mtx;
	QueueHandle_t queue;

	TimerSchedule();
	~TimerSchedule();
	vector<Timer *> timerList;

	void init();
	int RegisterTimer(string timer, TimerCallbackFunc timerCallbackFunc);
	int RegisterTimer(int time, TimerCallbackFunc timerCallbackFunc);
	int RegisterTimer(string timer, Rule *rule);
	int RegisterTimer(int time, Rule *rule);
	int UnregisterTimer(int index);
};

extern TimerSchedule *timerSchedule;
