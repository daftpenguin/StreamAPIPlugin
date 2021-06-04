#include "StreamAPIPlugin.h"

#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>

#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
namespace fs = std::filesystem;

#define DEFAULT_SERVER_PORT "10111"
#define DEFAULT_BUFLEN 512

#define APPEND_STRING_WITH_ITEM(oss, separator, showSlotName, slotName, itemString) { \
	oss << separator; \
	if (showSlotName) oss << slotName << ": "; \
	oss << itemString; \
}

BAKKESMOD_PLUGIN(StreamAPIPlugin, "Stream API Plugin", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;


void StreamAPIPlugin::onLoad()
{
	_globalCvarManager = cvarManager;

	// Initialize some things
	tokenFile = gameWrapper->GetDataFolder() / "streamapi" / "token.txt";
	webSocket.init(tokenFile);
	webSocket.loadTokenFromFile(tokenFile);
	guiWebSocketStatusLastChecked = std::chrono::system_clock::now();
	configureHttpServer();
	customMapSupport.init(gameWrapper->IsUsingSteamVersion(), gameWrapper->GetDataFolder() / "streamapi" / "maps.json");
	
	cvarManager->registerNotifier("streamapi_dump", bind(&StreamAPIPlugin::onDump, this, std::placeholders::_1), "dumps loadout data to console", PERMISSION_ALL);

	cvarManager->registerNotifier("streamapi_reload_token", [this](vector<string> params) {
		webSocket.loadTokenFromFile(tokenFile);
		if (useWebSocket) {
			webSocket.start();
		}
		}, "Reloads token from file", PERMISSION_ALL);
	
	gameWrapper->HookEventWithCaller<ServerWrapper>("Function TAGame.CarPreviewActor_TA.SetTeamIndex", [this](ServerWrapper sw, void* params, string eventName) {
		struct SetLoadoutSetParams {
			void* profile;
			int teamNum;
		};

		if (sw.IsNull())
			return;

		int teamNum = reinterpret_cast<SetLoadoutSetParams*>(params)->teamNum;
		if (teamNum == 0 || teamNum == 1)
			previewTeamNum = teamNum;
		});

	auto portVar = cvarManager->registerCvar("streamapi_port", DEFAULT_SERVER_PORT, "Port for HTTP API to listen for requests", true, true, 1024, true, 49151, true);
	auto useWebSocketVar = cvarManager->registerCvar("streamapi_use_external_bot", "0", "Pushes data to external API to be called from external bots", true, true, 0, true, 1, true);
	serverPort = portVar.getIntValue();
	strncpy(guiServerPort, to_string(serverPort).c_str(), sizeof(guiServerPort));
	useWebSocket = useWebSocketVar.getBoolValue();

	portVar.addOnValueChanged([this](std::string, CVarWrapper cvar) {
		auto newServerPort = cvar.getIntValue();
		if (newServerPort < 1024 || newServerPort > 49151 || newServerPort == serverPort) {
			return;
		}

		lock_guard<mutex> lock(stopStartLock);
		if (!useWebSocket) {
			cvarManager->log("Restarting HTTP API server on new port");
			startHttpServer(newServerPort);
		}
	});

	useWebSocketVar.addOnValueChanged([this](std::string, CVarWrapper cvar) {
		lock_guard<mutex> lock(stopStartLock);
		if (cvar.getBoolValue() != useWebSocket) {
			toggleBotSupport();
		}
	});

	{
		lock_guard<mutex> lock(stopStartLock);
		if (useWebSocket) {
			cvarManager->log("Starting web socket");
			webSocket.start();
		}
		else {
			cvarManager->log("Starting HTTP API server");
			startHttpServer(serverPort);
		}
	}

	trainingPackStr = "No training pack";
	webSocket.setData("training", trainingPackStr);

	getLoadout();
	getSens();
	getCamera();
	getBindings();
	getVideo();
	getTrainingPack();
	ranks.getRanks(gameWrapper);
	webSocket.setData("workshop", customMapSupport.getCustomMap());
	webSocket.setData("map", customMapSupport.getMap());

	mmrNotifierToken = gameWrapper->GetMMRWrapper().RegisterMMRNotifier([this](UniqueIDWrapper id) {
		// Workaround since Epic version doesn't appear to get MMR notifications for local user
		if (gameWrapper->IsUsingEpicVersion() && updateRankOnNextNotification) {
			updateRankOnNextNotification = false;
			gameWrapper->SetTimeout([this](GameWrapper* gw) {
				ranks.getRanks(gameWrapper);
				webSocket.setData("rank", ranks.toString("json", cvarManager));
				}, 3.0f);
		}

		if (gameWrapper->IsUsingSteamVersion() && id.GetIdString().compare(gameWrapper->GetUniqueID().GetIdString()) == 0) {
			ranks.updateRank(gameWrapper);
			webSocket.setData("rank", ranks.toString("json", cvarManager));
			updateRankOnNextNotification = false;
		}
		});
	
	gameWrapper->SetTimeout([this](GameWrapper* gw) { // Doesn't appear to update when ranks are initially retrieved
		ranks.getRanks(gameWrapper);
		webSocket.setData("rank", ranks.toString("json", cvarManager));
		}, 10.0f);

	gameWrapper->HookEventPost(LOADOUT_CHANGED_EVENT, [this](string eventName) { getLoadout(); });
	gameWrapper->HookEventPost(SENS_CHANGED_EVENT, [this](string eventName) { getSens(); });
	gameWrapper->HookEventPost(CAMERA_CHANGED_EVENT, [this](string eventName) { getCamera(); });
	gameWrapper->HookEventPost(BINDINGS_CHANGED_EVENT, [this](string eventName) { getBindings(); });
	for (auto evt : VIDEO_CHANGED_EVENTS) {
		gameWrapper->HookEventPost(evt, [this](string eventName) { cvarManager->log(eventName); getVideo(); });
	}
	gameWrapper->HookEventPost(CUSTOM_TRAINING_LOADED_EVENT, [this](string eventName) { getTrainingPack();  });
	gameWrapper->HookEventWithCaller<ServerWrapper>(MAP_LOADED_EVENT, [this](ServerWrapper sw, void* params, string eventName) {
		cvarManager->log(eventName);
		auto mapName = wstring(*reinterpret_cast<wchar_t**>(params));
		auto oldMapStr = customMapSupport.getMap();
		auto oldCustomMapStr = customMapSupport.getCustomMap();
		customMapSupport.updateMap(mapName);
		auto newMapStr = customMapSupport.getMap();
		if (oldMapStr.compare(newMapStr) != 0) {
			webSocket.setData("map", newMapStr);
		}
		auto newCustomMapStr = customMapSupport.getCustomMap();
		if (oldCustomMapStr.compare(newCustomMapStr) != 0) {
			webSocket.setData("workshop", newCustomMapStr);
		}
		});
	gameWrapper->HookEventPost(RANKS_UPDATE_EVENT, [this](string eventName) { updateRankOnNextNotification = true; });

	commandNameToCommand["loadout"] = std::bind(&StreamAPIPlugin::loadoutCommand, this, std::placeholders::_1);
	commandNameToCommand["sens"] = std::bind(&StreamAPIPlugin::sensCommand, this, std::placeholders::_1);
	commandNameToCommand["camera"] = std::bind(&StreamAPIPlugin::cameraCommand, this, std::placeholders::_1);
	commandNameToCommand["bindings"] = std::bind(&StreamAPIPlugin::bindingsCommand, this, std::placeholders::_1);
	commandNameToCommand["video"] = std::bind(&StreamAPIPlugin::videoCommand, this, std::placeholders::_1);
	commandNameToCommand["training"] = std::bind(&StreamAPIPlugin::trainingCommand, this, std::placeholders::_1);
	commandNameToCommand["rank"] = std::bind(&StreamAPIPlugin::rankCommand, this, std::placeholders::_1);
	commandNameToCommand["workshop"] = std::bind(&StreamAPIPlugin::workshopCommand, this, std::placeholders::_1);
	commandNameToCommand["map"] = std::bind(&StreamAPIPlugin::mapCommand, this, std::placeholders::_1);

	gameWrapper->RegisterDrawable(std::bind(&StreamAPIPlugin::render, this, std::placeholders::_1));
	
	/* Push Commands */
	cvarManager->registerNotifier("streamapi_submit_report", [this](vector<string> params) {
		SubmitReport("streamapi_submit_report", false);
		}, "submits a report with bakkesmod.log to website for debugging issues", PERMISSION_ALL);

	cvarManager->registerNotifier("streamapi_show_image", [this](vector<string> params) {
		auto imgPath = fs::path(params.size() > 1 ? params[1] : "dickbutt.png");
		if (!fs::exists(imgPath)) {
			imgPath = gameWrapper->GetDataFolder() / "streamapi" / imgPath;
		}
		if (!fs::exists(imgPath)) {
			cvarManager->log(L"Path to " + imgPath.wstring() + L" does not exist");
			return;
		}

		string imageId = imgPath.string();
		Vector2 position{ -1, -1 };
		Vector2 offset{ -1, -1 };
		float scale = 1.0;

		if (params.size() > 2) {
			vector<string> fields;
			boost::split(fields, params[2], boost::is_any_of(","));
			for (auto& field : fields) {
				vector<string> keyValue;
				boost::split(keyValue, field, boost::is_any_of("="));
				if (keyValue.size() != 2) {
					cvarManager->log("image settings must be of the form: setting=value. id value cannot contain any commas or = signs.");
					return;
				}
				try {
					if (keyValue[0].compare("id") == 0) {
						imageId = keyValue[1];
					}
					else if (keyValue[0].compare("scale") == 0) {
						scale = stof(keyValue[1]);
					}
					else if (keyValue[0].compare("x") == 0) {
						position.X = stoi(keyValue[1]);
					}
					else if (keyValue[0].compare("y") == 0) {
						position.Y = stoi(keyValue[1]);
					}
					else if (keyValue[0].compare("offset_x")) {
						offset.X = stoi(keyValue[1]);
					}
					else if (keyValue[0].compare("offset_y")) {
						offset.Y = stoi(keyValue[1]);
					}
					else {
						cvarManager->log("Ignoring unknown setting: '" + keyValue[0] + "'");
					}
				}
				catch (...) {
					cvarManager->log("x, y, offset_x, and offset_y values must specifically be integers (non-decimal/whole numbers) and scale must be a floating point number");
					return;
				}
			}
		}

		auto it = images.find(imageId);
		if (it != images.end()) {
			it->second.position = position;
			it->second.offset = offset;
			it->second.scale = scale;
			it->second.show = true;
		}
		else {
			auto img = Image{ std::make_shared<ImageWrapper>(imgPath, true), position, offset, scale, true };
			img.image->LoadForCanvas();
			images.emplace(imageId, img);
		}		
		}, "shows image on screen (PNG only), params= <img_path> " \
			"<optional id=img_id,scale=float/number,x=integer,y=integer,offset_x=integer,offset_y=integer> (img_id cannot contain spaces, commas, or = signs)",
			PERMISSION_ALL);

	cvarManager->registerNotifier("streamapi_hide_image", [this](vector<string> params) {
		string imageId = "dickbutt.png";
		if (params.size() > 1)
			imageId = params[1];

		auto it = images.find(imageId);
		if (it != images.end()) {
			it->second.show = false;
			return;
		}

		it = images.find((gameWrapper->GetDataFolder() / "streamapi" / imageId).string());
		if (it != images.end()) {
			it->second.show = false;
			return;
		}

		cvarManager->log("Failed to find image with id: " + imageId + ". Cannot hide image.");
		}, "hides image shown by streamapi_show_image, params= <img_id> (image path if no img_id specified to show command)", PERMISSION_ALL);

	cvarManager->registerNotifier("streamapi_run_cmd", bind(&StreamAPIPlugin::runPushCommand, this, placeholders::_1),
		"runs given command with optional config to only run in specific situations", PERMISSION_ALL);
}

void StreamAPIPlugin::onUnload()
{
	webSocket.stop();
	stopHttpServer();
}

void StreamAPIPlugin::toggleBotSupport()
{
	useWebSocket = !useWebSocket;
	if (!useWebSocket) {
		cvarManager->log("Starting HTTP API server and stopping web socket");
		webSocket.stop();
		startHttpServer(serverPort);
	}
	else {
		cvarManager->log("Starting web socket and stopping HTTP API server");
		stopHttpServer();
		webSocket.start();
	}
}

void StreamAPIPlugin::getLoadout()
{
	cvarManager->log("Updating loadout");

	bool teamNum = previewTeamNum;

	if (gameWrapper->IsInGame()) {
		auto car = gameWrapper->GetLocalCar();
		if (!car.IsNull()) {
			teamNum = car.GetTeamNum2();
		}
	}

	string loadoutStr = this->loadout.toString();
	this->loadout.clear();
	this->loadout.load(teamNum, cvarManager, gameWrapper);
	if (loadoutStr.compare(this->loadout.toString()) != 0) {
		auto ls = this->loadout.getItemString("json", ",", true, true);
		webSocket.setData("loadout", ls);
	}
}

void StreamAPIPlugin::getSens()
{
	cvarManager->log("Updating sens");

	string oldSensStr = sensStr;

	stringstream oss;
	oss.precision(2);

	gameWrapper->GetSettings();
	auto gp = gameWrapper->GetSettings().GetGamepadSettings();
	oss << "Steering: " << fixed << gp.SteeringSensitivity
		<< outputSeparator << "Aerial: " << fixed << gp.AirControlSensitivity
		<< outputSeparator << "Deadzone: " << fixed << gp.ControllerDeadzone
		<< outputSeparator << "Dodge Deadzone: " << fixed << gp.DodgeInputThreshold;
	sensStr = oss.str();

	if (sensStr.compare(oldSensStr) != 0) {
		webSocket.setData("sens", sensStr);
	}
}

void StreamAPIPlugin::getCamera()
{
	cvarManager->log("Updating camera");

	string oldCameraStr = cameraStr;

	stringstream oss;
	oss.precision(2);
	auto camera = gameWrapper->GetSettings().GetCameraSettings();
	oss << "FOV: " << (int) camera.FOV
		<< outputSeparator << "Distance: " << (int) camera.Distance
		<< outputSeparator << "Height: " << (int) camera.Height
		<< outputSeparator << "Angle: " << (int) camera.Pitch
		<< outputSeparator << "Stiffness: " << fixed << camera.Stiffness
		<< outputSeparator << "Swivel: " << fixed << camera.SwivelSpeed
		<< outputSeparator << "Transition: " << fixed << camera.TransitionSpeed;
	cameraStr = oss.str();

	if (cameraStr.compare(oldCameraStr) != 0) {
		webSocket.setData("camera", cameraStr);
	}
}

std::map<std::string, std::string> antiAliasValueToName({
	{ "0", "Off" },
	{ "2", "FXAA Low" },
	{ "4", "FXAA Medium" },
	{ "6", "FXAA High" },
	{ "7", "MLAA" },
	{ "8", "SMAA" },
	});

std::map<std::string, std::string> textureDetailToName({
	{ "TexturesHigher", "High Quality" },
	{ "TexturesHigh", "Quality" },
	{ "TexturesMedium", "Performance" },
	{ "TexturesLow", "High Performance" },
	});

std::map<int, std::string> windowModeToName({
	{ 0, "Fullscreen" },
	{ 1, "Windowed" },
	{ 2, "Borderless" },
	});

std::map<std::string, std::string> videoOptionsRename({
	{ "RenderDetail", "Render Detail" }
	});

void StreamAPIPlugin::getVideo()
{
	cvarManager->log("Updating video settings");

	string oldVideoStr = videoStr;

	auto settings = gameWrapper->GetSettings().GetVideoSettings();

	// Missing: Render Quality, World Detail, Particle Detail, Effect Intensity, High Quality Shaders, Ambient Occlusion, Depth of Field, Bloom, Dynamic Shadows, Motion Blur

	stringstream oss;
	
	if (settings.MaxFPS == 10000) {
		oss << "FPS: Uncapped" << outputSeparator;
	}
	else {
		oss << "FPS: " << settings.MaxFPS << outputSeparator;
	}
	oss << settings.Resolution << outputSeparator;
	oss << windowModeToName[settings.WindowMode] << outputSeparator;

	if (settings.bVsync) oss << "Vsync" << outputSeparator;
	if (settings.bShowLensFlares) oss << "Lens Flares" << outputSeparator;
	if (settings.bShowLightShafts) oss << "Light Shafts" << outputSeparator;
	if (settings.bShowWeatherFX) oss << "Weather FX" << outputSeparator;
	if (settings.bTranslucentArenaShaders) oss << "Transparent Goalposts" << outputSeparator;
	
	for (auto it = settings.VideoOptions.begin(); it != settings.VideoOptions.end(); ++it) {
		if (it->first.compare("AntiAlias") == 0) {
			auto aa = antiAliasValueToName.find(it->second);
			oss << "Anti-Alias: " << (aa != antiAliasValueToName.end() ? aa->second : it->second) << outputSeparator;
		}
		else if (it->first.compare("TextureDetail") == 0) {
			auto td = textureDetailToName.find(it->second);
			oss << "Texture Detail: " << (td != textureDetailToName.end() ? td->second : it->second) << outputSeparator;
		}
		else {
			auto rename = videoOptionsRename.find(it->first);
			oss << (rename != videoOptionsRename.end() ? rename->second : it->first) << ": " << it->second << outputSeparator;
		}
	}
	
	videoStr = oss.str();
	videoStr = videoStr.substr(0, videoStr.size() - outputSeparator.size()) + " (not all settings suppported yet)";

	if (videoStr.compare(oldVideoStr) != 0) {
		webSocket.setData("video", videoStr);
	}
}

unordered_map<string, string> ds4Renames({
	{ "XboxTypeS_A", "Cross" },
	{ "XboxTypeS_B", "Circle" },
	{ "XboxTypeS_X", "Square" },
	{ "XboxTypeS_Y", "Triangle" },
	{ "XboxTypeS_LeftShoulder", "L1" },
	{ "XboxTypeS_LeftTriggerAxis", "L2" },
	{ "XboxTypeS_RightShoulder", "R1" },
	{ "XboxTypeS_RightTriggerAxis", "R2" },
	{ "XboxTypeS_DPad_Up", "D-Up" },
	{ "XboxTypeS_DPad_Down", "D-Down" },
	{ "XboxTypeS_DPad_Left", "D-Left" },
	{ "XboxTypeS_DPad_Right", "D-Right" },
	{ "XboxTypeS_Start", "Options" },
	{ "XboxTypeS_Back", "Share" },
	{ "XboxTypeS_LeftY", "LS Y-axis" },
	{ "XboxTypeS_LeftX", "LS X-axis" },
	{ "XboxTypeS_RightY", "RS Y-axis" },
	{ "XboxTypeS_RightX", "RS X-axis" },
	{ "XboxTypeS_LeftThumbStick", "L3" },
	{ "XboxTypeS_RightThumbStick", "R3" },
	});

unordered_map<string, string> xboxRenames({
	{ "XboxTypeS_A", "A" },
	{ "XboxTypeS_B", "B" },
	{ "XboxTypeS_X", "X" },
	{ "XboxTypeS_Y", "Y" },
	{ "XboxTypeS_LeftShoulder", "LB" },
	{ "XboxTypeS_LeftTriggerAxis", "LT" },
	{ "XboxTypeS_RightShoulder", "RB" },
	{ "XboxTypeS_RightTriggerAxis", "RT" },
	{ "XboxTypeS_DPad_Up", "D-Up" },
	{ "XboxTypeS_DPad_Down", "D-Down" },
	{ "XboxTypeS_DPad_Left", "D-Left" },
	{ "XboxTypeS_DPad_Right", "D-Right" },
	{ "XboxTypeS_Start", "Start" },
	{ "XboxTypeS_Back", "Back" },
	{ "XboxTypeS_LeftY", "Left Stick Y-axis" },
	{ "XboxTypeS_LeftX", "Left Stick X-axis" },
	{ "XboxTypeS_RightY", "Right Stick Y-axis" },
	{ "XboxTypeS_RightX", "Right Stick X-axis" },
	{ "XboxTypeS_LeftThumbStick", "L3" },
	{ "XboxTypeS_RightThumbStick", "R3" },
	});

unordered_map<string, string> actionRenames({
	{ "ThrottleForward", "Forward" },
	{ "ThrottleReverse", "Backwards" },
	{ "SteerRight", "Steer Right" },
	{ "SteerLeft", "Steer Left" },
	{ "LookUp", "Look Up" },
	{ "LookDown", "Look Down" },
	{ "LookRight", "Look Right" },
	{ "LookLeft", "Look Left" },
	{ "SwivelUp", "Swivel Up" },
	{ "SwivelDown", "Swivel Down" },
	{ "SwivelRight", "Swivel Right" },
	{ "SwivelLeft", "Swivel Left" },
	{ "YawRight", "Yaw Right" },
	{ "YawLeft", "Yaw Left" },
	{ "PitchUp", "Pitch Up" },
	{ "PitchDown", "Pitch Down" },
	{ "RollRight", "Air Roll Right" },
	{ "RollLeft", "Air Roll Left" },
	{ "Jump", "Jump" },
	{ "Boost", "Boost" },
	{ "Handbrake", "Powerslide" },
	{ "SecondaryCamera", "Focus On Ball" },
	{ "ToggleRoll", "Air Roll" },
	{ "RearCamera", "Rear Cam" },
	{ "ToggleScoreboard", "Scoreboard" },
	{ "PushToTalk", "Push To Talk" },
	{ "ChatPreset1", "QC/Info" },
	{ "ChatPreset2", "QC/Compliments" },
	{ "ChatPreset3", "QC/Reactions" },
	{ "ChatPreset4", "QC/Apologies" },
	{ "ResetTraining", "Reset Shot" },
	{ "AutoSaveReplay", "Save Replay" },
	{ "UsePickup", "Use Item" },
	{ "NextPickup", "Next Item" },
	});

const unordered_map<string, string> defaultGamepadBindings({
	{ "ThrottleForward", "XboxTypeS_RightTriggerAxis" },
	{ "ThrottleReverse", "XboxTypeS_LeftTriggerAxis" },
	{ "SteerRight", "XboxTypeS_LeftX" },
	{ "SteerLeft", "XboxTypeS_LeftX" },
	{ "LookUp", "XboxTypeS_RightY" },
	{ "LookDown", "XboxTypeS_RightY" },
	{ "LookRight", "XboxTypeS_RightX" },
	{ "LookLeft", "XboxTypeS_RightX" },
	{ "SwivelUp", "XboxTypeS_RightY" },
	{ "SwivelDown", "XboxTypeS_RightY" },
	{ "SwivelRight", "XboxTypeS_RightX" },
	{ "SwivelLeft", "XboxTypeS_RightX" },
	{ "YawRight", "XboxTypeS_LeftX" },
	{ "YawLeft", "XboxTypeS_LeftX" },
	{ "PitchUp", "XboxTypeS_LeftY" },
	{ "PitchDown", "XboxTypeS_LeftY" },
	{ "RollRight", "None" },
	{ "RollLeft", "None" },
	{ "Jump", "XboxTypeS_A" },
	{ "Boost", "XboxTypeS_B" },
	{ "Handbrake", "XboxTypeS_X" },
	{ "SecondaryCamera", "XboxTypeS_Y" },
	{ "ToggleRoll", "XboxTypeS_LeftTrigger" },
	{ "RearCamera", "XboxTypeS_RightThumbStick" },
	{ "ToggleScoreboard", "XboxTypeS_LeftShoulder" },
	{ "PushToTalk", "None" },
	{ "ChatPreset1", "XboxTypeS_DPad_Up" },
	{ "ChatPreset2", "XboxTypeS_DPad_Left" },
	{ "ChatPreset3", "XboxTypeS_DPad_Right" },
	{ "ChatPreset4", "XboxTypeS_DPad_Down" },
	{ "ResetTraining", "XboxTypeS_RightShoulder" },
	{ "AutoSaveReplay", "XboxTypeS_Back" },
	{ "UsePickup", "XboxTypeS_LeftThumbStick" },
	{ "NextPickup", "XboxTypeS_RightShoulder" },
	{ "ResetMouseCenter", "None" },
	{ "MouseSteerLeft", "None" },
	{ "MouseSteerRight", "None" },
	{ "MouseThrottleReverse", "None" },
	{ "MouseThrottleForward", "None" },
	});

const unordered_map<string, string> defaultPCBindings({
	{ "ThrottleForward", "W" },
	{ "ThrottleReverse", "S" },
	{ "SteerRight", "D" },
	{ "SteerLeft", "A" },
	{ "LookUp", "MouseY" },
	{ "LookDown", "MouseY" },
	{ "LookRight", "MouseX" },
	{ "LookLeft", "MouseX" },
	{ "YawRight", "D" },
	{ "YawLeft", "A" },
	{ "PitchUp", "S" },
	{ "PitchDown", "W" },
	{ "RollRight", "E" },
	{ "RollLeft", "Q" },
	{ "Boost", "LeftMouseButton" },
	{ "Jump", "RightMouseButton" },
	{ "Handbrake", "LeftShift" },
	{ "SecondaryCamera", "SpaceBar" },
	{ "ToggleRoll", "LeftShift" },
	{ "RearCamera", "MiddleMouseButton" },
	{ "UsePickup", "R" },
	{ "NextPickup", "C" },
	{ "ToggleScoreboard", "Tab" },
	{ "ChatPreset1", "One" },
	{ "ChatPreset2", "Two" },
	{ "ChatPreset3", "Three" },
	{ "ChatPreset4", "Four" },
	{ "ResetTraining", "BackSpace" },
	{ "AutoSaveReplay", "None" },
	{ "MouseSteerLeft", "None" },
	{ "MouseSteerRight", "None" },
	{ "MouseThrottleReverse", "None" },
	{ "MouseThrottleForward", "None" },
	{ "PushToTalk", "None" },
	{ "SwivelLeft", "None" },
	{ "SwivelRight", "None" },
	{ "SwivelDown", "None" },
	{ "SwivelUp", "None" },
	{ "ResetMouseCenter", "None" },
	});

enum INPUT_TYPE {
	DS4 = 0,
	XBOX = 1,
	KBM = 2,
};

std::string generateBindingsStr(const vector<pair<string, string>>& bindings, unordered_map<string, string>* inputRenamer,
	const unordered_map<string, string>& defaultBindings, string bindingSeparator, string actionSeparator)
{
	stringstream oss;
	bool isFirst = true;

	oss << "Default bindings except: ";

	//stringstream ods;

	int changes = 0;
	for (auto binding : bindings) {
		//ods << "\n{ \"" << binding.second << "\", \"" << binding.first << "\" },";

		// Only get actions within actionRenames
		auto actionIt = actionRenames.find(binding.second);
		if (actionIt == actionRenames.end()) continue;
		auto action = actionIt->second;

		// Ignore default bindings
		auto def = defaultBindings.find(binding.second);
		if (def == defaultBindings.end() || def->second.compare(binding.first) == 0) continue;

		// Rename input if there's an input renamer and it's in the map
		auto inputName = binding.first;
		if (inputRenamer != nullptr) {
			auto renameIt = inputRenamer->find(inputName);
			if (renameIt != inputRenamer->end()) {
				inputName = renameIt->second;
			}
		}

		++changes;
		if (!isFirst) oss << bindingSeparator;
		oss << action << actionSeparator << inputName;
		isFirst = false;
	}

	//_globalCvarManager->log("1" + ods.str());

	if (changes == 0) {
		return "Default bindings";
	}

	return oss.str();
}

void StreamAPIPlugin::getBindings()
{
	cvarManager->log("Updating bindings");

	string oldGamepadStr = ds4BindingsStr;
	string oldKbmStr = kbmBindingsStr;

	if (gameWrapper->GetSettings().GetAllGamepadBindings().size() == 0) {
		ds4BindingsStr = generateBindingsStr(gameWrapper->GetSettings().GetAllPCBindings(), &ds4Renames, defaultGamepadBindings, outputSeparator, ": ");
		xboxBindingsStr = generateBindingsStr(gameWrapper->GetSettings().GetAllPCBindings(), &xboxRenames, defaultGamepadBindings, outputSeparator, ": ");
		kbmBindingsStr = "KBM bindings not supported yet (pending BakkesMod update)";
	}
	else {
		ds4BindingsStr = generateBindingsStr(gameWrapper->GetSettings().GetAllGamepadBindings(), &ds4Renames, defaultGamepadBindings, outputSeparator, ": ");
		xboxBindingsStr = generateBindingsStr(gameWrapper->GetSettings().GetAllGamepadBindings(), &xboxRenames, defaultGamepadBindings, outputSeparator, ": ");
		kbmBindingsStr = generateBindingsStr(gameWrapper->GetSettings().GetAllPCBindings(), nullptr, defaultPCBindings, outputSeparator, ": ");
	}

	if (ds4BindingsStr.compare(oldGamepadStr) != 0 || kbmBindingsStr.compare(oldKbmStr) != 0) {
		webSocket.setData("bindings",
			"{\"ds4\":\"" + ds4BindingsStr + "\",\"xbox\":\"" + xboxBindingsStr + "\",\"kbm\":\"" + kbmBindingsStr + "\"}");
	}
}

void StreamAPIPlugin::getTrainingPack()
{
	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		// It seems this data is more reliably available if we wait a second
		if (!gw->IsInCustomTraining()) {
			return;
		}

		auto te = TrainingEditorWrapper(gw->GetGameEventAsServer().memory_address);
		if (te.IsNull()) {
			return;
		}

		auto td = te.GetTrainingData();
		auto tdd = td.GetTrainingData();
		auto code = tdd.GetCode();
		if (code.IsNull()) {
			cvarManager->log("code is null");
			return;
		}

		auto description = tdd.GetTM_Name();
		if (description.IsNull()) {
			cvarManager->log("description is null");
			return;
		}

		auto creator = tdd.GetCreatorName();
		if (creator.IsNull()) {
			cvarManager->log("creator is null");
			return;
		}

		cvarManager->log("Updating training pack");
		stringstream oss;
		oss << description.ToString() << " by "
			<< creator.ToString() << " ("
			<< to_string(te.GetTotalRounds())
			<< " drills): " << code.ToString();
		trainingPackStr = oss.str();

		webSocket.setData("training", trainingPackStr);

		}, 1.0f);
}

