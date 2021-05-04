#include "CustomMapSupport.h"
#include "StreamAPIPlugin.h"

#include <fstream>
#include <filesystem>
#include <openssl/md5.h>

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

CustomMapSupport::CustomMapSupport() : lastModified(0), mapsJsonPath(mapsJsonPath), loadedMapDetails("No map loaded yet"), initialized(false) {}

void CustomMapSupport::init(std::filesystem::path mapsJsonPath)
{
	// TODO: Load the local maps.json in main thread, and load the server copy in a different one? Lock data when parsing json.

	// We now have a maps.json which has a mapping of workshop ids to map struct
	// Load each map struct into mapping from hash to earliest map struct (to handle collisions)
	// Additionally, maintain a mapping from filename to map struct

	// Next we load all workshop maps and identify their maps, and store them as mapping from filepath to map struct
	// If steam, check workshop maps and leverage their workshop ids
	// For both epic and steam, check mods folder and use their filenames and hashes
	// For both, check the file sizes of the default maps

	// Load the local copy of maps.json
	bool downloadUpdate = true;
	json mapsJson;
	lastModified = 0;
	if (fs::exists(mapsJsonPath)) {
		ifstream fin(mapsJsonPath);
		if (fin.fail()) {
			_globalCvarManager->log("CustomMapSupport: failed to load data from local maps.json. Attempting to download latest version.");
		}
		else {
			stringstream ss;
			ss << fin.rdbuf();
			try {
				mapsJson = json::parse(ss.str());
				if (mapsJson.find("lastModified") == mapsJson.end() || mapsJson.find("maps") == mapsJson.end()) {
					_globalCvarManager->log("CustomMapSupport: failed to get maps from local maps.json. Attempting to download latest version.");
				}
				else {
					mapsJson.at("lastModified").get_to(lastModified);
				}
			}
			catch (...) {
				_globalCvarManager->log("CustomMapSupport: failed to parse data from local maps.json. Attempting to download latest version.");
			}
		}
	}
	else {
		_globalCvarManager->log("CustomMapSupport: no local maps.json file. This file should have been provided with the installation. Attempting to download latest version.");
	}


	// Check if there's an update from https://raw.githubusercontent.com/daftpenguin/RocketLeagueWorkshopScraper/main/releases/meta.json
	httplib::Client cli("https://raw.githubusercontent.com");
	if (auto res = cli.Get("/daftpenguin/RocketLeagueWorkshopScraper/main/releases/meta.json")) {
		if (res->status == 200) {
			try {
				json j = json::parse(res->body);
				if (j.find("lastModified") == j.end()) {
					_globalCvarManager->log("CustomMapSupport: failed to get lastModified from meta.json. Using old maps.json for map lookups.");
				}
				else {
					auto lastModified = j["lastModified"].get<unsigned long long>();
					if (lastModified <= this->lastModified) {
						downloadUpdate = false;
					}
				}
			}
			catch (...) {
				_globalCvarManager->log("CustomMapSupport: failed to parse meta.json. Using old maps.json for map lookups.");
			}
		}
		else {
			_globalCvarManager->log("CustomMapSupport: failed to retrieve meta.json from server: " + res->body);
		}
	}

	// If there's an update, get https://raw.githubusercontent.com/daftpenguin/RocketLeagueWorkshopScraper/main/releases/maps.json and save to mapsJsonPath
	if (downloadUpdate) {
		if (auto res = cli.Get("/daftpenguin/RocketLeagueWorkshopScraper/main/releases/maps.json")) {
			if (res->status == 200) {
				try {
					json j = json::parse(res->body);
					if (j.find("maps") == j.end() || j.find("lastModified") == j.end()) {
						_globalCvarManager->log("CustomMapSupport: failed to find maps key in downloaded maps.json. Using old maps.json for map lookups.");
					}
					else {
						mapsJson = j;

						ofstream fout(mapsJsonPath);
						if (fout.fail()) {
							_globalCvarManager->log("CustomMapSupport: new maps.json downloaded, but failed to save to file: " + string(strerror(errno)));
						}
						else {
							fout << res->body << endl;
						}
						fout.close();
					}
				}
				catch (...) {
					_globalCvarManager->log("CustomMapSupport: failed to parse maps.json. Using old maps.json for map lookups.");
				}
			}
			else {
				_globalCvarManager->log("CustomMapSupport: failed to retrieve maps.json from server: " + res->body);
			}
		}
	}

	// Complete the loading of the data from the json
	loadMapsFromJson(mapsJson);
	initialized = true;
}

