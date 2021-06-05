#pragma once
#define WIN32_LEAN_AND_MEAN

#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <filesystem>

#define MAX_COMMAND_NAME_SIZE 128
#define MAX_COMMAND_SIZE 1024

class PushCommand {
public:
	PushCommand() : PushCommand("") { };
	PushCommand(std::string name);
	void save();
	void execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::map<std::string, std::string>& params);
	void renderUISetting(std::function<void(std::string& cmdName)> saveFunction, std::function<void (std::string& cmdName)> removeFunction);

	std::string name;
	char nameBuffer[MAX_COMMAND_NAME_SIZE] = { 0 }; // For UI
	std::string command;
	char commandBuffer[MAX_COMMAND_SIZE] = { 0 }; // For UI

	bool enabled;
	bool anyTimeEnabled;
	bool casualEnabled;
	bool rankedEnabled;
	bool extraModesEnabled;
	bool privateMatchEnabled;
	bool lanMatchEnabled;
	bool otherEnabled;

private:
	bool checkConditions(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);

	bool editing;
};

class PushCommands {
public:
	PushCommands();
	void init(std::filesystem::path jsonPath);
	void renderSettingsUI();
	void execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::string query);

private:
	void loadJson();
	void saveJson();
	void add();
	void removeCommand(std::string& cmdName);
	void saveCommand(std::string& origCmdName);
	void setUIError(std::string errorMsg);

	std::filesystem::path jsonPath;
	bool commandsLoaded;
	std::map<std::string, PushCommand> commands;
	std::string errorMsg;
};