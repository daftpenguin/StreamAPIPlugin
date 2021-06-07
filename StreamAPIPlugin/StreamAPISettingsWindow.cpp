#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "pluginsdk.lib")

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include "cpp-httplib/httplib.h"
#include "Util.h"

#include "StreamAPIPlugin.h"

#include <windows.h>
#include <shellapi.h>

using namespace std;

const wchar_t* setupUrlW = L"https://www.daftpenguin.com/rocket-league/stream-api/setup";
const char* setupUrl = "https://www.daftpenguin.com/rocket-league/stream-api/setup";
const size_t setupUrlLen = strlen(setupUrl);

std::vector<std::string> announcement = {
	{"NEW FEATURE! Allow your viewers to activate commands that perform actions in your game. See the \"Action Commands\" tab below."},
};

struct InternalConsoleCommand {
	std::string name;
	std::string params;
	std::string description;
};

std::vector<InternalConsoleCommand> internalConsoleCommands({
	{ "streamapi_show_image", "<img_path> <settings>", "This shows an image (PNG ONLY) from the given path. Calling this with no params will show dickbut.png. The path can either be absolute or relative to the data/streamapi folder inside the bakkesmod folder. The settings param is a comma separate list of setting=value that can be used to control the display of the image. Settings are: id, scale, x, y, x_offset, y_offset. With no x or y given, the image is displayed in the centered on that axis, and offsetted if offsets are given. If no id given, the path is used as the id. The id can then be used to hide the image, or change the settings of an image that's already being displayed." },
	{ "streamapi_hide_image", "<img_id>", "Hides image previously shown by streamapi_hide_image, using the img_id (path if no img_id set in settings). Hides all images if no img_id is given." },
	});

struct ExternalConsoleCommand {
	int pluginId;
	std::string pluginName;
	bool verified;
	std::string author;
	std::string description;
	std::wstring url = L"";

	std::wstring getURL() {
		if (url.empty()) {
			url = L"https://bakkesplugins.com/plugins/view/" + to_wstring(pluginId);
		}
		return url;
	}
};

std::vector<ExternalConsoleCommand> externalConsoleCommands({
	ExternalConsoleCommand{ -1, "", true, "", "cl_online_status_<detailed/override>: overrides your online status as shown in your friends' friends list." },
	ExternalConsoleCommand{ -1, "", true, "", "cl_itemmod_<enabled/code>: use \"cl_itemmod_enabled 1; cl_itemmod_code ${code};\" to allow viewers to equip items with item mod codes by exposing the code parameter through your bot command. I have plans to release a webpage soon that allows viewers to create designs in browser and generate the item mod code." },
	ExternalConsoleCommand{ 202, "BakkesMod Loadout Randomizer", false, "DaftPenguin", "randomize_bm_loadout: randomizes your loadout from ALL items that can be equipped through BakkesMod's item mod." },
	ExternalConsoleCommand{ 233, "Attachment Rescaler", true, "ItsBranK", "rescaler_enabled (0 or 1), rescaler_topper_scale (1-50), rescaler_antenna_scale (1-50): rescales toppers and antennas of ALL players on the field, similar to April Fools' Day 2018." },
	ExternalConsoleCommand{ 103, "Rainbow Car", true, "CinderBlock (now maintained by BM team)", "RainbowCar_<Primary/Secondary>_<Enable/Offset/Saturation/Reverse/Saturation/Speed/Value>: many cvars to control the settings that cycles the colors through the rainbow for both primary and secondary colors." },
	ExternalConsoleCommand{ 92, "Party time", true, "CinderBlock (now maintained by BM team)", "PartyTime_<Enable_Cars/Enable_Field/Enable_LocalCarOnly/Speed>: rainbow field colors but only works in offline modes, LAN matches, and private matches with unlimited time." },
	ExternalConsoleCommand{ 34, "First Person", true, "CinderBlock (now maintained by BM team)", "FirstPerson<Enable/Disable> (command), FirstPerson_UseCustomBallCam (cvar): only works in offline modes, LAN matches, and private matches with unlimited time. Changes player's camera position and rotation to align to the hood of the car." },
	ExternalConsoleCommand{ 150, "Color Changer", true, "CinderBlock (now maintained by BM team)", "Colors_<far too many to list> (cvars). Changes the colors of the field and cars in every game, like club vs club games. Can be used in ranked and casual, but colors cannot be changed mid-match. Perhaps these commands could be sync'd to your RGB lighting?" },

	});

// TODO: Don't generate all the ShellExecute URLs every frame...

