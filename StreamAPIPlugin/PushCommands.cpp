#include "PushCommands.h"
#include "StreamAPIPlugin.h"
#include "nlohmann/json.hpp"
#include "Ranks.h"
#include "imgui/imgui.h"
#include "Util.h"

#include <chrono>
#include <fstream>
#include <unordered_set>
#include <windows.h>
#include <shellapi.h>

#include <boost/algorithm/string.hpp>

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

void to_json(json& j, const PushCommand& c)
{
	stringstream ss;
	if (c.enabled) ss << "enabled|";
	if (c.anyTimeEnabled) ss << "anyTime|";
	if (c.casualEnabled) ss << "casual|";
	if (c.rankedEnabled) ss << "ranked|";
	if (c.extraModesEnabled) ss << "extraModes|";
	if (c.privateMatchEnabled) ss << "privateMatch|";
	if (c.lanMatchEnabled) ss << "lanMatch|";
	if (c.otherEnabled) ss << "other|";
	string conditions = ss.str();
	conditions = conditions.substr(0, conditions.size() - 1);

	j = json{
		{ "name", c.name },
		{ "command", c.command },
		{ "conditions", conditions }
	};
}

void from_json(const json& j, PushCommand& c)
{
	j.at("name").get_to(c.name);
	j.at("command").get_to(c.command);

	strncpy(c.nameBuffer, c.name.c_str(), MAX_COMMAND_NAME_SIZE);
	strncpy(c.commandBuffer, c.command.c_str(), MAX_COMMAND_SIZE);

	unordered_set<string> conditions;
	boost::split(conditions, j.at("conditions").get<string>(), boost::is_any_of("|"));

	c.enabled = conditions.find("enabled") != conditions.end();
	c.anyTimeEnabled = conditions.find("anyTime") != conditions.end();
	c.casualEnabled = conditions.find("casual") != conditions.end();
	c.rankedEnabled = conditions.find("ranked") != conditions.end();
	c.extraModesEnabled = conditions.find("extraModes") != conditions.end();
	c.privateMatchEnabled = conditions.find("privateMatch") != conditions.end();
	c.lanMatchEnabled = conditions.find("lanMatch") != conditions.end();
	c.otherEnabled = conditions.find("other") != conditions.end();
}

/* PushCommand */

PushCommand::PushCommand(std::string name, std::string command, bool enabled, bool anyTime) : name(name), command(command), enabled(enabled),
	anyTimeEnabled(anyTime), casualEnabled(false), rankedEnabled(false), extraModesEnabled(false), privateMatchEnabled(false),
	lanMatchEnabled(false), otherEnabled(false), editing(false)
{
	strncpy(nameBuffer, name.c_str(), MAX_COMMAND_NAME_SIZE);
	strncpy(commandBuffer, command.c_str(), MAX_COMMAND_SIZE);
}

void PushCommand::save()
{
	editing = false;
	name = string(nameBuffer);
	command = string(commandBuffer);
}

void PushCommand::execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::string& command)
{
	if (checkConditions(cv, gw)) {
		cv->executeCommand("sleep 0; " + command);
	}
	else {
		cv->log("Conditions failed, cannot execute command.");
	}
}

void PushCommand::execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	execute(cv, gw, command);
}

void PushCommand::execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::map<std::string, std::string>& params)
{
	string modifiedCommand = command;
	for (auto it = params.begin(); it != params.end(); ++it) {
		boost::replace_all(modifiedCommand, "${" + it->first + "}", it->second);
	}
	execute(cv, gw, modifiedCommand);
}

void PushCommand::testCommandBuffer(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::map<std::string, std::string>& params)
{
	string modifiedCommand = commandBuffer;
	for (auto it = params.begin(); it != params.end(); ++it) {
		boost::replace_all(modifiedCommand, "${" + it->first + "}", it->second);
	}
	execute(cv, gw, modifiedCommand);
}

