#include "CustomMapSupport.h"
#include "StreamAPIPlugin.h"
#include "WinReg-4.1.0/WinReg/WinReg.hpp"

#include <fstream>
#include <filesystem>
#include <openssl/md5.h>
#include <boost/algorithm/string.hpp>

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

fs::path getSteamRLInstallPath()
{
	winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam" };
	wstring steamPathStr = key.GetStringValue(L"SteamPath");

	if (steamPathStr.empty()) {
		_globalCvarManager->log("CustomMapSupport: Steam not installed");
		return fs::path();
	}

	fs::path steamPath(steamPathStr);
	if (!fs::exists(steamPath)) {
		_globalCvarManager->log("CustomMapSupport: Steam path doesn't exist");
		return fs::path();
	}

	vector<fs::path> steamLibraryPaths({ steamPath / "steamapps" });
	if (fs::exists(steamPath / "steamapps" / "libraryfolders.vdf")) {
		ifstream fin(steamPath / "steamapps" / "libraryfolders.vdf");
		if (fin.fail()) {
			_globalCvarManager->log("CustomMapSupport: Failed to open libraryfolders.vdf");
			return fs::path();
		}
		string line;
		while (getline(fin, line)) {
			boost::algorithm::trim(line);
			vector<string> fields;
			boost::algorithm::split(fields, line, boost::is_any_of("\t"));
			if (fields.size() == 3) {
				try {
					int v = stoi(fields[0].substr(1, fields[0].size() - 2));
					fs::path path(fields[2].substr(1, fields[2].size() - 2));
					steamLibraryPaths.push_back(path);
				}
				catch (...) {
					continue;
				}
			}
		}
	}

	for (const fs::path& path : steamLibraryPaths) {
		if (fs::exists(path / "appmanifest_252950.acf")) {
			fs::path rlPath = path / "common" / "rocketleague";
			if (fs::exists(rlPath)) return rlPath;
		}
	}

	return fs::path();
}

fs::path getEpicRLInstallPath()
{
	winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Epic Games\\EOS" };
	wstring epicManifestsDir = key.GetStringValue(L"ModSdkMetadataDir");

	if (epicManifestsDir.empty()) {
		return fs::path();
	}

	for (auto& p : fs::directory_iterator(epicManifestsDir)) {
		if (p.is_directory()) continue;

		auto manifest = p.path();
		ifstream fin(manifest);
		if (fin.fail()) {
			_globalCvarManager->log("CustomMapSupport: Failed to load epic manifest file: " + manifest.string());
			continue;
		}

		stringstream ss;
		ss << fin.rdbuf();

		try {
			json j = json::parse(ss.str());
			if (j.find("MandatoryAppFolderName") == j.end() || j.find("InstallLocation") == j.end()) {
				_globalCvarManager->log("CustomMapSupport: Failed to find InstallLocation or MandatoryAppFolderName in manifest file: " + manifest.string());
				continue;
			}
			auto mandatoryAppFolderName = fs::path(j["MandatoryAppFolderName"].get<string>());
			if (mandatoryAppFolderName.compare("rocketleague") != 0) {
				continue;
			}
			auto installLocation = fs::path(j["InstallLocation"].get<string>());
			if (!fs::exists(installLocation)) {
				_globalCvarManager->log("CustomMapSupport: InstallLocation (" + installLocation.string() + ") retrieved from manifest file (" + manifest.string() + ") does not exist");
				continue;
			}
			return installLocation;
		}
		catch (...) {
			_globalCvarManager->log("CustomMapSupport: Failed to parse manifest file: " + manifest.string());
		}
	}

	return fs::path();
}

CustomMapSupport::CustomMapSupport() : lastModified(0), mapsJsonPath(mapsJsonPath), loadedMapDetails("No map loaded yet"), initialized(false)
{
	
}

