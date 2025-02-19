#pragma once

using namespace std;

class ButtonSignal
{
private:
public:
	volatile bool isPressed;
	// bool isStartAP;
	// int startAPTimeCount;

	ButtonSignal();
	~ButtonSignal();
	void init();
	void OnPress();
	void OnRelease();
	bool GetStatus();
};

extern ButtonSignal *buttonSignal;
