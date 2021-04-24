#pragma once

#define WEBSOCKET_DEBUG

#include "WebSocketDataField.h"

#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <filesystem>

#include <websocketpp/client.hpp>
#ifdef WEBSOCKET_DEBUG
#include <websocketpp/config/asio_no_tls_client.hpp>
typedef websocketpp::client<websocketpp::config::asio_client> client;
#else
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Ws2_32.lib")

#include <websocketpp/config/asio_client.hpp>
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;
#endif

#define WEBSOCKET_SERVER_HOST "daftpenguin.com"
#define WEBSOCKET_LOCAL_SERVER_HOST "localhost"
#define WEBSOCKET_SERVER_PORT "443"
#define WEBSOCKET_LOCAL_SERVER_PORT "9001"
#define WEBSOCKET_PATH "/api/rocket-league/stream-api/ws"
#define WEBSOCKET_MAX_BACKOFF_SECS 64
#define WEBSOCKET_SLEEP_MSECS 5000
//#define WEBSOCKET_NO_UPDATES_UNTIL_PING 5

enum class WebSocketStatus {
	BAD_TOKEN,
	STOPPED,
	CONNECTING,
	AUTHENTICATING,
	AUTHENTICATED,
	AUTHENTICATION_FAILED,
	DISCONNECTED,
	FAILED,
};

class WebSocket
{
public:
	WebSocket();
	bool setToken(std::string token);
	void start();
	void stop();
	void setData(std::string fieldName, std::string data);
	void init(std::filesystem::path tokenFile);
	void loadTokenFromFile(std::filesystem::path fpath);
	bool verifyToken(std::string token);
	std::string& getStatus();

private:
	void internalStop();
	void run();
	void setStatus(WebSocketStatus status, std::string statusMsg, std::string reason);
	void setAllDataDirty();
	std::string waitForData();

	std::mutex stopStartLock;

	std::mutex dataMutex;
	std::condition_variable cv;
	bool stopSocket;
	bool isRunning;
	bool someDataIsDirty;
	std::unordered_map<std::string, WebSocketDataField> data;

	std::string reason;
	std::thread thread;
	std::string token;
	std::filesystem::path tokenFile;

	std::mutex statusLock;
	WebSocketStatus status;
	std::string statusMsg;
	std::string fullStatusMsg;

	websocketpp::connection_hdl hdl;

	void on_message(websocketpp::connection_hdl, client::message_ptr msg);
	void on_open(client* c, websocketpp::connection_hdl hdl);
	void on_close(client* c, websocketpp::connection_hdl hdl);
	void on_fail(client* c, websocketpp::connection_hdl hdl);
#ifndef WEBSOCKET_DEBUG
	context_ptr on_tls_init();
#endif

	/*
	int noUpdates;
	std::string lastClosedReason;
	*/
};