void StreamAPIPlugin::onDump(vector<string> params)
{
	if (params.size() > 1 && params[1].compare("stale") == 0) {
		cvarManager->log("Dumping stale data");
	}
	else {
		cvarManager->log("Refreshing data before dumping");
		getLoadout();
		getSens();
		getCamera();
		getBindings();
		getVideo();
		ranks.getRanks(gameWrapper);
		webSocket.setData("rank", ranks.toString("json", cvarManager));
	}

	cvarManager->log(this->loadout.toString());
	cvarManager->log("Sens: \n\t" + sensStr);
	cvarManager->log("Camera: \n\t" + cameraStr);
	cvarManager->log("KBM bindings: \n\t" + kbmBindingsStr);
	cvarManager->log("Xbox bindings: \n\t" + xboxBindingsStr);
	cvarManager->log("DS4 bindings: \n\t" + ds4BindingsStr);
	cvarManager->log("Video: \n\t" + videoStr);
	cvarManager->log("Ranks: \n\t" + ranks.toString("", cvarManager));
	cvarManager->log("CustomMap: " + customMapSupport.getCustomMap());
	cvarManager->log("Map: " + customMapSupport.getMap());
}

void StreamAPIPlugin::startHttpServer(int port)
{
	stopHttpServer();
	if (!serverIsRunning) {
		serverPort = port;
		strncpy(guiServerPort, to_string(serverPort).c_str(), sizeof(guiServerPort));
		serverIsRunning = true;
		abortHttpStart = false;
		cvarManager->log("Creating httpThread");
		httpThread = std::thread(&StreamAPIPlugin::runHttpServer, this, serverPort);
		cvarManager->log("httpThread created");
	}
}

