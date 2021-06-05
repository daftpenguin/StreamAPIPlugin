#include "PushCommands.h"
#include "StreamAPIPlugin.h"
#include "nlohmann/json.hpp"
#include "Ranks.h"
#include "imgui/imgui.h"

#include <chrono>
#include <fstream>
#include <unordered_set>
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

PushCommand::PushCommand(std::string name) : name(name), command(""), enabled(false), anyTimeEnabled(false), casualEnabled(false), rankedEnabled(false),
	extraModesEnabled(false), privateMatchEnabled(false), lanMatchEnabled(false), otherEnabled(false), editing(false)
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

void PushCommand::execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::map<std::string, std::string>& params)
{
	if (checkConditions(cv, gw)) {
		string modifiedCommand = command;
		for (auto it = params.begin(); it != params.end(); ++it) {
			boost::replace_all(modifiedCommand, "${" + it->first + "}", it->second);
		}

		cv->executeCommand("sleep 0; " + modifiedCommand);
	}
	else {
		cv->log("Conditions failed, cannot execute command.");
	}
}

void PushCommand::renderUISetting(std::function<void(std::string& cmdName)> saveFunction, std::function<void(std::string& cmdName)> removeFunction)
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

	if (!editing) {
		ImGui::TextWrapped("Name: %s", name.c_str());
		ImGui::TextWrapped("Command: %s", command.c_str());
	}
	else {
		ImGui::InputText(("Name##" + name).c_str(), nameBuffer, MAX_COMMAND_NAME_SIZE);
		ImGui::InputText(("Command##" + name).c_str(), commandBuffer, MAX_COMMAND_SIZE);

		ImGui::Text("Conditions:");
		ImGui::Checkbox(("Always##" + name).c_str(), &anyTimeEnabled);
		ImGui::SameLine();
		ImGui::Checkbox(("Casual##" + name).c_str(), &casualEnabled);
		ImGui::SameLine();
		ImGui::Checkbox(("Ranked##" + name).c_str(), &rankedEnabled);
		ImGui::SameLine();
		ImGui::Checkbox(("Extra Modes##" + name).c_str(), &extraModesEnabled);
		ImGui::SameLine();
		ImGui::Checkbox(("Private Match##" + name).c_str(), &privateMatchEnabled);
		ImGui::SameLine();
		ImGui::Checkbox(("LAN Match##" + name).c_str(), &lanMatchEnabled);
		ImGui::SameLine();
		ImGui::Checkbox(("Other##" + name).c_str(), &otherEnabled);
	}
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

void PushCommands::renderSettingsUI()
{
	if (!commandsLoaded) {
		ImGui::TextWrapped("Commands haven't yet loaded from json file. These should load rather quickly so if you're seeing this message, check bakkesmod.log for errors.");
	}
	else {
		ImGui::TextWrapped("Click add a command, give it a name, then give it a console command to run (or sequence of console commands separated by ;). Enable the command and set conditions for when the command can be run (\"other\" is any time the other conditions don't apply except \"always\").");
		ImGui::TextWrapped("Advanced users: commands also support named placeholders to allow command configuration to be exposed outside of the plugin. For instance, the command \"cl_rendering_disabled 1; sleep ${duration}; cl_rendering_disabled 0\" will disable frames for the given duration (in milliseconds). You could then make a command in your chat bot that can generate a number from say 1,000 to 10,000, and then append that value as a parameter in the URL request to the API as &duration=<value>. For any added parameters &param=value, the plugin will search the console commands configured in the plugin for ${param} and replace it with value.");

		if (!errorMsg.empty()) {
			ImGui::Separator();
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered), string("Error: " + errorMsg).c_str());
		}

		for (auto it = commands.begin(); it != commands.end(); ++it) {
			it->second.renderUISetting(bind(&PushCommands::saveCommand, this, placeholders::_1), bind(&PushCommands::removeCommand, this, placeholders::_1));
		}
		ImGui::Separator();

		if (ImGui::Button("Add Command")) {
			add();
		}
	}
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