unordered_set<wstring> ignoredMapNames({
	L"ARC_P",
	L"ARC_Standard_P",
	L"BB_P",
	L"Beach_Night_P",
	L"Beach_P",
	L"CHN_Stadium_Day_P",
	L"CHN_Stadium_P",
	L"CS_Day_P",
	L"CS_HW_P",
	L"CS_P",
	L"EuroStadium_Night_P",
	L"EuroStadium_P",
	L"EuroStadium_Rainy_P",
	L"EuroStadium_SnowNight_P",
	L"Farm_Night_P",
	L"Farm_P",
	L"Haunted_TrainStation_P",
	L"HoopsStadium_P",
	L"MENU_Main_p",
	L"Music_P",
	L"NeoTokyo_P",
	L"NeoTokyo_Standard_P",
	L"Park_Night_P",
	L"Park_P",
	L"Park_Rainy_P",
	L"ShatterShot_P",
	L"Stadium_Day_P",
	L"Stadium_Foggy_P",
	L"Stadium_P",
	L"Stadium_Race_Day_P",
	L"Stadium_Winter_P",
	L"ThrowbackHockey_P",
	L"ThrowbackStadium_P",
	L"TrainStation_Dawn_P",
	L"TrainStation_Night_P",
	L"TrainStation_P",
	L"TrainStation_Spooky_P",
	L"Underwater_P",
	L"UtopiaStadium_Dusk_P",
	L"UtopiaStadium_P",
	L"UtopiaStadium_Snow_P",
	L"Wasteland_Art_Night_P",
	L"Wasteland_Art_Night_S_P",
	L"Wasteland_Art_P",
	L"Wasteland_Art_S_P",
	L"Wasteland_Night_P",
	L"Wasteland_Night_S_P",
	L"Wasteland_Night_VFX_P",
	L"Wasteland_P",
	L"Wasteland_S_P"
	});

unordered_set<wstring> mapReplacementMaps({
	L"Labs_Basin_P.upk",
	L"Labs_CirclePillars_P.upk",
	L"Labs_Corridor_P.upk",
	L"Labs_Cosmic_P.upk",
	L"Labs_Cosmic_V4_P.upk",
	L"Labs_DoubleGoal_P.upk",
	L"Labs_DoubleGoal_V2_P.upk",
	L"Labs_Galleon_P.upk",
	L"Labs_Octagon_02_P.upk",
	L"Labs_Octagon_P.upk",
	L"Labs_Underpass_P.upk",
	L"Labs_Underpass_v0_p.upk",
	L"Labs_Utopia_P.upk"
	});

void CustomMapSupport::updateMap(std::wstring mapName)
{
	_globalCvarManager->log(L"CustomMapSupport: received map update for: " + mapName);
	if (!initialized) {
		loadedMapDetails = "Maps data not initialized yet";
	}
	else {
		if (ignoredMapNames.find(mapName) != ignoredMapNames.end()) {
			return;
		}

		fs::path path(mapName);

		if (mapReplacementMaps.find(mapName) != mapReplacementMaps.end()) {
			updateByHash(path);
			return;
		}

		// TODO: Attempt to find absolute path if not an absolute path

		if (path.is_absolute() && fs::exists(path)) {
			// Attempt to get workshopId from path
			for (auto p : path) {
				if (p.compare("252950") == 0) continue;

				auto pstr = p.string();
				if (std::all_of(pstr.begin(), pstr.end(), ::isdigit)) {
					if (updateByWorkshopId(pstr)) {
						_globalCvarManager->log(L"CustomMapSupport: custom map detect by workshop ID in path: " + mapName);
						return;
					}
				}
			}

			if (updateByFilename(path.filename().string(), false)) {
				_globalCvarManager->log(L"CustomMapSupport: custom map detected by filename: " + mapName);
				return;
			}

			if (updateByHash(path)) {
				_globalCvarManager->log(L"CustomMapSupport: custom map detected by hash: " + mapName);
				return;
			}
			updateByFilename(path.filename().string(), true);
			_globalCvarManager->log(L"CustomMapSupport: failed to detect custom map. Setting as map name: " + mapName);
		}
		else {
			updateByFilename(path.filename().string(), true);
			_globalCvarManager->log(L"CustomMapSupport: failed to detect custom map. Setting as map name: " + mapName);
		}
	}
}

std::string CustomMapSupport::toString()
{
	return loadedMapDetails;
}

