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

bool Ota::CheckNeedUpdate(string fwVersion)
{
	// TODO: check current version
	return true;
}

int Ota::Update(string fwUrl, string fwChecksumAlgorithm, string fwChecksum)
{
	// Update opkg
	return 0;
}

int Ota::Update(int fwSize, string fwChecksumAlgorithm, string fwChecksum)
{
	return -1;
}

int Ota::UpdateChunk(int chunkId, uint8_t *data, int dataLen)
{
	return -1;
}

int Ota::UpdateFinish()
{
	return -1;
}