void StreamAPIPlugin::stopHttpServer()
{
	if (serverIsRunning) {
		serverIsRunning = false;
		cvarManager->log("Calling httpServer.stop()");
		abortHttpStart = true;
		httpServer.stop();
		cvarManager->log("Calling httpThread.join()");
		httpThread.join();
		cvarManager->log("httpThread has been joined");
	}
}

void StreamAPIPlugin::runHttpServer(int port)
{
	cvarManager->log("Starting HTTP API server on port " + to_string(port));
	serverStatus = "Running";
	if (!abortHttpStart) httpServer.listen("0.0.0.0", port);
	cvarManager->log("HTTP API server stopped");
	serverStatus = "Stopped";
}

void StreamAPIPlugin::configureHttpServer()
{
	httpServer.Get("/cmd", [this](const httplib::Request& req, httplib::Response& res) {
		string cmd;
		string args;
		if (req.has_param("cmd")) {
			cmd = req.get_param_value("cmd");
			if (req.has_param("args")) {
				args = req.get_param_value("args");
			}
			cvarManager->log("Cmd: " + cmd + ", args: " + args);

			auto it = commandNameToCommand.find(cmd);
			if (it != commandNameToCommand.end()) {
				res.set_content(it->second(args), "text/plain");
			}
			else {
				res.set_content("bad command", "text/plain");
			}
		}
		else {
			res.set_content("no command given", "text/plain");
		}

		});
}

