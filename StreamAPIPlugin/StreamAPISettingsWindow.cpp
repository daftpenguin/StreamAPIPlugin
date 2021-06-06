#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "pluginsdk.lib")

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include "cpp-httplib/httplib.h"

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
		ShellExecute(0, 0, wstring(commandsUrl.begin(), commandsUrl.end()).c_str(), 0, 0, SW_SHOW);
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
			ShellExecute(0, 0, wstring(internalUrl.begin(), internalUrl.end()).c_str(), 0, 0, SW_SHOW);
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
			ShellExecute(0, 0, wstring(apiUrl.begin(), apiUrl.end()).c_str(), 0, 0, SW_SHOW);
		}
		ImGui::TextWrapped("If button doesn't work:");
		ImGui::SameLine();
		ImGui::InputText("##ExternalAPIURL", (char*)apiUrl.c_str(), apiUrl.size(), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ReadOnly);
	}
}

void StreamAPIPlugin::RenderPushCommands()
{
	pushCommands.renderSettingsUI(cvarManager, gameWrapper);
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
		if (guiReportDetails[0] == '\0' && (!useWebSocket || !tokenConfigured)) {
			if (!useWebSocket) {
				guiReportStatus = "You must provide details (and contact details please) to report an issue with internal bot support enabled.";
			}
			else {
				guiReportStatus = "You must provide details (and contact details please) to report an issue without setting your external bot token.";
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