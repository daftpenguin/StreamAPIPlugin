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

std::vector<std::string> mapExtensions({ ".udk", ".upk", ".umap" });

fs::path getSteamRLInstallPath()
{
	wstring steamPathStr;
	try {
		winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam" };
		steamPathStr = key.GetStringValue(L"SteamPath");
	}
	catch (...) {}

	if (steamPathStr.empty()) {
		_globalCvarManager->log("CustomMapSupport: Steam version not installed");
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
	wstring epicManifestsDir;
	try {
		winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Epic Games\\EOS" };
		epicManifestsDir = key.GetStringValue(L"ModSdkMetadataDir");
	}
	catch (...) {}

	if (epicManifestsDir.empty()) {
		_globalCvarManager->log("CustomMapSupport: Epic version not installed");
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
	L"arc_p",
	L"arc_standard_p",
	L"bb_p",
	L"beach_night_p",
	L"beach_p",
	L"chn_stadium_day_p",
	L"chn_stadium_p",
	L"cs_day_p",
	L"cs_hw_p",
	L"cs_p",
	L"eurostadium_night_p",
	L"eurostadium_p",
	L"eurostadium_rainy_p",
	L"eurostadium_snownight_p",
	L"farm_night_p",
	L"farm_p",
	L"haunted_trainstation_p",
	L"hoopsstadium_p",
	L"menu_main_p",
	L"music_p",
	L"neotokyo_p",
	L"neotokyo_standard_p",
	L"park_night_p",
	L"park_p",
	L"park_rainy_p",
	L"shattershot_p",
	L"stadium_day_p",
	L"stadium_foggy_p",
	L"stadium_p",
	L"stadium_race_day_p",
	L"stadium_winter_p",
	L"throwbackhockey_p",
	L"throwbackstadium_p",
	L"trainstation_dawn_p",
	L"trainstation_night_p",
	L"trainstation_p",
	L"trainstation_spooky_p",
	L"underwater_p",
	L"utopiastadium_dusk_p",
	L"utopiastadium_p",
	L"utopiastadium_snow_p",
	L"wasteland_art_night_p",
	L"wasteland_art_night_s_p",
	L"wasteland_art_p",
	L"wasteland_art_s_p",
	L"wasteland_night_p",
	L"wasteland_night_s_p",
	L"wasteland_night_vfx_p",
	L"wasteland_p",
	L"wasteland_s_p"
	});

unordered_set<wstring> mapReplacementMaps({
	L"labs_basin_p",
	L"labs_circlepillars_p",
	L"labs_corridor_p",
	L"labs_cosmic_p",
	L"labs_cosmic_v4_p",
	L"labs_doublegoal_p",
	L"labs_doublegoal_v2_p",
	L"labs_galleon_p",
	L"labs_octagon_02_p",
	L"labs_octagon_p",
	L"labs_underpass_p",
	L"labs_underpass_v0_p",
	L"labs_utopia_p"
	});

void CustomMapSupport::updateMap(std::wstring mapName)
{
	_globalCvarManager->log(L"CustomMapSupport: received map update for: " + mapName);
	if (!initialized) {
		loadedMapDetails = "Maps data not initialized yet";
	}
	else {
		if (ignoredMapNames.find(boost::to_lower_copy(mapName)) != ignoredMapNames.end()) {
			return;
		}

		if (mapReplacementMaps.find(boost::to_lower_copy(mapName)) != mapReplacementMaps.end()) {
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
	// TODO: Scraper script should combine lethamyr and workshop maps if they match?
	_globalCvarManager->log("CustomMapSupport: Doing full parse of maps.json");
	try {
		j.at("maps").get_to(workshopIdToMap);
		for (auto it = workshopIdToMap.begin(); it != workshopIdToMap.end(); ++it) {
			if (it->second.type == CustomMapType::WORKSHOP) {
				for (auto it2 = it->second.mapFileHistory.begin(); it2 != it->second.mapFileHistory.end(); ++it2) {
					auto hashIt = hashToAllMaps.find(it2->fullHash);
					if (hashIt == hashToAllMaps.end() || hashIt->second.published > it->second.published) {
						hashToAllMaps.emplace(it2->fullHash, it->second);
					}

					auto filenameIt = filenameToAllMaps.find(it2->filename);
					if (filenameIt == filenameToAllMaps.end() || filenameIt->second.published > it->second.published) {
						filenameToAllMaps.emplace(it2->filename, it->second);
					}
				}
			}
			else {
				auto hashIt = hashToAllMaps.find(it->second.fullHash);
				if (hashIt == hashToAllMaps.end()) {
					hashToAllMaps.emplace(it->second.fullHash, it->second);
				}

				auto filenameIt = filenameToAllMaps.find(it->second.filename);
				if (filenameIt == filenameToAllMaps.end()) {
					hashToAllMaps.emplace(it->second.filename, it->second);
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
	if (map.type == CustomMapType::WORKSHOP) {
		loadedMapDetails = map.title + " created by " + map.author +
			" can be downloaded at https://steamcommunity.com/sharedfiles/filedetails/?id=" + map.workshopId;
	}
	else {
		loadedMapDetails = map.title + " created by " + map.author + " can be downloaded at " + map.link;
	}
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
	vector<string> possibleFilenames;
	if (fs::path(filename).has_extension()) {
		possibleFilenames.push_back(filename);
	}
	else {
		for (auto& extension : mapExtensions) {
			possibleFilenames.push_back(filename + extension);
		}
	}

	for (auto& fname : possibleFilenames) {
		auto it = filenameToAllMaps.find(fname);
		if (it != filenameToAllMaps.end()) {
			updateByMap(it->second);
			return true;
		}
	}

	if (!forced) return false;

	loadedMapDetails = filename + " (failed to identify workshop info)";
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

std::filesystem::path CustomMapSupport::findAbsolutePath(std::filesystem::path path)
{
	if (path.is_absolute()) return path;

	// Make a best effort to find the map's file location

	vector<fs::path> possibleFilenames;
	if (path.has_extension()) {
		possibleFilenames.push_back(path.filename());
	}
	else {
		for (auto& extension : mapExtensions) {
			possibleFilenames.push_back(path.stem().string() + extension);
		}
	}

	for (auto& fname : possibleFilenames) {
		for (auto& d : customMapsDirs) {
			auto p = d / fname;
			if (fs::exists(p)) {
				return p;
			}
		}
	}

	if (!workshopDir.empty()) {
		for (auto& fname : possibleFilenames) {
			for (auto& d : fs::directory_iterator(workshopDir)) {
				auto p = d.path() / fname;
				if (fs::exists(p)) {
					return p;
				}
			}
		}
	}

	for (auto& fname : possibleFilenames) {
		for (auto& d : standardMapsDir) {
			auto p = d / fname;
			if (fs::exists(p)) {
				return p;
			}
		}
	}

	_globalCvarManager->log("CustomMapSupport: failed to find absolute path for: " + path.string());
	return path;
}

void from_json(const json& j, WorkshopMapFile& mapFile)
{
	j.at("filename").get_to(mapFile.filename);
	j.at("fullHash").get_to(mapFile.fullHash);
	j.at("updateTimestamp").get_to(mapFile.updateTimestamp);
}

void from_json_workshopMap(const json& j, CustomMap& map)
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

void from_json_lethamyrMap(const json& j, CustomMap& map)
{
	j.at("author").get_to(map.author);
	j.at("title").get_to(map.title);
	j.at("desc").get_to(map.description);
	j.at("link").get_to(map.link);
	j.at("filename").get_to(map.filename);
	j.at("fullHash").get_to(map.fullHash);
}

void from_json(const json& j, CustomMap& map)
{
	if (j.find("link") != j.end()) {
		map.type = CustomMapType::LETHAMYR;
		from_json_lethamyrMap(j, map);
	}
	else {
		map.type = CustomMapType::WORKSHOP;
		from_json_workshopMap(j, map);
	}
}