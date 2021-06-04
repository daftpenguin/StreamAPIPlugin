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

map<wstring, string> standardMapNames = {
	{ L"arc_p", "ARCtagon" },
	{ L"arc_standard_p", "Starbase ARC" },
	{ L"beach_night_p", "Salty Shores (Night)" },
	{ L"beach_p", "Salty Shores" },
	{ L"chn_stadium_day_p", "Forbidden Temple (Day)" },
	{ L"chn_stadium_p", "Forbidden Temple" },
	{ L"cs_day_p", "Champions Field (Day)" },
	{ L"cs_hw_p", "Rivals Arena" },
	{ L"cs_p", "Champions Field" },
	{ L"eurostadium_night_p", "Mannfield (Night)" },
	{ L"eurostadium_p", "Mannfield" },
	{ L"eurostadium_rainy_p", "Mannfield (Stormy)" },
	{ L"eurostadium_snownight_p", "Mannfield (Snowy)" },
	{ L"farm_night_p", "Farmstead (Night)" },
	{ L"farm_p", "Farmstead" },
	{ L"hoopsstadium_p", "Dunk House" },
	{ L"labs_circlepillars_p", "Pillars" },
	{ L"labs_cosmic_v4_p", "Cosmic" },
	{ L"labs_doublegoal_v2_p", "Double Goal" },
	{ L"labs_octagon_02_p", "Octagon" },
	{ L"labs_underpass_p", "Underpass" },
	{ L"labs_utopia_p", "Utopia Retro" },
	{ L"music_p", "Neon Fields" },
	{ L"neotokyo_p", "Tokyo Underpass" },
	{ L"neotokyo_standard_p", "Neo Tokyo" },
	{ L"park_night_p", "Beckwith Park (Midnight)" },
	{ L"park_p", "Beckwith Park" },
	{ L"park_rainy_p", "Beckwith Park (Stormy)" },
	{ L"shattershot_p", "Core 707" },
	{ L"stadium_day_p", "DFH Stadium (Day)" },
	{ L"stadium_foggy_p", "DFH Stadium (Stormy)" },
	{ L"stadium_p", "DFH Stadium" },
	{ L"stadium_race_day_p", "DFH Stadium (Circuit)" },
	{ L"stadium_winter_p", "DFH Stadium (Snowy)" },
	{ L"throwbackstadium_p", "Throwback Stadium" },
	{ L"trainstation_dawn_p", "Urban Central (Dawn)" },
	{ L"trainstation_night_p", "Urban Central (Night)" },
	{ L"trainstation_p", "Urban Central" },
	{ L"underwater_p", "AquaDome" },
	{ L"utopiastadium_dusk_p", "Utopia Coliseum (Dusk)" },
	{ L"utopiastadium_p", "Utopia Coliseum" },
	{ L"utopiastadium_snow_p", "Utopia Coliseum (Snowy)" },
	{ L"wasteland_night_p", "Badlands (Night)" },
	{ L"wasteland_night_s_p", "Wasteland (Night)" },
	{ L"wasteland_p", "Badlands" },
	{ L"wasteland_s_p", "Wasteland" },
};