bool PushCommand::checkConditions(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	if (!enabled) return false;
	if (anyTimeEnabled) return true;

	if (gw->IsInGame()) {
		auto server = gw->GetCurrentGameState();
		if (server.IsNull()) {
			cv->log("PushCommand: Server is null. Cannot check config for run_cmd. Aborting.");
			return false;
		}
		GameSettingPlaylistWrapper playlistWrapper = server.GetPlaylist();
		if (playlistWrapper.IsNull()) {
			cv->log("PushCommand: PlaylistWrapper is null. Cannot check config for run_cmd. Aborting.");
			return false;
		}

		if (playlistWrapper.IsPrivateMatch())
			return privateMatchEnabled;
		if (playlistWrapper.IsLanMatch())
			return lanMatchEnabled;

		int id = playlistWrapper.GetPlaylistId();
		switch (id) {
		case (int)StreamAPIPlaylist::RANKEDDUEL:
		case (int)StreamAPIPlaylist::RANKEDDOUBLES:
		case (int)StreamAPIPlaylist::RANKEDSTANDARD:
			return rankedEnabled;
		case (int)StreamAPIPlaylist::RANKEDHOOPS:
		case (int)StreamAPIPlaylist::RANKEDSNOWDAY:
		case (int)StreamAPIPlaylist::RANKEDRUMBLE:
		case (int)StreamAPIPlaylist::RANKEDDROPSHOT:
			return extraModesEnabled;
		case (int)StreamAPIPlaylist::DUEL:
		case (int)StreamAPIPlaylist::DOUBLES:
		case (int)StreamAPIPlaylist::STANDARD:
		case (int)StreamAPIPlaylist::CHAOS:
			return casualEnabled;
		default:
			cv->log("PushCommand: Got unexpected playlist ID: " + to_string(id) + ". Assuming it's a casual playlist.");
			return casualEnabled;
		}
	}
	else {
		return otherEnabled;
	}
}

/* Push Commands */

PushCommands::PushCommands() : jsonPath(), commandsLoaded(false), errorMsg("")
{}

void PushCommands::init(std::filesystem::path jsonPath)
{
	this->jsonPath = jsonPath;
	loadJson();
}

void PushCommands::loadJson()
{
	if (fs::exists(jsonPath)) {
		ifstream fin(jsonPath);
		if (fin.fail()) {
			_globalCvarManager->log("PushCommands: failed to load existing commands file");
		}
		else {
			stringstream ss;
			ss << fin.rdbuf();
			try {
				json j = json::parse(ss.str());
				auto it = j.find("commands");
				if (it == j.end()) {
					_globalCvarManager->log("PushCommands: json file missing commands field");
				}
				else {
					it.value().get_to(this->commands);
					commandsLoaded = true;
				}
			}
			catch (json::parse_error& e) {
				_globalCvarManager->log("PushCommands: failed to parse data from json file: " + string(e.what()));
			}
		}

		if (!commandsLoaded) {
			auto now = std::chrono::system_clock::now().time_since_epoch();
			auto epochSeconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
			auto jsonPathCopy = jsonPath.parent_path() / (jsonPath.stem().string() + "-" + to_string(epochSeconds) + ".json");
			fs::copy_file(jsonPath, jsonPathCopy);
			_globalCvarManager->log("Commands failed to load. Creating backup of existing json file to: " + jsonPathCopy.string());
			commandsLoaded = true;
		}
	}
	else {
		_globalCvarManager->log("PushCommands: no existing commands file, starting with no commands configured");
		commandsLoaded = true;
	}
}

void PushCommands::saveJson()
{
	// Save commands as { "commands": { "commandName": { command struct... } } }
	ofstream file;
	file.open(jsonPath);
	if (file.fail()) {
		_globalCvarManager->log("Failed to open json file to save. Error: " + string(strerror(errno)));
	}
	else {
		json j = json{
			{ "commands", commands }
		};
		file << j.dump(2) << endl;
	}
	file.close();
}

void PushCommands::add()
{
	// Get a free command name so we can add it to commands
	string cmdName = "cmd1";
	for (int cmdNum = 1; commands.find(cmdName) != commands.end(); cmdName = "cmd" + to_string(++cmdNum));
	commands.emplace(cmdName, PushCommand(cmdName));
}

void PushCommands::removeCommand(std::string& cmdName)
{
	auto it = commands.find(cmdName);
	if (it == commands.end()) {
		setUIError("Remove failed: cannot find command named " + string(cmdName));
		return;
	}
	commands.erase(it);
	saveJson();
}

void PushCommands::saveCommand(std::string& origCmdName)
{
	auto it = commands.find(origCmdName);
	if (it == commands.end()) {
		setUIError("Save failed: cannot find original command named " + string(origCmdName));
		return;
	}

	PushCommand& cmd = it->second;
	if (cmd.name.compare(cmd.nameBuffer) != 0) {
		auto checkExists = commands.find(cmd.nameBuffer);
		if (checkExists != commands.end()) {
			setUIError("Save failed: A command with the name " + string(cmd.nameBuffer) + " already exists. Command names must be unique.");
			return;
		}
		commands.emplace(cmd.nameBuffer, cmd);
		commands.erase(it);
	}

	cmd.save();
	saveJson();
}

void PushCommands::setUIError(std::string errorMsg)
{
	this->errorMsg = errorMsg;
	_globalCvarManager->log(errorMsg);
}