/* Commands */
std::string StreamAPIPlugin::loadoutCommand(std::string args)
{
	return this->loadout.getItemString(args, outputSeparator, showSlotName, showBMCode);
}

std::string StreamAPIPlugin::sensCommand(std::string args)
{
	return sensStr;
}

std::string StreamAPIPlugin::cameraCommand(std::string args)
{
	return cameraStr;
}

std::string StreamAPIPlugin::bindingsCommand(std::string args)
{
	if (args.compare("kbm") == 0) {
		return kbmBindingsStr;
	}
	else if (args.compare("xbox")) {
		return xboxBindingsStr;
	}
	return ds4BindingsStr;
}

std::string StreamAPIPlugin::videoCommand(std::string args)
{
	return videoStr;
}

std::string StreamAPIPlugin::trainingCommand(std::string args)
{
	return trainingPackStr;
}

std::string StreamAPIPlugin::rankCommand(std::string args)
{
	return ranks.toString(args, cvarManager);
}

std::string StreamAPIPlugin::workshopCommand(std::string args)
{
	return customMapSupport.getCustomMap();
}

std::string StreamAPIPlugin::mapCommand(std::string args)
{
	return customMapSupport.getMap();
}

void StreamAPIPlugin::runPushCommand(std::vector<std::string> params)
{
	if (params.size() == 1) {
		cvarManager->log("No command given");
		return;
	}

	if (params.size() > 2) {
		if (!checkPushConfig(params[2])) {
			return;
		}
	}

	cvarManager->executeCommand("sleep 0; " + params[1]);
}