std::unordered_set<std::wstring> cachedCookedPCFilenames({
	L"akaudio",
	L"arc_field",
	L"arc_fx",
	L"arc_p",
	L"arc_p_mapimage_sf",
	L"arc_sfx",
	L"arc_sky",
	L"arc_standard_field",
	L"arc_standard_fx",
	L"arc_standard_grass",
	L"arc_standard_p",
	L"arc_standard_p_mapimage_sf",
	L"arc_standard_sky",
	L"at_glados_potato_sf",
	L"at_glados_potato_t_sf",
	L"at_oddworld_abe_sf",
	L"at_oddworld_abe_t_sf",
	L"at_oddworld_glukkon_sf",
	L"at_oddworld_glukkon_t_sf",
	L"at_portalcube_sf",
	L"at_portalcube_t_sf",
	L"at_portal_pc_sf",
	L"at_portal_pc_t_sf",
	L"at_warframe_chroma_sf",
	L"at_warframe_chroma_t_sf",
	L"at_warframe_excalibur_sf",
	L"at_warframe_excalibur_t_sf",
	L"at_warframe_loki_sf",
	L"at_warframe_loki_t_sf",
	L"bb_field",
	L"bb_grounds",
	L"bb_lights",
	L"bb_oob",
	L"bb_oob2",
	L"bb_p",
	L"bb_p_mapimage_sf",
	L"bb_sfx",
	L"beach_cage",
	L"beach_field",
	L"beach_fx",
	L"beach_night_cage",
	L"beach_night_field",
	L"beach_night_fx",
	L"beach_night_oob",
	L"beach_night_oob2",
	L"beach_night_oobterrain",
	L"beach_night_p",
	L"beach_night_p_mapimage_sf",
	L"beach_night_sfx",
	L"beach_oob",
	L"beach_oob2",
	L"beach_oobterrain",
	L"beach_p",
	L"beach_p_mapimage_sf",
	L"beach_sfx",
	L"bg_beach",
	L"bg_chn_stadium",
	L"bg_cs",
	L"bg_farm_upsidedown",
	L"bg_musicmap",
	L"bg_park",
	L"bg_stadium",
	L"bg_stadium_race_day",
	L"bg_throwbackhockey",
	L"bg_trainstation_spooky",
	L"bg_utopiasnow",
	L"blackmarkettest_skin_sf",
	L"blackmarkettest_skin_t_sf",
	L"chn_stadium_cage",
	L"chn_stadium_day_field",
	L"chn_stadium_day_mountains",
	L"chn_stadium_day_oob",
	L"chn_stadium_day_p",
	L"chn_stadium_day_p_mapimage_sf",
	L"chn_stadium_day_sfx",
	L"chn_stadium_day_vfx",
	L"chn_stadium_field",
	L"chn_stadium_lighting",
	L"chn_stadium_oob",
	L"chn_stadium_oob_mountains",
	L"chn_stadium_p",
	L"chn_stadium_p_mapimage_sf",
	L"chn_stadium_rocks",
	L"chn_stadium_sfx",
	L"chn_stadium_vfx",
	L"core",
	L"crown_bronze_sf",
	L"crown_bronze_t_sf",
	L"crown_gold_sf",
	L"crown_gold_t_sf",
	L"crown_platinum_sf",
	L"crown_platinum_t_sf",
	L"crown_silver_sf",
	L"crown_silver_t_sf",
	L"cs_day_field",
	L"cs_day_grounds",
	L"cs_day_lights",
	L"cs_day_oob",
	L"cs_day_oob2",
	L"cs_day_p",
	L"cs_day_p_mapimage_sf",
	L"cs_day_sfx",
	L"cs_field",
	L"cs_grounds",
	L"cs_hw_field",
	L"cs_hw_grounds",
	L"cs_hw_lights",
	L"cs_hw_oob",
	L"cs_hw_oob2",
	L"cs_hw_p",
	L"cs_hw_p_mapimage_sf",
	L"cs_hw_sfx",
	L"cs_lights",
	L"cs_oob",
	L"cs_oob2",
	L"cs_p",
	L"cs_p_mapimage_sf",
	L"cs_sfx",
	L"currency_credits_sf",
	L"currency_credits_t_sf",
	L"editorlandscaperesources",
	L"editormaterials",
	L"editormeshes",
	L"editorresources",
	L"engine",
	L"enginebuildings",
	L"enginedebugmaterials",
	L"enginematerials",
	L"engineresources",
	L"enginevolumetrics",
	L"engine_mi_shaders",
	L"eurostadium_night_p",
	L"eurostadium_night_p_mapimage_sf",
	L"eurostadium_night_sfx",
	L"eurostadium_p",
	L"eurostadium_p_mapimage_sf",
	L"eurostadium_rainy_audio",
	L"eurostadium_rainy_p",
	L"eurostadium_rainy_p_mapimage_sf",
	L"eurostadium_snownight_p",
	L"eurostadium_snownight_p_mapimage_sf",
	L"eurostadium_snownight_sfx",
	L"eurostadium_snow_sfx",
	L"eurostad_oob_audio_map",
	L"farm_night_oobterrain",
	L"farm_night_p",
	L"farm_night_p_mapimage_sf",
	L"farm_night_sfx",
	L"farm_oobterrain",
	L"farm_p",
	L"farm_p_mapimage_sf",
	L"farm_sfx",
	L"farm_upsidedown_sfx",
	L"farm_vfx",
	L"gameinfo_basketball_sf",
	L"gameinfo_breakout_sf",
	L"gameinfo_default_sf",
	L"gameinfo_football_sf",
	L"gameinfo_fte_sf",
	L"gameinfo_gfxmenu_sf",
	L"gameinfo_godball_sf",
	L"gameinfo_hockey_sf",
	L"gameinfo_items_sf",
	L"gameinfo_replay_sf",
	L"gameinfo_season_sf",
	L"gameinfo_soccar_sf",
	L"gameinfo_trainingeditor_sf",
	L"gameinfo_tutorial_sf",
	L"gfxplaylist_beachball_sf",
	L"gfxplaylist_hauntedball_sf",
	L"gfxplaylist_heatseeker_sf",
	L"gfxplaylist_spikerush_sf",
	L"gfxshell_sf",
	L"gfxsounds_endround_sf",
	L"gfxsounds_hud_sf",
	L"gfxsounds_mainmenu_sf",
	L"gfxsounds_season_sf",
	L"gfxsounds_shared_sf",
	L"gfxsounds_tutorialvo_sf",
	L"gfxsounds_tutorial_sf",
	L"gfxsounds_xp_sf",
	L"gfxui",
	L"gfx_blueprintswalkthrough_sf",
	L"gfx_bugreportmenu_sf",
	L"gfx_challengemenu_sf",
	L"gfx_challengesprimemenu_sf",
	L"gfx_clubsmenu_sf",
	L"gfx_controllerlayout_sf",
	L"gfx_credits_sf",
	L"gfx_endgamemenu_sf",
	L"gfx_esportsshopmenu_sf",
	L"gfx_eventmenu_sf",
	L"gfx_fonts_sf",
	L"gfx_garagemenu_sf",
	L"gfx_hudmatchinfo_sf",
	L"gfx_hud_sf",
	L"gfx_invitetopartymenu_sf",
	L"gfx_leaderboardsmenu_sf",
	L"gfx_loadingscreen_sf",
	L"gfx_mainmenu_sf",
	L"gfx_mappreferencesmenu_sf",
	L"gfx_midgamemenu_sf",
	L"gfx_midgamescoreboard_sf",
	L"gfx_musiccuemenu_sf",
	L"gfx_nameplates_static_sf",
	L"gfx_playerinteractionmenu_sf",
	L"gfx_playertradingmenu_sf",
	L"gfx_playervanitymenu_sf",
	L"gfx_playmenu_sf",
	L"gfx_prematchlobby_sf",
	L"gfx_premiumcontainermenu_sf",
	L"gfx_premiuminventory_sf",
	L"gfx_privatematchmenu_sf",
	L"gfx_purchasebundlesmenu_sf",
	L"gfx_rendertotexture_sf",
	L"gfx_replayhud_sf",
	L"gfx_replaysmenu_sf",
	L"gfx_rocketpassmenu_sf",
	L"gfx_rotatingshopmenu_sf",
	L"gfx_seasonmenu_sf",
	L"gfx_serverbrowsermenu_sf",
	L"gfx_settingsmenu_sf",
	L"gfx_shared_logos_sf",
	L"gfx_shared_partnerlogos_sf",
	L"gfx_shared_powerupicons_sf",
	L"gfx_shopprimemenu_sf",
	L"gfx_spectatorhud_sf",
	L"gfx_startmenu_sf",
	L"gfx_statsmenu_sf",
	L"gfx_teamselect_sf",
	L"gfx_tournamentdetailsmenu_sf",
	L"gfx_tournamentresults_sf",
	L"gfx_tournamentsmenu_sf",
	L"gfx_tradeinmenu_sf",
	L"gfx_trainingbrowsermenu_sf",
	L"gfx_trainingeditormenu_sf",
	L"gfx_trainingmenu_sf",
	L"gfx_uishell_sf",
	L"gfx_winnermenu_sf",
	L"gfx_workshopmenu_sf",
	L"gfx_xpprogressmenu_sf",
	L"guidcache",
	L"haunted_trainstation_p",
	L"haunted_trainstation_p_mapimage_sf",
	L"haunted_trainstation_sfx",
	L"hoopsstadium_p",
	L"hoopsstadium_p_mapimage_sf",
	L"hoopsstadium_sfx",
	L"ipdrv",
	L"itemcontainer_sf",
	L"itemcontainer_t_sf",
	L"joingametransition",
	L"key_magic_sf",
	L"key_magic_t_sf",
	L"key_sf",
	L"key_t_sf",
	L"labs_basin_p",
	L"labs_basin_p_mapimage_sf",
	L"labs_circlepillars_p",
	L"labs_circlepillars_p_mapimage_sf",
	L"labs_corridor_p",
	L"labs_corridor_p_mapimage_sf",
	L"labs_cosmic_p",
	L"labs_cosmic_v4_p",
	L"labs_cosmic_v4_p_mapimage_sf",
	L"labs_doublegoal_p",
	L"labs_doublegoal_v2_p",
	L"labs_doublegoal_v2_p_mapimage_sf",
	L"labs_galleon_mast_p",
	L"labs_galleon_mast_p_mapimage_sf",
	L"labs_galleon_p",
	L"labs_galleon_p_mapimage_sf",
	L"labs_octagon_02_p",
	L"labs_octagon_02_p_mapimage_sf",
	L"labs_octagon_p",
	L"labs_oob_01",
	L"labs_sfx",
	L"labs_underpass_p",
	L"labs_underpass_p_mapimage_sf",
	L"labs_underpass_v0_p",
	L"labs_utopia_p",
	L"labs_utopia_p_mapimage_sf",
	L"maptemplateindex",
	L"maptemplates",
	L"menu_anthem",
	L"menu_beach_sfx",
	L"menu_carselect_intro",
	L"menu_garagecomplex",
	L"menu_main_p",
	L"menu_premiumgarage",
	L"mods",
	L"music_effects",
	L"music_field",
	L"music_oob",
	L"music_p",
	L"music_p_mapimage_sf",
	L"music_sfx",
	L"mutators_balls_sf",
	L"mutators_items_sf",
	L"mutators_sf",
	L"neotokyo_buildings",
	L"neotokyo_buildings_s",
	L"neotokyo_lights",
	L"neotokyo_lights_s",
	L"neotokyo_p",
	L"neotokyo_p_mapimage_sf",
	L"neotokyo_sfx",
	L"neotokyo_signs",
	L"neotokyo_signs_02",
	L"neotokyo_signs_s",
	L"neotokyo_standard_p",
	L"neotokyo_standard_p_mapimage_sf",
	L"nodebuddies",
	L"nue_cine_master",
	L"onlinesubsystemeos",
	L"onlinesubsystemsteamworks",
	L"park_night_p",
	L"park_night_p_mapimage_sf",
	L"park_night_sfx",
	L"park_p",
	L"park_p_mapimage_sf",
	L"park_rainy_p",
	L"park_rainy_p_mapimage_sf",
	L"park_rainy_sfx",
	L"park_sfx",
	L"pennant_bluechecker_sf",
	L"pennant_bluechecker_t_sf",
	L"pennant_camo_sf",
	L"pennant_camo_t_sf",
	L"pennant_orangenylon_sf",
	L"pennant_orangenylon_t_sf",
	L"pennant_psyonix_sf",
	L"pennant_psyonix_t_sf",
	L"projectx",
	L"refshadercache-pc-d3d-sm5",
	L"rocketpass_premium_sf",
	L"rocketpass_premium_t_sf",
	L"seasonlogos_sf",
	L"seasonlogos_t_sf",
	L"shattershot_oob_exploration",
	L"shattershot_p",
	L"shattershot_p_mapimage_sf",
	L"shattershot_sfx",
	L"shattershot_vfx",
	L"shopitem_body_carcar_sf",
	L"shopitem_body_endo_sf",
	L"shopitem_body_flatbread_sf",
	L"shopitem_body_mitty_sf",
	L"shopitem_body_musclecar2_sf",
	L"shopitem_body_musclecar_sf",
	L"shopitem_body_o2_sf",
	L"shopitem_body_pumpernickel_sf",
	L"shopitem_body_rhino2_sf",
	L"shopitem_body_sourdough_sf",
	L"shopitem_body_takumi_ii_sf",
	L"shopitem_body_torch2_sf",
	L"shopitem_body_twinzer_sf",
	L"shopitem_body_wheatbread_sf",
	L"stadium_day_p",
	L"stadium_day_p_mapimage_sf",
	L"stadium_foggy_p",
	L"stadium_foggy_p_mapimage_sf",
	L"stadium_foggy_sfx",
	L"stadium_oob_audio_map",
	L"stadium_p",
	L"stadium_p_mapimage_sf",
	L"stadium_race_day_audio_map",
	L"stadium_race_day_p",
	L"stadium_race_day_p_mapimage_sf",
	L"stadium_winter_p",
	L"stadium_winter_p_mapimage_sf",
	L"startup",
	L"startup_loc_int",
	L"tagame",
	L"throwbackhockey_field",
	L"throwbackhockey_oob",
	L"throwbackhockey_p",
	L"throwbackhockey_p_mapimage_sf",
	L"throwbackstadium_field",
	L"throwbackstadium_oob",
	L"throwbackstadium_p",
	L"throwbackstadium_p_mapimage_sf",
	L"trainstation_dawn_p",
	L"trainstation_dawn_p_mapimage_sf",
	L"trainstation_dawn_sfx",
	L"trainstation_night_p",
	L"trainstation_night_p_mapimage_sf",
	L"trainstation_night_sfx",
	L"trainstation_p",
	L"trainstation_p_mapimage_sf",
	L"trainstation_sfx",
	L"trainstation_spooky_p",
	L"trainstation_spooky_p_mapimage_sf",
	L"tutorialadvanced",
	L"tutorialadvanced_b",
	L"tutorialbeginner",
	L"tutorialtest_k",
	L"tutorial_fx",
	L"underwater_field",
	L"underwater_fx",
	L"underwater_lights",
	L"underwater_oob_foliage",
	L"underwater_p",
	L"underwater_plants",
	L"underwater_p_mapimage_sf",
	L"underwater_sfx",
	L"utopiastadium_dusk_p",
	L"utopiastadium_dusk_p_mapimage_sf",
	L"utopiastadium_night_sfx",
	L"utopiastadium_p",
	L"utopiastadium_p_mapimage_sf",
	L"utopiastadium_sfx",
	L"utopiastadium_snow_p",
	L"utopiastadium_snow_p_mapimage_sf",
	L"utopiastadium_snow_sfx",
	L"wasteland_art_night_p",
	L"wasteland_art_night_s_p",
	L"wasteland_art_p",
	L"wasteland_art_s_p",
	L"wasteland_night_p",
	L"wasteland_night_p_mapimage_sf",
	L"wasteland_night_sfx",
	L"wasteland_night_s_p",
	L"wasteland_night_s_p_mapimage_sf",
	L"wasteland_night_s_vfx",
	L"wasteland_night_vfx_p",
	L"wasteland_p",
	L"wasteland_p_mapimage_sf",
	L"wasteland_sfx",
	L"wasteland_s_p",
	L"wasteland_s_p_mapimage_sf",
	L"wasteland_s_vfx",
	L"wasteland_vfx",
	L"windrv",
	});