void CustomMapSupport::init(bool isSteamVersion, std::filesystem::path mapsJsonPath)
{
	// TODO: Do initialization in a separate thread?

	auto steamPath = getSteamRLInstallPath();
	if (!steamPath.empty()) {
		auto standardMapPath = steamPath / "TAGame" / "CookedPCConsole";
		if (fs::exists(standardMapPath)) {
			if (isSteamVersion) {
				standardMapsDir = standardMapPath;
			}

			auto modsPath = standardMapPath / "mods";
			if (fs::exists(modsPath)) {
				customMapsDirs.push_back(modsPath);
			}
		}

		auto workshopPath = steamPath.parent_path().parent_path() / "workshop" / "content" / "252950";
		if (fs::exists(workshopPath)) {
			workshopDir = workshopPath;
		}
	}

	auto epicPath = getEpicRLInstallPath();
	if (!epicPath.empty()) {
		auto standardMapPath = epicPath / "TAGame" / "CookedPCConsole";
		if (fs::exists(standardMapPath)) {
			if (!isSteamVersion) {
				standardMapsDir = standardMapPath;
			}

			auto modsPath = standardMapPath / "mods";
			if (fs::exists(modsPath)) {
				customMapsDirs.push_back(modsPath);
			}
		}
	}

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
	L"Labs_Basin_P",
	L"Labs_CirclePillars_P",
	L"Labs_Corridor_P",
	L"Labs_Cosmic_P",
	L"Labs_Cosmic_V4_P",
	L"Labs_DoubleGoal_P",
	L"Labs_DoubleGoal_V2_P",
	L"Labs_Galleon_P",
	L"Labs_Octagon_02_P",
	L"Labs_Octagon_P",
	L"Labs_Underpass_P",
	L"Labs_Underpass_v0_p",
	L"Labs_Utopia_P"
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

		if (mapReplacementMaps.find(mapName) != mapReplacementMaps.end()) {
			updateByHash(standardMapsDir / (mapName + L".upk"));
			return;
		}

		fs::path path(mapName);
		if (updateByFilename(path.filename().string(), false)) {
			_globalCvarManager->log(L"CustomMapSupport: custom map detected by filename: " + mapName);
			return;
		}

		path = findAbsolutePath(path);

		// Attempt to get workshopId from path
		if (path.is_absolute() && fs::exists(path)) {
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
		}

		if (path.is_absolute() && fs::exists(path) && updateByHash(path)) {
			_globalCvarManager->log(L"CustomMapSupport: custom map detected by hash: " + mapName);
			return;
		}

		updateByFilename(path.filename().string(), true);
		_globalCvarManager->log(L"CustomMapSupport: failed to detect custom map. Setting as map name: " + mapName);
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
	// TODO: If there isn't an extension, try them all
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
	char* buf = (char *) malloc(READ_FILE_CHUNK_SIZE);
	while (!fin.eof() || !fin.fail()) {
		fin.read(buf, READ_FILE_CHUNK_SIZE);
		MD5_Update(&ctx, buf, fin.gcount());
	}

	unsigned char md5[MD5_DIGEST_LENGTH];
	bool matched = false;
	if (MD5_Final(md5, &ctx)) {
		stringstream ss;
		for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
			ss << hex << setfill('0') << setw(2) << static_cast<unsigned>(md5[i]);
		}
		std::string hash = ss.str();

		auto hashIt = hashToAllMaps.find(hash);
		if (hashIt == hashToAllMaps.end()) {
			_globalCvarManager->log("CustomMapSupport: hash (" + hash + ") for file (" + path.string() + ") did not match any files in maps.json");
		}
		else {
			updateByMap(hashIt->second);
			matched = true;
		}
	}
	else {
		_globalCvarManager->log("CustomMapSupport: failed to generate hash from file: " + path.string());
	}

	free(buf);
	return matched;
}

vector<string> mapExtensions({ ".udk", ".upk", ".umap" });

std::filesystem::path CustomMapSupport::findAbsolutePath(std::filesystem::path path)
{
	if (path.is_absolute()) return path;

	// Make a best effort approach to finding the map's file location

	// TODO: We shouldn't add an extension if there is one already
	for (auto& extension : mapExtensions) {
		for (auto& d : customMapsDirs) {
			auto p = d / (path.stem().string() + extension);
			if (fs::exists(p)) {
				return p;
			}
		}
	}

	if (!workshopDir.empty()) {
		for (auto& extension : mapExtensions) {
			for (auto& d : fs::directory_iterator(workshopDir)) {
				auto p = d.path() / (path.stem().string() + extension);
				if (fs::exists(p)) {
					return p;
				}
			}
		}
	}

	_globalCvarManager->log("CustomMapSupport: failed to find absolute path for: " + path.string());
	return path;
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