void StreamAPIPlugin::RenderSettings()
{
	if (announcement.size() > 0) {
		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		for (auto& line : announcement) {
			// Indent doesn't work well here, as I think there's already an indent being used in BM's settings window
			ImGui::Dummy(ImVec2(10.0f, 0.0f));
			ImGui::SameLine();
			ImGui::TextWrapped(line.c_str());
		}
		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		ImGui::Separator();
	}
	
	ImGui::Dummy(ImVec2(0.0f, 10.0f));

	if (ImGui::BeginTabBar("TabBar", ImGuiTabBarFlags_None)) {
		if (ImGui::BeginTabItem("API Settings")) {
			RenderGeneralSettings();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Action Commands (beta)")) {
			RenderPushCommands();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Console Commands")) {
			RenderConsoleCommands();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Report Bug")) {
			RenderReportSubmission();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::Separator();
	ImGui::TextWrapped("Stream API Plugin created by DaftPenguin");
}

void StreamAPIPlugin::RenderGeneralSettings()
{
	ImGui::TextWrapped("Click enable button to toggle between internal and external bot support.");
	ImGui::TextWrapped("TO USE EXTERNAL BOT SUPPORT, YOU MUST LOG INTO MY WEBSITE AND GENERATE A TOKEN. CLICK THE BUTTON TO OPEN THE SETUP PAGE IN YOUR BROWSER.");

	string commandsUrl = "https://www.daftpenguin.com/rocket-league/stream-api/commands" + string((useWebSocket ? "" : "/local"));
	if (ImGui::Button("Open Command Setup Page (in browser)")) {
		ShellExecute(0, 0, stringToWstring(commandsUrl).c_str(), 0, 0, SW_SHOW);
	}
	ImGui::TextWrapped("If button doesn't work:");
	ImGui::SameLine();
	ImGui::InputText("##CommandsURL", (char*)commandsUrl.c_str(), commandsUrl.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);

	ImGui::Separator();

	/* Local bot section */

	ImGui::TextWrapped("Local Bot Settings");
	if (useWebSocket) {
		if (ImGui::Button("Enable##LocalBot")) {
			cvarManager->executeCommand("streamapi_use_external_bot 0");
		}
	}
	else {
		ImGui::InputText("HTTP API Server Port", guiServerPort, sizeof(guiServerPort), ImGuiInputTextFlags_CharsDecimal);
		ImGui::SameLine();
		if (ImGui::Button("Save")) {
			cvarManager->executeCommand("streamapi_port " + string(guiServerPort));
		}
		ImGui::TextWrapped("Status: %s", serverStatus);

		string internalUrl = "http://127.0.0.1:" + to_string(serverPort) + "/cmd?cmd=loadout";
		if (ImGui::Button("Open Local API (in browser)")) {
			ShellExecute(0, 0, stringToWstring(internalUrl).c_str(), 0, 0, SW_SHOW);
		}
		ImGui::TextWrapped("If button doesn't work:");
		ImGui::SameLine();
		ImGui::InputText("##InternalAPIURL", (char*)internalUrl.c_str(), internalUrl.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
	}

	ImGui::Separator();

	/* External bot section */

	ImGui::TextWrapped("External Bot Settings");

	if (!useWebSocket) {
		if (ImGui::Button("Enable##ExternalBot")) {
			cvarManager->executeCommand("streamapi_use_external_bot 1");
		}
	}
	else {
		if (ImGui::Button("Open Setup Website (in browser)")) {
			ShellExecute(0, 0, setupUrlW, 0, 0, SW_SHOW);
		}
		ImGui::TextWrapped("If button doesn't work:");
		ImGui::SameLine();
		ImGui::InputText("##ExternalSetupURL", (char*)setupUrl, setupUrlLen, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);

		if (ImGui::Button("Paste Token")) {
			auto guiWebSocketToken = ImGui::GetClipboardText();
			if (webSocket.verifyToken(guiWebSocketToken)) {
				if (webSocket.setToken(guiWebSocketToken)) {
					webSocket.start();
					guiShowBadToken = false;
				}
				else {
					guiShowBadToken = true;
				}
			}
			else {
				guiShowBadToken = true;
			}
		}
		if (guiShowBadToken) {
			ImGui::SameLine();
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered), "Last pasted token failed verification and was not set. Please try again.");
		}

		auto now = std::chrono::system_clock::now();
		if (now - guiWebSocketStatusLastChecked > std::chrono::milliseconds(200)) {
			guiWebSocketStatus = webSocket.getStatus();
			guiWebSocketStatusLastChecked = now;
		}
		ImGui::TextWrapped("Status: %s", guiWebSocketStatus.c_str());

		string apiUrl = "https://www.daftpenguin.com/api/rocket-league/stream-api/data/" + webSocket.platform + "/" + webSocket.username + "?cmd=loadout";
		if (ImGui::Button("Open External API (in browser)")) {
			ShellExecute(0, 0, stringToWstring(apiUrl).c_str(), 0, 0, SW_SHOW);
		}
		ImGui::TextWrapped("If button doesn't work:");
		ImGui::SameLine();
		ImGui::InputText("##ExternalAPIURL", (char*)apiUrl.c_str(), apiUrl.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
	}
}

void StreamAPIPlugin::RenderPushCommands()
{
	pushCommands.renderSettingsUI(cvarManager, gameWrapper, useWebSocket);
}

void StreamAPIPlugin::RenderConsoleCommands()
{
	ImGui::TextWrapped("With the introduction of \"action commands\", a few console commands were also added that might be fun to expose to your viewers.");
	ImGui::TextWrapped("Here is a list of the few console commands/cvars this plugin has added, as well as a few console commands/cvars provided by other plugins.");
	ImGui::Separator();
	ImGui::TextWrapped("Console commands added by this plugin:");
	
	for (auto it = internalConsoleCommands.begin(); it != internalConsoleCommands.end(); ++it) {
		ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered), it->name.c_str());
		ImGui::SameLine();
		ImGui::TextWrapped("params: %s. %s", it->params.c_str(), it->description.c_str());
	}

	ImGui::TextWrapped("The other \"streamapi_\" commands are not meant to be used for this. They're mostly internal commands necessary for the functionality of the plugin.");
	
	ImGui::Separator();

	ImGui::TextWrapped("Here's a non-exhaustive list of console commands either built into BakkesMod or provided by other plugins that may be fun to expose to viewers.");
	ImGui::TextWrapped("You can find the plugin's description for each command or cvar by pressing F6 and typing findcvar or findnotifier along with some characters matching a part of the cvar/command.");
	ImGui::TextWrapped("For convenience, there's a button to install each plugin by their ID. This does not check if the plugin already exists before installing.");

	ImGui::Dummy(ImVec2(0.0f, 5.0f));
	for (auto it = externalConsoleCommands.begin(); it != externalConsoleCommands.end(); ++it) {
		if (it->pluginId > 0) {
			if (ImGui::Button(("Install Plugin##Install" + to_string(it->pluginId)).c_str())) {
				cvarManager->executeCommand("sleep 0; bpm_install " + to_string(it->pluginId) + ";");
			}
			ImGui::SameLine();
			if (ImGui::Button(("Open Webpage##OpenWebpage" + to_string(it->pluginId)).c_str())) {
				ShellExecute(0, 0, it->getURL().c_str(), 0, 0, SW_SHOW);
			}
			ImGui::SameLine();
			ImGui::TextWrapped("%s by %s (%s) - %s", it->pluginName.c_str(), it->author.c_str(), (it->verified ? "verified" : "not verified"), it->description.c_str());
		}
		else {
			ImGui::TextWrapped("Built-in command/cvar: %s", it->description.c_str());
		}
	}

	ImGui::TextWrapped("Okay, you can just search bakkesplugins.com for CinderBlock and find a bunch of cool plugins with cvars/notifiers you can create commands with...");
	
}