fs::path getSteamRLInstallPath()
{
	wstring steamPathStr;
	winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam" };
	steamPathStr = key.TryGetStringValue(L"SteamPath").value_or(L"");

	if (steamPathStr.empty()) {
		_globalCvarManager->log("CustomMapSupport: SOFTWARE\\Valve\\Steam\\SteamPath doesn't exist, trying SOFTWARE\\WOW6432Node\\Valve\\Steam");
		winreg::RegKey key{ HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam" };
		steamPathStr = key.TryGetStringValue(L"SteamPath").value_or(L"");
	}

	if (steamPathStr.empty()) {
		_globalCvarManager->log("CustomMapSupport: failed to identify Steam installation path from registry, try default location");
		if (fs::exists(DEFAULT_STEAM_INSTALLATION_PATH)) {
			steamPathStr = DEFAULT_STEAM_INSTALLATION_PATH;
		}
	}

	if (steamPathStr.empty()) {
		_globalCvarManager->log("CustomMapSupport: failed to identify Steam installation path from registry");
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
	winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Epic Games\\EOS" };
	epicManifestsDir = key.TryGetStringValue(L"ModSdkMetadataDir").value_or(L"");

	if (epicManifestsDir.empty()) {
		_globalCvarManager->log("CustomMapSupport: Failed to find epic installation from registry. Trying default installation path.");
		epicManifestsDir = DEFAULT_EPIC_INSTALLATION_PATH;
	} else if (!fs::exists(epicManifestsDir)) {
		_globalCvarManager->log("CustomMapSupport: Found epic in registry, but manifests directory doesn't exist. Trying default installation path.");
		epicManifestsDir = DEFAULT_EPIC_INSTALLATION_PATH;
	}

	if (!fs::exists(epicManifestsDir)) {
		_globalCvarManager->log("CustomMapSupport: Failed to find epic manifests directory. Map replacement identification will be unsupported.");
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

CustomMapSupport::CustomMapSupport() : lastModified(0), mapsJsonPath(mapsJsonPath), loadedMapDetails("No map loaded yet"), loadedCustomMapDetails("No map loaded yet"), initialized(false)
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

void CustomMapSupport::updateMap(std::wstring mapName)
{
	if (updateCustomMap(mapName)) {
		loadedMapDetails = loadedCustomMapDetails;
	}
	else {
		auto standardMap = standardMapNames.find(boost::to_lower_copy(mapName));
		if (standardMap != standardMapNames.end()) {
			loadedMapDetails = standardMap->second;
		}
	}
}

bool CustomMapSupport::updateCustomMap(std::wstring mapName)
{
	_globalCvarManager->log(L"CustomMapSupport: received map update for: " + mapName);
	if (!initialized) {
		loadedCustomMapDetails = "Maps data not initialized yet";
	}
	else {
		boost::to_lower(mapName);
		int r = updateIfCookedPCMap(mapName);
		if (r > 0) {
			_globalCvarManager->log(L"CustomMapSupport: cooked PC map found, ignoring or updating if map replacement: " + mapName);
			return r > 1;
		}

		fs::path path(mapName);
		if (updateByFilename(path.filename().string(), false)) {
			_globalCvarManager->log(L"CustomMapSupport: custom map detected by filename: " + mapName);
			return true;
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
						return true;
					}
				}
			}			
		}

		if (path.is_absolute() && fs::exists(path) && updateByHash(path)) {
			_globalCvarManager->log(L"CustomMapSupport: custom map detected by hash: " + mapName);
			return true;
		}

		updateByFilename(path.filename().string(), true);
		_globalCvarManager->log(L"CustomMapSupport: failed to detect custom map. Setting as map name: " + mapName);
		return true;
	}
	return false;
}

std::string CustomMapSupport::getCustomMap()
{
	return loadedCustomMapDetails;
}

std::string CustomMapSupport::getMap()
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
		loadedCustomMapDetails = map.title + " created by " + map.author +
			" can be downloaded at https://steamcommunity.com/sharedfiles/filedetails/?id=" + map.workshopId;
	}
	else {
		loadedCustomMapDetails = map.title + " created by " + map.author + " can be downloaded at " + map.link;
	}
}

// Returns 0 on if not cooked PC map, 1 if cooked PC map but not map replacement, 2 if map replacement
int CustomMapSupport::updateIfCookedPCMap(std::wstring mapName)
{
	// TODO: What do we do if it's a map replacement and we haven't found the standard maps dir?
	bool isCookedPCMap = standardMapsDir.empty() ? cachedCookedPCFilenames.find(mapName) != cachedCookedPCFilenames.end() : fs::exists(standardMapsDir / (mapName + L".upk"));
	if (isCookedPCMap) {
		if (fs::exists(standardMapsDir / (mapName + L".upk"))) {
			if (mapReplacementMaps.find(boost::to_lower_copy(mapName)) != mapReplacementMaps.end()) {
				if (updateByHash(standardMapsDir / (mapName + L".upk"))) {
					return 2;
				}
			}
		}
		return 1;
	}
	return 0;
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

	loadedCustomMapDetails = filename + " (failed to identify workshop info)";
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

	if (!workshopDir.empty() && fs::exists(workshopDir)) {
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