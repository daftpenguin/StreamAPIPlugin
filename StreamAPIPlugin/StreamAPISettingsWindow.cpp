#include "StreamAPIPlugin.h"

#include <windows.h>
#include <shellapi.h>

using namespace std;

void StreamAPIPlugin::RenderSettings()
{
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
			ShellExecute(0, 0, L"https://www.daftpenguin.com/rocket-league/stream-api/setup", 0, 0, SW_SHOW);
		}

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

		/*auto now = std::chrono::system_clock::now();
		if (now - guiWebSocketStatusLastChecked > std::chrono::seconds(1)) {
			guiWebSocketStatus = webSocket.getStatus();
			guiWebSocketStatusLastChecked = now;
		}*/
		guiWebSocketStatus = webSocket.getStatus();
		ImGui::TextWrapped("Status: %s", guiWebSocketStatus.c_str());
	}

	ImGui::Separator();
	ImGui::TextWrapped("Stream API Plugin created by DaftPenguin");
}

std::string StreamAPIPlugin::GetPluginName()
{
	return "Stream API Plugin";
}

void StreamAPIPlugin::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}