void PushCommands::execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::string query)
{
	// query is the URL query with params in any order: param1=1&cmd_name=<command name>&param2=2...
	vector<string> fields;
	boost::split(fields, query, boost::is_any_of("&"));

	map<string, string> params;
	string cmdName;

	for (auto& field : fields) {
		auto equalIdx = field.find_first_of('=');
		string key, value;
		if (equalIdx == string::npos) {
			key = field;
			value = "1";
			if (key.compare("cmd_name") == 0) {
				cv->log("PushCommand: missing cmd_name value in query: " + query);
				return;
			}
		}
		else {
			key = field.substr(0, equalIdx);
			value = field.substr(equalIdx + 1);
		}

		if (key.compare("cmd_name") == 0) {
			cmdName = value;
		}
		else {
			params.emplace(key, value);
		}
	}

	auto it = commands.find(cmdName);
	if (it == commands.end()) {
		cv->log("PushCommand: command not found for name " + cmdName + " found in query: " + query);
		return;
	}

	it->second.execute(cv, gw, params);
}

/* GUI Related Methods */

struct SampleCommand {
	std::string name;
	int hideSettingsSecs;
	PushCommand command;
	std::map<std::string, std::string> testParams;
};

std::vector<SampleCommand> sampleCommands({
	SampleCommand{ "Troll images for ~5 seconds", 5, PushCommand("show_image", "streamapi_show_image dickbutt.png id=img4,scale=0.3,y_offset=300; sleep 250; streamapi_show_image dickbutt.png id=img1,scale=0.5; sleep 250; streamapi_show_image dickbutt.png id=img2,scale=0.3,x_offset=-300; sleep 250; streamapi_show_image dickbutt.png id=img3,scale=0.3,x_offset=300; sleep 250; streamapi_show_image dickbutt.png id=img5,scale=0.3,y_offset=-300; sleep 2500; streamapi_hide_image img1; sleep 250; streamapi_hide_image img2; sleep 250; streamapi_hide_image img3; sleep 250; streamapi_hide_image img4; sleep 250; streamapi_hide_image img5;", true, true) },
	SampleCommand{ "Black Screen for 2 seconds", 2, PushCommand("black_screen", "cl_rendering_disabled 1; sleep 2000; cl_rendering_disabled 0", true, true) },
	SampleCommand{ "Hide UI for 2 seconds (boost meter and scoreboard in game)", 2, PushCommand("hide_ui", "cl_rendering_scaleform_disabled 1; sleep 2000; cl_rendering_scaleform_disabled 0", true, true) },
	SampleCommand{ "Set item mode code", 0, PushCommand("itemmod_code", "cl_itemmod_enabled 1; cl_itemmod_code \"${code};\"", true, true), map<string, string>({{ "code", "Awo2FVyATOibGWGjDuPTZIoPy5yIgEUETTdxhyrzimMmAEDA//8/AA==" }}) },
	});


void PushCommands::renderSettingsUI(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, bool useWebSocket)
{
	if (!commandsLoaded) {
		ImGui::TextWrapped("Commands haven't yet loaded from json file. These should load rather quickly so if you're seeing this message, check bakkesmod.log for errors.");
	}
	else {
		ImGui::TextWrapped("This is like a library of named commands that can be activated by name through the API.");
		ImGui::TextWrapped("Configure commands below, then expose them through your chat bot. See commands page for a tool that can help with this.");

		if (ImGui::Button("Copy Commands Config##CopyActionCommands")) {
			json j = json{
				{ "commands", commands }
			};
			ImGui::SetClipboardText(j.dump().c_str());
		}

		ImGui::SameLine();
		string commandsUrl = "https://www.daftpenguin.com/rocket-league/stream-api/commands" + string((useWebSocket ? "" : "/local"));
		if (ImGui::Button("Open Command Setup Page (in browser)")) {
			ShellExecute(0, 0, stringToWstring(commandsUrl).c_str(), 0, 0, SW_SHOW);
		}
		ImGui::SameLine();
		ImGui::InputText("##CommandsURL", (char*)commandsUrl.c_str(), commandsUrl.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);

		ImGui::Separator();

		ImGui::Text("1. Add command");
		ImGui::Text("2. Name it");
		ImGui::TextWrapped("3. Set console command(s) to run when it's called (F6 and findcvar helps to experiment)");
		ImGui::TextWrapped("4. Set conditions that control when the command is allowed to be executed (\"other\" is basically workshops, freeplay, main menu)");
		ImGui::Text("5. Save command");
		ImGui::TextWrapped("Advanced users: commands support named placeholders like ${placeholder} to allow command configuration to be exposed through API. Please only use letters, numbers, and underscores in placeholder names, and ${cmd_name} and ${cmd} are reserved. Try adding the black screen sample command, edit it, then replace \"sleep 2000\" with \"sleep ${duration}\". Test the command by putting \"duration=5000\" in the params box then click test button (sleep is in milliseconds).");

		ImGui::Separator();

		ImGui::Text("Sample Commands:");

		for (auto& command : sampleCommands) {
			ImGui::Text(command.name.c_str());
			ImGui::SameLine();
			if (ImGui::Button(("Test##TestSampleCommand" + command.name).c_str())) {
				if (command.hideSettingsSecs > 0) {
					cv->executeCommand("togglemenu settings; sleep " + to_string(command.hideSettingsSecs) + "000; togglemenu settings");
				}
				command.command.execute(cv, gw, command.testParams);

			}
			ImGui::SameLine();
			if (ImGui::Button(("Add##AddSampleCommand" + command.name).c_str())) {
				// Find available name, then add it
				if (commands.find(command.command.name) == commands.end()) {
					// TODO: command.command.command? ffs we should probably leverage inheritence or some shit...
					commands.emplace(command.command.name, PushCommand(command.command.name, command.command.command, true, true));
				}
				else {
					string cmdName = command.command.name + "1";
					for (int cmdNum = 1; commands.find(cmdName) != commands.end(); cmdName = command.command.name + to_string(++cmdNum));
					commands.emplace(cmdName, PushCommand(cmdName, command.command.command, true, true));
				}
				saveJson();
			}
		}

		if (!errorMsg.empty()) {
			ImGui::Separator();
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered), string("Error: " + errorMsg).c_str());
		}

		for (auto it = commands.begin(); it != commands.end(); ++it) {
			it->second.renderUISetting(cv, gw, bind(&PushCommands::saveCommand, this, placeholders::_1), bind(&PushCommands::removeCommand, this, placeholders::_1));
		}
		ImGui::Separator();

		if (ImGui::Button("Add Command")) {
			add();
		}
	}
}


