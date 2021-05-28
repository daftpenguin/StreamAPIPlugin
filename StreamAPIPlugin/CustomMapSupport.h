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
const std::wstring DEFAULT_STEAM_INSTALLATION_PATH = L"C:\\Program Files (x86)\\Steam";
const std::wstring DEFAULT_EPIC_INSTALLATION_PATH = L"C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";

enum class CustomMapType {
	WORKSHOP,
	LETHAMYR,
};

struct WorkshopMapFile {
	std::string filename;
	std::string fullHash;
	unsigned long long updateTimestamp;
};

class CustomMap {
public:
	CustomMapType type;
	std::string title;
	std::string author;
	std::string description;

	// For Workshop based maps
	std::string workshopId;
	unsigned long long published;
	unsigned long long lastUpdated;
	std::vector<WorkshopMapFile> mapFileHistory;

	// For Lethamyr based maps
	std::string link;
	std::string filename;
	std::string fullHash;
};

void from_json(const nlohmann::json& j, WorkshopMapFile& mapFile);
void from_json(const nlohmann::json& j, CustomMap& map);

class CustomMapSupport
{
public:
	CustomMapSupport();
	void init(bool isSteamVersion, std::filesystem::path mapsJsonPath);
	void updateMap(std::wstring mapName);
	std::string getCustomMap();
	std::string getMap();

private:
	void loadMapsFromJson(nlohmann::json& j);

	bool updateCustomMap(std::wstring mapName);
	void updateByMap(CustomMap& map);
	int updateIfCookedPCMap(std::wstring mapName);
	bool updateByWorkshopId(std::string workshopId);
	bool updateByFilename(std::string filename, bool forced);
	bool updateByHash(std::filesystem::path path);

	std::filesystem::path findAbsolutePath(std::filesystem::path path);

	unsigned long long lastModified;

	std::filesystem::path mapsJsonPath;
	std::unordered_map<std::string, CustomMap> workshopIdToMap; // For Leth maps, this will be title to map
	std::unordered_map<std::string, CustomMap> hashToAllMaps;
	std::unordered_map<std::string, CustomMap> filenameToAllMaps;

	std::string loadedMapDetails;
	std::string loadedCustomMapDetails;
	std::atomic<bool> initialized;

	std::filesystem::path standardMapsDir;
	std::vector<std::filesystem::path> customMapsDirs;
	std::filesystem::path workshopDir;
};

