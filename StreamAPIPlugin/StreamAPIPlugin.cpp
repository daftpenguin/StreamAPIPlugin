#include "StreamAPIPlugin.h"

#include <fstream>

using namespace std;

#define DEFAULT_SERVER_PORT "10111"
#define DEFAULT_BUFLEN 512

#define APPEND_STRING_WITH_ITEM(oss, separator, showSlotName, slotName, itemString) { \
	oss << separator; \
	if (showSlotName) oss << slotName << ": "; \
	oss << itemString; \
}

BAKKESMOD_PLUGIN(StreamAPIPlugin, "Stream API Plugin", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
int _globalBMVersion = 0;

// TODO: external chat bot support

void StreamAPIPlugin::onLoad()
{
	_globalCvarManager = cvarManager;
	
	cvarManager->registerNotifier("streamapi_dump", bind(&StreamAPIPlugin::onDump, this, std::placeholders::_1), "dumps loadout data to console", PERMISSION_ALL);

	ifstream fin(gameWrapper->GetBakkesModPath() / "version.txt", ios::in);
	if (fin.is_open()) {
		string line;
		getline(fin, line);
		try {
			_globalBMVersion = stoi(line);
		}
		catch (int e) {
			_globalBMVersion = 0;
			cvarManager->log("Failed to read BakkesMod version from version.txt. Plugin will not work.");
		}
	}
	
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

	cvarManager->registerCvar("streamapi_gray_port", "false", "", false, true, 0, true, 1, false);

	auto portVar = cvarManager->registerCvar("streamapi_port", DEFAULT_SERVER_PORT, "Port for HTTP API to listen for requests", true, true, 1024, true, 49151, true);
	serverPort = portVar.getIntValue();
	httpThread = std::thread(&StreamAPIPlugin::runHttpServer, this, serverPort);

	portVar.addOnValueChanged([this](std::string, CVarWrapper cvar) {
		cvarManager->log("Restarting HTTP API server on new port");
		serverPort = cvar.getIntValue();
		if (serverPort < 1024 || serverPort > 49151) {
			serverPort = runningServerPort;
			return;
		}

		httpServer.stop();
		httpThread.join();

		httpThread = std::thread(&StreamAPIPlugin::runHttpServer, this, serverPort);
	});

	getLoadout();
	getSens();
	getCamera();
	getBindings();
	getVideo();
	getTrainingPack();
	ranks.getRanks(gameWrapper);
	gameWrapper->GetMMRWrapper().RegisterMMRNotifier([this](UniqueIDWrapper id) { if (id == gameWrapper->GetUniqueID()) ranks.updateRank(gameWrapper); });
	gameWrapper->SetTimeout([this](GameWrapper* gw) { // Doesn't appear to update when ranks are initially retrieved
		ranks.getRanks(gameWrapper);
		}, 10.0f);

	gameWrapper->HookEventPost(LOADOUT_CHANGED_EVENT, [this](string eventName) { getLoadout(); });
	gameWrapper->HookEventPost(SENS_CHANGED_EVENT, [this](string eventName) { getSens(); });
	gameWrapper->HookEventPost(CAMERA_CHANGED_EVENT, [this](string eventName) { getCamera(); });
	gameWrapper->HookEventPost(BINDINGS_CHANGED_EVENT, [this](string eventName) { getBindings(); });
	for (auto evt : VIDEO_CHANGED_EVENTS) {
		gameWrapper->HookEventPost(evt, [this](string eventName) { cvarManager->log(eventName); getVideo(); });
	}
	gameWrapper->HookEventPost(CUSTOM_TRAINING_LOADED_EVENT, [this](string eventName) { getTrainingPack();  });

	commandNameToCommand["loadout"] = std::bind(&StreamAPIPlugin::loadoutCommand, this, std::placeholders::_1);
	commandNameToCommand["sens"] = std::bind(&StreamAPIPlugin::sensCommand, this, std::placeholders::_1);
	commandNameToCommand["camera"] = std::bind(&StreamAPIPlugin::cameraCommand, this, std::placeholders::_1);
	commandNameToCommand["bindings"] = std::bind(&StreamAPIPlugin::bindingsCommand, this, std::placeholders::_1);
	commandNameToCommand["video"] = std::bind(&StreamAPIPlugin::videoCommand, this, std::placeholders::_1);
	commandNameToCommand["training"] = std::bind(&StreamAPIPlugin::trainingCommand, this, std::placeholders::_1);
	commandNameToCommand["rank"] = std::bind(&StreamAPIPlugin::rankCommand, this, std::placeholders::_1);
}

void StreamAPIPlugin::onUnload()
{
	cvarManager->log("Stopping HTTP API server");
	httpServer.stop();
	httpThread.join();
}

void StreamAPIPlugin::getLoadout()
{
	if (_globalBMVersion == 0)
		return;

	cvarManager->log("Updating loadout");

	bool teamNum = previewTeamNum;

	if (gameWrapper->IsInGame()) {
		auto car = gameWrapper->GetLocalCar();
		if (!car.IsNull()) {
			teamNum = car.GetTeamNum2();
		}
	}

	this->loadout.clear();
	this->loadout.load(teamNum, cvarManager, gameWrapper);
}

void StreamAPIPlugin::getSens()
{
	cvarManager->log("Updating sens");

	stringstream oss;
	oss.precision(2);

	gameWrapper->GetSettings();
	auto gp = gameWrapper->GetSettings().GetGamepadSettings();
	oss << "Steering: " << gp.SteeringSensitivity
		<< outputSeparator << "Aerial: " << gp.AirControlSensitivity
		<< outputSeparator << "Deadzone: " << gp.ControllerDeadzone
		<< outputSeparator << "Dodge Deadzone: " << gp.DodgeInputThreshold;
	sensStr = oss.str();
}

void StreamAPIPlugin::getCamera()
{
	cvarManager->log("Updating camera");

	stringstream oss;
	oss.precision(2);
	auto camera = gameWrapper->GetSettings().GetCameraSettings();
	oss << "FOV: " << (int) camera.FOV
		<< outputSeparator << "Distance: " << (int) camera.Distance
		<< outputSeparator << "Height: " << (int) camera.Height
		<< outputSeparator << "Angle: " << (int) camera.Pitch
		<< outputSeparator << "Stiffness: " << camera.Stiffness
		<< outputSeparator << "Swivel: " << camera.SwivelSpeed
		<< outputSeparator << "Transition: " << camera.TransitionSpeed;
	cameraStr = oss.str();
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
	{ "RanderDetail", "Render Detail" }
	});

