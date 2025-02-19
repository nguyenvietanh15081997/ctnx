#pragma once

#include <string>

using namespace std;

namespace Ota
{
	void init();
	bool startOta(string name, string url, string sum);
}