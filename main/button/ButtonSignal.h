#pragma once

using namespace std;

class ButtonSignal
{
private:
public:
	volatile bool isPressed;
	// bool isStartAP;
	// int startAPTimeCount;
	static ButtonSignal *getInstance();

	ButtonSignal();
	~ButtonSignal();
	void init();
	void OnPress();
	void OnRelease();
	bool GetStatus();
};