void StreamAPIPlugin::RenderReportSubmission()
{
	ImGui::TextWrapped("Sometimes weird things happen and features don't work the way they're supposed to. If you notice a bug, you can use the form below to submit a report.");
	ImGui::TextWrapped("By clicking submit, your bakkesmod.log file, token details, and any info you share below will be sent to my server so that I can debug the issue.");
	ImGui::TextWrapped("PLEASE provide your streaming account name in the details if you don't have external bot support configured, as I have no way to follow up.");
	ImGui::TextWrapped("I am notified within minutes of new reports, so I may pop into your stream if I am free.");
	ImGui::InputTextMultiline("Details", guiReportDetails, sizeof(guiReportDetails));
	if (ImGui::Button("Submit")) {
		guiReportStatus = "";
		bool tokenConfigured = !webSocket.getToken().empty();
		if (strlen(guiReportDetails) < 20 && (!useWebSocket || !tokenConfigured)) {
			if (!useWebSocket) {
				guiReportStatus = "To report an issue with internal bot support enabled, you must provide details with at least 20 characters (include contact details please).";
			}
			else {
				guiReportStatus = "To report an issue without setting your external bot token, you must provide details with at least 20 characters (include contact details please).";
			}
		}
		else {
			SubmitReport(string(guiReportDetails), true);
		}
	}
	if (!guiReportStatus.empty()) {
		ImGui::SameLine();
		ImGui::TextWrapped(guiReportStatus.c_str());
	}
}

std::string StreamAPIPlugin::GetPluginName()
{
	return "Stream API Plugin";
}

void StreamAPIPlugin::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}