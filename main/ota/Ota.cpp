#include "Ota.h"

extern "C" void ota_init();
extern "C" void ota_start(const char *name, const char *url, const char *sum, int timeout);

void Ota::init()
{
	ota_init();
}

bool Ota::startOta(string name, string url, string sum)
{
	ota_start(name.c_str(), url.c_str(), sum.c_str(), 10000);
	return 0;
}