void CustomMapSupport::loadMapsFromJson(nlohmann::json& j)
{
	_globalCvarManager->log("CustomMapSupport: Doing full parse of maps.json");
	try {
		j.at("maps").get_to(workshopIdToMap);
		for (auto it = workshopIdToMap.begin(); it != workshopIdToMap.end(); ++it) {
			for (auto it2 = it->second.mapFileHistory.begin(); it2 != it->second.mapFileHistory.end(); ++it2) {
				// Update hash to maps
				auto hashIt = hashToAllMaps.find(it2->fullHash);
				if (hashIt == hashToAllMaps.end() || hashIt->second.published > it->second.published) {
					hashToAllMaps[it2->fullHash] = it->second;
				}

				// Update filename to maps
				auto filenameIt = filenameToAllMaps.find(it2->filename);
				if (filenameIt == filenameToAllMaps.end() || filenameIt->second.published > it->second.published) {
					filenameToAllMaps[it2->filename] = it->second;
				}
			}
		}
	}
	catch (...) {
		_globalCvarManager->log("CustomMapSupport: failed to do full parse of maps.json");
	}
}

void CustomMapSupport::updateByMap(CustomMap& map)
{
	loadedMapDetails = map.title + " created by " + map.author + " can be downloaded at https://steamcommunity.com/sharedfiles/filedetails/?id=" + map.workshopId;
}

bool CustomMapSupport::updateByWorkshopId(std::string workshopId)
{
	auto findIt = workshopIdToMap.find(workshopId);
	if (findIt == workshopIdToMap.end()) return false;
	updateByMap(findIt->second);
	return true;
}

bool CustomMapSupport::updateByFilename(std::string filename, bool forced)
{
	auto it = filenameToAllMaps.find(filename);
	if (it == filenameToAllMaps.end()) {
		if (forced) {
			loadedMapDetails = filename + " (failed to identify workshop info)";
			return true;
		}
		else {
			return false;
		}
	}

	updateByMap(it->second);
	return true;
}

bool CustomMapSupport::updateByHash(std::filesystem::path path)
{
	// Generate md5 hash of file
	if (!fs::exists(path)) {
		_globalCvarManager->log("CustomMapSupport: given path for hash does not exist: " + path.string());
		return false;
	}

	ifstream fin(path, ifstream::binary);
	if (fin.fail()) {
		_globalCvarManager->log("CustomMapSupport: failed to open file to generate hash: " + path.string());
		return false;
	}

	// Using openSSL's MD5 since we're already depending on openSSL
	MD5_CTX ctx;
	MD5_Init(&ctx);
	char buf[READ_FILE_CHUNK_SIZE];
	while (!fin.eof() || !fin.fail()) {
		fin.read(buf, READ_FILE_CHUNK_SIZE);
		MD5_Update(&ctx, buf, fin.gcount());
	}

	unsigned char md5[MD5_DIGEST_LENGTH];
	if (MD5_Final(md5, &ctx)) {
		stringstream ss;
		for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
			ss << hex << setfill('0') << setw(2) << static_cast<unsigned>(md5[i]);
		}
		std::string hash = ss.str();

		auto hashIt = hashToAllMaps.find(hash);
		if (hashIt == hashToAllMaps.end()) {
			_globalCvarManager->log("CustomMapSupport: hash (" + hash + ") for file (" + path.string() + ") did not match any files in maps.json");
			return false;
		}

		updateByMap(hashIt->second);
	}
	else {
		_globalCvarManager->log("CustomMapSupport: failed to generate hash from file: " + path.string());
		return false;
	}
}

void from_json(const json& j, CustomMapFile& mapFile)
{
	j.at("filename").get_to(mapFile.filename);
	j.at("fullHash").get_to(mapFile.fullHash);
	j.at("updateTimestamp").get_to(mapFile.updateTimestamp);
}

void from_json(const json& j, CustomMap& map)
{
	j.at("workshopId").get_to(map.workshopId);
	j.at("author").get_to(map.author);
	j.at("title").get_to(map.title);
	j.at("desc").get_to(map.description);
	j.at("published").get_to(map.published);
	j.at("mapFileHistory").get_to(map.mapFileHistory);

	map.lastUpdated = map.published;
	for (const auto& mapFile : map.mapFileHistory) {
		if (mapFile.updateTimestamp > map.lastUpdated) {
			map.lastUpdated = mapFile.updateTimestamp;
		}
	}	
}