bool StreamAPIPlugin::checkPushConfig(std::string config)
{
	// Settings allowed: any,other,casual,ranked,extramodes,lan,private
	unordered_set<string> settings;
	boost::split(settings, config, boost::is_any_of(","));

	if (settings.find("any") != settings.end())
		return true;
	
	if (gameWrapper->IsInGame()) {
		auto server = gameWrapper->GetCurrentGameState();
		if (server.IsNull()) {
			cvarManager->log("Server is null. Cannot check config for run_cmd. Aborting.");
			return false;
		}
		GameSettingPlaylistWrapper playlistWrapper = server.GetPlaylist();
		if (playlistWrapper.IsNull()) {
			cvarManager->log("PlaylistWrapper is null. Cannot check config for run_cmd. Aborting.");
			return false;
		}

		if (playlistWrapper.IsPrivateMatch())
			return settings.find("private") != settings.end();
		if (playlistWrapper.IsLanMatch())
			return settings.find("lan") != settings.end();

		int id = playlistWrapper.GetPlaylistId();
		switch (id) {
		case (int)StreamAPIPlaylist::RANKEDDUEL:
		case (int)StreamAPIPlaylist::RANKEDDOUBLES:
		case (int)StreamAPIPlaylist::RANKEDSTANDARD:
			return settings.find("ranked") != settings.end();
		case (int)StreamAPIPlaylist::RANKEDHOOPS:
		case (int)StreamAPIPlaylist::RANKEDSNOWDAY:
		case (int)StreamAPIPlaylist::RANKEDRUMBLE:
		case (int)StreamAPIPlaylist::RANKEDDROPSHOT:
			return settings.find("extramodes") != settings.end();
		case (int)StreamAPIPlaylist::DUEL:
		case (int)StreamAPIPlaylist::DOUBLES:
		case (int)StreamAPIPlaylist::STANDARD:
		case (int)StreamAPIPlaylist::CHAOS:
			return settings.find("casual") != settings.end();
		default:
			cvarManager->log("Got unexpected playlist ID: " + to_string(id) + ". Assuming it's a casual playlist.");
			return settings.find("casual") != settings.end();
		}		
	}
	else {
		return settings.find("other") != settings.end();
	}
}

