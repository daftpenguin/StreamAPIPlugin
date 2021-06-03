#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"
#include "Loadout.h"
#include "Ranks.h"
#include "WebSocket.h"
#include "imgui/imgui.h"
#include "CustomMapSupport.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>

#include "fmt/core.h"
#include "fmt/ranges.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH); // "." stringify(VERSION_BUILD);

const std::string PREVIEW_TEAMNUM_CHANGED_EVENT = "Function TAGame.CarPreviewActor_TA.SetTeamIndex";

const std::string LOADOUT_CHANGED_EVENT = "Function TAGame.CarMeshComponent_TA.SetLoadout";
const std::string SENS_CHANGED_EVENT = "Function TAGame.GFxData_Controls_TA.HandleActiveBindingsChanged";
const std::string CAMERA_CHANGED_EVENT = "Function TAGame.GFxData_Settings_TA.OnCameraOptionChanged";
const std::string BINDINGS_CHANGED_EVENT = "Function TAGame.PlayerInput_Menu_TA.OnActiveBindingsChanged";
const std::string CUSTOM_TRAINING_LOADED_EVENT = "Function TAGame.GameEvent_TrainingEditor_TA.StartPlayTest";
const std::string MAP_LOADED_EVENT = "Function TAGame.LoadingScreen_TA.HandlePreLoadMap";

// Should probably just hook on UserSetting change for all but loadout...
const std::string VIDEO_CHANGED_EVENTS[] = {
	"Function TAGame.GFxData_Settings_TA.OnAdvancedVideoOptionChanged",
	"Function TAGame.GFxData_Settings_TA.ApplyViewportSettings",
	"Function TAGame.GFxData_Settings_TA.SetWeatherFX",
	"Function TAGame.GFxData_Settings_TA.SetLensFlares",
	"Function TAGame.GFxData_Settings_TA.SetEffectIntensity",
	"Function TAGame.PostProcessManager_TA.ApplyPostProcessTypeOverride",
};

const std::string CAMERA_INVERT_SWIVEL_CHANGED_EVENT = "Function TAGame.GFxData_Settings_TA.SetInvertSwivelPitch";
const std::string CAMERA_SHAKE_CHANGED_EVENT = "Function TAGame.GFxData_Settings_TA.SetCameraShake";
const std::string RANKS_UPDATE_EVENT = "Function TAGame.GameEvent_Soccar_TA.EventMatchWinnerSet";

const unsigned int MAX_REPORT_SIZE = 20000 * 512;
constexpr auto REPORT_SERVER_URL = "https://www.daftpenguin.com";
//constexpr auto REPORT_SERVER_URL = "http://localhost:9000";

const short BAKKESMOD_VERSION = 140;

template<typename S, typename... Args>
void LOG(const S& format_str, Args&&... args)
{
	_globalCvarManager->log(fmt::format(format_str, args...));
}

class StreamAPIPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow
{
	virtual void onLoad();
	virtual void onUnload();

private:
	void toggleBotSupport();

	void getLoadout();
	void getSens();
	void getCamera();
	void getBindings();
	void getVideo();
	void getTrainingPack();

	int previewTeamNum = 0;
	Loadout loadout;
	std::string sensStr;
	std::string cameraStr;
	std::string kbmBindingsStr;
	std::string ds4BindingsStr;
	std::string xboxBindingsStr;
	std::string videoStr;
	std::string trainingPackStr;

	std::unique_ptr<MMRNotifierToken> mmrNotifierToken;
	Ranks ranks;
	bool updateRankOnNextNotification = false;
	
	CustomMapSupport customMapSupport;

private:
	void onDump(std::vector<std::string> params);
	void SubmitReport();
	void SubmitReportThread();

	std::mutex stopStartLock; // Lock needed to synchronize between starting and stopping external and local bot support

private:
	/* Http Server */
	// TODO: Wrap this up in a class like WebSocket
	int serverPort;
	std::string serverStatus;
	httplib::Server httpServer;
	std::thread httpThread;
	bool serverIsRunning;
	std::atomic<bool> abortHttpStart; // Server might not have started yet when attempting to stop it (happens during onLoad when port changed and useWebSocket is updated from config.cfg)

	void startHttpServer(int port);
	void stopHttpServer();
	void runHttpServer(int port);
	void configureHttpServer();

private:
	/* Commands */
	std::string loadoutCommand(std::string args);
	std::string sensCommand(std::string args);
	std::string cameraCommand(std::string args);
	std::string bindingsCommand(std::string args);
	std::string videoCommand(std::string args);
	std::string trainingCommand(std::string args);
	std::string rankCommand(std::string args);
	std::string workshopCommand(std::string args);
	std::string mapCommand(std::string args);

	std::map<std::string, std::function<std::string(std::string args)> > commandNameToCommand;

	std::string outputSeparator = " | ";
	bool showSlotName = true;
	bool showBMCode = true;

private:
	/* WebSocket */
	bool useWebSocket;
	std::filesystem::path tokenFile;
	WebSocket webSocket;

private:
	/* GUI */
	ImGuiContext* imguiCtx = nullptr;
	char guiServerPort[6];
	bool guiShowBadToken = false;
	std::string guiServerTestStatus;
	std::string guiWebSocketStatus;
	std::string guiWebSocketTestStatus;
	std::chrono::system_clock::time_point guiWebSocketStatusLastChecked;
	char guiReportDetails[1024];
	std::string guiReportStatus;

	void RenderSettings();
	std::string GetPluginName();
	void SetImGuiContext(uintptr_t ctx);
};