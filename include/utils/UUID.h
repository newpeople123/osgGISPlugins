#include <random>
#ifndef OSG_GIS_PLUGINS_UUID
#define OSG_GIS_PLUGINS_UUID 1

std::string generateUUID() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dis(0, 15);

	const std::string hexChars = "0123456789abcdef";

	std::string uuid;
	for (int i = 0; i < 32; ++i) {
		if (i == 8 || i == 12 || i == 16 || i == 20) {
			uuid += "-";
		}
		else if (i == 16) {
			uuid += "4";
		}
		else if (i == 12) {
			uuid += hexChars[3 & dis(gen)];
		}
		else {
			uuid += hexChars[15 & dis(gen)];
		}
	}

	return uuid;
}
#endif // !OSG_GIS_PLUGINS_UUID