void StreamAPIPlugin::SubmitReport(std::string reportDetails, bool submittedFromUI)
{
	gameWrapper->Execute(
		[this, reportDetails, submittedFromUI](GameWrapper* gw) {
			// Dump data to log before sending log (might not be included in report if exceeds MAX_REPORT_SIZE)
			cvarManager->log("Dumping data to log before submitting report");
			onDump(vector<string>{"streamapi_dump", "stale"});
			onDump(vector<string>{"streamapi_dump"});
			boost::thread t{ &StreamAPIPlugin::SubmitReportThread, this, reportDetails, submittedFromUI }; // TODO: join this thread
		});
}

void StreamAPIPlugin::SubmitReportThread(std::string reportDetails, bool submittedFromUI) {
	if (submittedFromUI) guiReportStatus = "";

	cvarManager->log("Reading bakkesmod.log");
	char* buf = (char*)malloc(MAX_REPORT_SIZE);
	if (!buf) {
		if (submittedFromUI) guiReportStatus = "Failed to allocate buffer for report. Aborting report submission.";
		cvarManager->log("Failed to allocate buffer for report. Aborting report submission.");
		return;
	}

	size_t bufWritten = 0;
	size_t written;
	int errno;

	// Write token to buf
	written = snprintf(&buf[bufWritten], MAX_REPORT_SIZE - bufWritten, "Token: %s\n\n", webSocket.getToken().c_str()); // TODO: Do we need to worry about encoding?
	bufWritten += min(written, MAX_REPORT_SIZE - 1 - bufWritten);

	// Write report to buf
	if (!reportDetails.empty()) {
		written = snprintf(&buf[bufWritten], MAX_REPORT_SIZE - bufWritten, "Report:\n%s\n\n", reportDetails.c_str());
		bufWritten += min(written, MAX_REPORT_SIZE - 1 - bufWritten);
	}

	// Read bakkesmod.log into buf
	ifstream fin(gameWrapper->GetBakkesModPath() / "bakkesmod.log");
	if (fin.fail()) {
		if (!reportDetails.empty()) {
			if (submittedFromUI) guiReportStatus = "Failed to read bakkesmod.log for submission. Submitting report details without bakkesmod.log data.";
			cvarManager->log("Failed to read bakkesmod.log for submission. Submitting report details without bakkesmod.log data.");
		}
		else {
			if (submittedFromUI) guiReportStatus = "Failed to read bakkesmod.log for submission.";
			cvarManager->log("Failed to read bakkesmod.log for submission.");
		}
	}
	else {
		fin.read(&buf[bufWritten], MAX_REPORT_SIZE - 1 - bufWritten);
		size_t n = fin.gcount();
		bufWritten += n;
		cvarManager->log("Read " + to_string(n) + " bytes from bakkesmod.log into buffer of size " + to_string(MAX_REPORT_SIZE));
		buf[bufWritten] = 0;
	}

	cvarManager->log("Submitting report");
	httplib::MultipartFormDataItems items = {
		{ "report", buf, "report.txt", "text/plain" }
	};

	httplib::Client cli(REPORT_SERVER_URL);
	try {
		auto res = cli.Post("/api/rocket-league/stream-api/submit-report", items);
		if (res) {
			if (res->status == 200) {
				if (submittedFromUI) guiReportStatus = "Report submitted successfully";
				cvarManager->log("Report submission returned status: " + to_string(res->status) + ". " + "Report submitted successfully");
			}
			else {
				if (submittedFromUI) guiReportStatus = "Failed to submit report to server: " + res->body;
				cvarManager->log("Report submission returned status: " + to_string(res->status) + ". " + "Failed to submit report to server: " + res->body);
			}
		}
		else {
			if (submittedFromUI) guiReportStatus = "Could not reach report server";
			cvarManager->log("Could not reach report server");
		}
	}
	catch (exception e) {
		cvarManager->log("Failed to connect to server: " + string(e.what()));
	}

	free(buf);
}

void StreamAPIPlugin::render(CanvasWrapper canvas) {
	// Render any images that are shown
	Vector2 centerPos = gameWrapper->GetScreenSize() / 2;
	for (auto it = images.begin(); it != images.end(); ++it) {
		auto img = it->second.image.get();
		if (it->second.show && img->IsLoadedForCanvas()) {
			Vector2 position = it->second.position;
			Vector2 offset = it->second.offset;
			if (position.X < 0) position.X = centerPos.X - it->second.scale * (img->GetSize().X / 2);
			if (position.Y < 0) position.Y = centerPos.Y - it->second.scale * (img->GetSize().Y / 2);
			canvas.SetPosition(position + offset);
			canvas.DrawTexture(img, it->second.scale);
		}
	}
}