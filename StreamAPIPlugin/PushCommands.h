#pragma once
#define WIN32_LEAN_AND_MEAN

#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <filesystem>

#define MAX_COMMAND_NAME_SIZE 128
#define MAX_COMMAND_SIZE 4096
#define MAX_TEST_PARAMS_SIZE 512

class PushCommand {
public:
	PushCommand(std::string name, std::string command, bool enabled, bool anyTime);
	PushCommand(std::string name) : PushCommand(name, "", false, false) {};
	PushCommand() : PushCommand("") { };
	void save();
	void execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);
	void execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::map<std::string, std::string>& params);
	void testCommandBuffer(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::map<std::string, std::string>& params);
	void renderUISetting(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw,
		std::function<void(std::string& cmdName)> saveFunction, std::function<void (std::string& cmdName)> removeFunction);

	std::string name;
	char nameBuffer[MAX_COMMAND_NAME_SIZE] = { 0 }; // For UI
	std::string command;
	char commandBuffer[MAX_COMMAND_SIZE] = { 0 }; // For UI

	char testParams[MAX_TEST_PARAMS_SIZE] = { 0 }; // For UI

	bool enabled;
	bool anyTimeEnabled;
	bool casualEnabled;
	bool rankedEnabled;
	bool extraModesEnabled;
	bool privateMatchEnabled;
	bool lanMatchEnabled;
	bool otherEnabled;

private:
	void execute(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, std::string& command);
	bool checkConditions(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);

	bool editing;
};

class PushCommands {
public:
	PushCommands();
	void init(std::filesystem::path jsonPath);
	void renderSettingsUI(std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw, bool useWebSocket);
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