void StreamAPIPlugin::getVideo()
{
	cvarManager->log("Updating video settings");

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
}

void StreamAPIPlugin::getBindings()
{
	cvarManager->log("Updating bindings");

	stringstream oss;

	map<string, string> bindings = gameWrapper->GetSettings().GetPCBindings();
	bool isFirst = true;
	for (auto it = bindings.begin(); it != bindings.end(); it++) {
		if (!isFirst) oss << outputSeparator;
		oss << it->first << ": " << it->second;
		isFirst = false;
	}
	kbmBindingsStr = oss.str();
	
	oss.str("");
	bindings = gameWrapper->GetSettings().GetGamepadBindings();
	isFirst = true;
	for (auto it = bindings.begin(); it != bindings.end(); it++) {
		if (!isFirst) oss << outputSeparator;
		oss << it->first << ": " << it->second;
		isFirst = false;
	}
	controllerBindingsStr = oss.str();
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
		/*if (td.GetbUnowned() != 1) {
			return;
		}*/

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

		}, 1.0f);
}

void StreamAPIPlugin::onDump(vector<string> params)
{
	getLoadout();
	getSens();
	getCamera();
	getBindings();
	getVideo();
	ranks.getRanks(gameWrapper);

	cvarManager->log(this->loadout.toString());
	cvarManager->log("Sens: \n\t" + sensStr);
	cvarManager->log("Camera: \n\t" + cameraStr);
	cvarManager->log("Controller bindings: \n\t" + controllerBindingsStr);
	cvarManager->log("KBM bindings: \n\t" + kbmBindingsStr);
	cvarManager->log("Video: \n\t" + videoStr);
	cvarManager->log("Ranks: \n\t" + ranks.toString("", cvarManager));
}

void StreamAPIPlugin::runHttpServer(int port)
{
	cvarManager->log("Starting HTTP API server on port " + to_string(port));
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

	httpServer.listen("0.0.0.0", port);
	cvarManager->log("HTTP API server stopped");
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
	return "Not implemented yet";
	return (args.compare("kbm") == 0 ? kbmBindingsStr : controllerBindingsStr);
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