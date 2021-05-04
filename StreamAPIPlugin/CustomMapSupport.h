#pragma once
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "pluginsdk.lib")

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include "cpp-httplib/httplib.h"

#include <filesystem>
#include <unordered_map>

#include "nlohmann/json.hpp"

const int READ_FILE_CHUNK_SIZE = 1000000;

struct CustomMapFile {
	std::string filename;
	std::string fullHash;
	unsigned long long updateTimestamp;
};

struct CustomMap {
	std::string workshopId;
	std::string author;
	std::string title;
	std::string description;
	unsigned long long published;
	unsigned long long lastUpdated;

	std::vector<CustomMapFile> mapFileHistory;
};

void from_json(const nlohmann::json& j, CustomMapFile& mapFile);
void from_json(const nlohmann::json& j, CustomMap& map);

class CustomMapSupport
{
public:
	CustomMapSupport();
	void init(bool isSteamVersion, std::filesystem::path mapsJsonPath);
	void updateMap(std::wstring mapName);
	std::string toString();

private:
	void loadMapsFromJson(nlohmann::json& j);

	void updateByMap(CustomMap& map);
	bool updateByWorkshopId(std::string workshopId);
	bool updateByFilename(std::string filename, bool forced);
	bool updateByHash(std::filesystem::path path);

	std::filesystem::path findAbsolutePath(std::filesystem::path path);

	unsigned long long lastModified;

	std::filesystem::path mapsJsonPath;
	std::unordered_map<std::string, CustomMap> workshopIdToMap;
	std::unordered_map<std::string, CustomMap> hashToAllMaps;
	std::unordered_map<std::string, CustomMap> filenameToAllMaps;

	std::string loadedMapDetails;
	std::atomic<bool> initialized;

	std::filesystem::path standardMapsDir;
	std::vector<std::filesystem::path> customMapsDirs;
	std::filesystem::path workshopDir;
};