void PushCommand::renderUISetting(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw,
	std::function<void(std::string& cmdName)> saveFunction, std::function<void(std::string& cmdName)> removeFunction)
{
	ImGui::Separator();

	if (editing) {
		if (ImGui::Button(("Save##" + name).c_str())) {
			saveFunction(name);
		}
	}
	else {
		if (ImGui::Button(("Edit##" + name).c_str())) {
			editing = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(("Remove##" + name).c_str())) {
		removeFunction(name);
	}
	ImGui::SameLine();
	if (enabled) {
		if (ImGui::Button(("Disable##" + name).c_str())) {
			enabled = false;
			saveFunction(name);
		}
	}
	else {
		if (ImGui::Button(("Enable##" + name).c_str())) {
			enabled = true;
			saveFunction(name);
		}
	}

	ImGui::SameLine();
	ImGui::PushItemWidth(200.0f);
	ImGui::InputTextWithHint(("Test params##TestParams" + name).c_str(), "Hint: p1=v1,p2=v2", testParams, MAX_TEST_PARAMS_SIZE);
	ImGui::SameLine();
	if (ImGui::Button(("Test##TestCommandInLibrary" + name).c_str())) {
		map<string, string> params;
		vector<string> fields;
		boost::split(fields, testParams, boost::is_any_of(","));
		for (auto& field : fields) {
			auto equalIdx = field.find_first_of('=');
			if (equalIdx == string::npos) {
				params.emplace(field, "1");
			}
			else {
				params.emplace(field.substr(0, equalIdx), field.substr(equalIdx + 1));
			}
		}
		testCommandBuffer(cv, gw, params);
	}

	if (!editing) {
		ImGui::TextWrapped("Name: %s", name.c_str());
		ImGui::TextWrapped("Command: %s", command.c_str());
	}
	else {
		ImGui::InputText(("Name##" + name).c_str(), nameBuffer, MAX_COMMAND_NAME_SIZE);
		ImGui::InputText(("Command##" + name).c_str(), commandBuffer, MAX_COMMAND_SIZE);

		ImGui::Text("Conditions:"); ImGui::SameLine();
		ImGui::Checkbox(("Always##" + name).c_str(), &anyTimeEnabled); ImGui::SameLine();
		ImGui::Checkbox(("Casual##" + name).c_str(), &casualEnabled); ImGui::SameLine();
		ImGui::Checkbox(("Ranked##" + name).c_str(), &rankedEnabled); ImGui::SameLine();
		ImGui::Checkbox(("Extra Modes##" + name).c_str(), &extraModesEnabled); ImGui::SameLine();
		ImGui::Checkbox(("Private Match##" + name).c_str(), &privateMatchEnabled); ImGui::SameLine();
		ImGui::Checkbox(("LAN Match##" + name).c_str(), &lanMatchEnabled); ImGui::SameLine();
		ImGui::Checkbox(("Other##" + name).c_str(), &otherEnabled);
	}
}