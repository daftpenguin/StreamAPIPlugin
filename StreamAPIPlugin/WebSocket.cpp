#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Ws2_32.lib")

#include "WebSocket.h"
#include "StreamAPIPlugin.h"
#include "nlohmann/json.hpp"

using namespace std;
using namespace websocketpp::lib;
namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

WebSocket::WebSocket() : isRunning(false), token(""), status(WebSocketStatus::STOPPED), stopSocket(false), someDataIsDirty(false)
{
	data.emplace("loadout", WebSocketDataField("loadout"));
	data.emplace("sens", WebSocketDataField("sens"));
	data.emplace("camera", WebSocketDataField("camera"));
	data.emplace("bindings", WebSocketDataField("bindings"));
	data.emplace("video", WebSocketDataField("video"));
	data.emplace("training", WebSocketDataField("training"));
	data.emplace("rank", WebSocketDataField("rank"));

	srand(time(NULL));
}

void WebSocket::init(std::filesystem::path tokenFile)
{
	this->tokenFile = tokenFile;
	setStatus(WebSocketStatus::DISCONNECTED, "Disconnected", "");
}

bool WebSocket::setToken(std::string token)
{
	if (verifyToken(token)) {
		ofstream fout(tokenFile);
		if (fout.is_open()) {
			fout << token;
			fout.close();
			this->token = token;
			return true;
		}
		return false;
	}
	setStatus(WebSocketStatus::BAD_TOKEN, "Stopped", "Bad token configured");
	return false;
}

void WebSocket::start()
{
	lock_guard<mutex> lock(stopStartLock);
	internalStop();

	if (token.empty()) {
		setStatus(WebSocketStatus::STOPPED, "Stopped", "Must configure token");
		return;
	}
	
	thread = std::thread(&WebSocket::run, this);
}

void WebSocket::stop()
{
	lock_guard<mutex> lock(stopStartLock);
	internalStop();
}

void WebSocket::internalStop()
{
	if (isRunning) {
		{
			lock_guard<mutex> lck(dataMutex);
			stopSocket = true;
		}
		cv.notify_one();
		thread.join();
		_globalCvarManager->log("WebSocket thread joined");
		isRunning = false;
	}
}

void WebSocket::setData(std::string fieldName, std::string data)
{
	auto it = this->data.find(fieldName);
	if (it == this->data.end()) return;
	
	{
		lock_guard<mutex> lck(dataMutex);
		it->second.set(data);
		someDataIsDirty = true;
		// Only notify if socket can write data, we otherwise might be waiting for reconnect
		if (status == WebSocketStatus::AUTHENTICATED) {
			cv.notify_one();
		}
	}
}

void WebSocket::loadTokenFromFile(std::filesystem::path fpath)
{
	if (fs::exists(fpath)) {
		ifstream fin(fpath, ios::in);
		if (fin.is_open()) {
			stringstream tokenSS;
			string line;
			while (getline(fin, line)) {
				tokenSS << line;
			}
			if (setToken(tokenSS.str())) {
				_globalCvarManager->log("WebSocket: token loaded from file");
			}
		}
	}
	else {
		_globalCvarManager->log("WebSocket: token file doesn't exist. Must configure token to use websocket for external bot support");
	}
}

bool WebSocket::verifyToken(std::string token)
{
	try {
		json j = json::parse(token);
		bool valid = !(j.find("user") == j.end() || j.find("email") == j.end() || j.find("platform") == j.end() || j.find("token") == j.end());
		if (valid) {
			username = j.at("user").get<string>();
			platform = j.at("platform").get<string>();
			_globalCvarManager->log("WebSocket: From token: User: " + username + ", Platform: " + platform);
		}
		return valid;
	}
	catch (...) {
		return false;
	}
}

void WebSocket::run()
{
	stopSocket = false;
	isRunning = true;
	float reconnectDelay = 1.0;

#ifdef WEBSOCKET_DEBUG
	std::string uri = "ws://" + string(WEBSOCKET_LOCAL_SERVER_HOST) + ":" + WEBSOCKET_LOCAL_SERVER_PORT + WEBSOCKET_PATH;
#else
	std::string uri = "wss://" + string(WEBSOCKET_SERVER_HOST) + ":" + WEBSOCKET_SERVER_PORT + WEBSOCKET_PATH;
#endif

	while (!stopSocket) {
		try {
			setStatus(WebSocketStatus::CONNECTING, "Connecting", "");

			client c;
			c.init_asio();
			c.set_message_handler(bind(&WebSocket::on_message, this, placeholders::_1, placeholders::_2));
#ifndef WEBSOCKET_DEBUG
			c.set_tls_init_handler(bind(&WebSocket::on_tls_init, this));
#endif
			error_code ec;
			client::connection_ptr con = c.get_connection(uri, ec);
			if (ec) {
				setStatus(WebSocketStatus::FAILED, "Failed to create connection.", ec.message());
				break;
			}

			con->set_open_handler(bind(&WebSocket::on_open, this, &c, placeholders::_1));
			con->set_close_handler(bind(&WebSocket::on_close, this, &c, placeholders::_1));
			con->set_fail_handler(bind(&WebSocket::on_fail, this, &c, placeholders::_1));
			hdl = con->get_handle();
			c.connect(con);

			websocketpp::lib::thread asio_thread(&client::run, &c);

			setAllDataDirty(); // When reconnecting/first connect, push all data

			while (!stopSocket) {
				string dataToSend = waitForData();

				if (status == WebSocketStatus::AUTHENTICATED) reconnectDelay = 1.0;

				if (status != WebSocketStatus::AUTHENTICATED || stopSocket || c.stopped()) {
					break;
				}

				if (!dataToSend.empty()) {
					c.send(hdl, dataToSend, websocketpp::frame::opcode::text, ec);
					if (ec) {
						_globalCvarManager->log("WebSocket: error occurred when pushing updates: " + ec.message());
					}
					else {
						_globalCvarManager->log("WebSocket: pushed updates");
					}
				}
			}

			if (!c.stopped()) c.stop();
			asio_thread.join();
		}
		catch (websocketpp::exception const& e) {
			stringstream reasonSS;
			reasonSS << "Unexpected exception: " << e.what() << " (code: " << e.code() << ")";
			setStatus(WebSocketStatus::FAILED, "Connection failed", reasonSS.str());
			stopSocket = true;
			cv.notify_one();
		}

		_globalCvarManager->log("Locking for reconnection wait");
		std::unique_lock<std::mutex> lck(dataMutex);
		if (!stopSocket && status != WebSocketStatus::AUTHENTICATION_FAILED) {
			int ms = reconnectDelay * 1000 + (rand() % 1000);
			if (reconnectDelay < WEBSOCKET_MAX_BACKOFF_SECS) {
				reconnectDelay *= 2;
			}
			_globalCvarManager->log("WebSocket: Attempting reconnect in " + to_string(ms) + "ms");
			cv.wait_for(lck, std::chrono::milliseconds(ms));
		}
	}
}

std::string& WebSocket::getStatus()
{
	stringstream oss;
	{
		lock_guard<mutex> lock(statusLock);
		oss << this->statusMsg;
		if (!reason.empty()) oss << ". Reason: " << reason;
	}
	fullStatusMsg = oss.str();
	return fullStatusMsg;
}

void WebSocket::setStatus(WebSocketStatus status, std::string statusMsg, std::string reason)
{
	{
		lock_guard<mutex> lock(statusLock);
		this->status = status;
		this->statusMsg = statusMsg;
		this->reason = reason;
	}

	if (!reason.empty()) {
		_globalCvarManager->log("WebSocket: " + statusMsg + ". Reason: " + reason);
	}
	else {
		_globalCvarManager->log("WebSocket: " + statusMsg);
	}
}

void WebSocket::setAllDataDirty()
{
	dataMutex.lock();
	for (auto it = data.begin(); it != data.end(); ++it) {
		it->second.isDirty = true;
	}
	someDataIsDirty = true;
	dataMutex.unlock();
}

string WebSocket::waitForData()
{
	while (true) {
		std::unique_lock<std::mutex> lck(dataMutex);
		if (status == WebSocketStatus::AUTHENTICATING || (!someDataIsDirty && !stopSocket && status == WebSocketStatus::AUTHENTICATED)) {
			cv.wait(lck);
		}

		if (status == WebSocketStatus::AUTHENTICATING || status == WebSocketStatus::CONNECTING) continue; // Continue waiting on authentication
		if (status != WebSocketStatus::AUTHENTICATED || stopSocket) return "";

		if (someDataIsDirty) {
			stringstream oss;
			oss << "{";
			bool isFirst = true;
			for (auto it = data.begin(); it != data.end(); ++it) {
				if (it->second.isDirty) {
					if (!isFirst) oss << ",";
					it->second.append(oss);
					isFirst = false;
				}
			}
			oss << "}";
			someDataIsDirty = false;
			return oss.str();
		}
	}
}

void WebSocket::on_message(websocketpp::connection_hdl, client::message_ptr msg)
{
	auto payload = msg->get_payload();
	_globalCvarManager->log("WebSocket: Received payload: " + payload);
	if (payload.compare("OK") != 0) {
		if (status == WebSocketStatus::AUTHENTICATING) {
			setStatus(WebSocketStatus::AUTHENTICATION_FAILED, "Authentication failed", payload);
			stopSocket = true;
			cv.notify_one();
		}
		else {
			reason = payload;
			_globalCvarManager->log("WebSocket: Received error from server: " + reason);
		}
	}
	else if (status == WebSocketStatus::AUTHENTICATING) {
		setStatus(WebSocketStatus::AUTHENTICATED, "Connected", "");
		cv.notify_one();
	}
}

void WebSocket::on_open(client* c, websocketpp::connection_hdl hdl)
{
	setStatus(WebSocketStatus::AUTHENTICATING, "Authenticating", "");

	websocketpp::lib::error_code ec;
	c->send(hdl, token, websocketpp::frame::opcode::text, ec);
	if (ec) {
		setStatus(WebSocketStatus::AUTHENTICATION_FAILED, "Authentication failed", ec.message());
		stopSocket = true;
		cv.notify_one();
	}
}

void WebSocket::on_close(client* c, websocketpp::connection_hdl hdl)
{
	auto con = c->get_con_from_hdl(hdl);
	reason = con->get_remote_close_reason();
	if (status == WebSocketStatus::AUTHENTICATING) {
		setStatus(WebSocketStatus::AUTHENTICATION_FAILED, "Authentication failed", reason);
		stopSocket = true;
	}
	else {
		setStatus(WebSocketStatus::DISCONNECTED, "Connection closed", reason);
	}
	cv.notify_one();
}

void WebSocket::on_fail(client* c, websocketpp::connection_hdl hdl)
{
	auto con = c->get_con_from_hdl(hdl);
	auto ec = con->get_ec();
	stringstream reasonSS;
	reasonSS << ec.message() << " (code: " << ec.value() << ")";
	if (ec.value() == 2) reasonSS << ". Server might be down.";
	setStatus(WebSocketStatus::FAILED, "Connection failed", reasonSS.str());
	cv.notify_one();
}

#ifndef WEBSOCKET_DEBUG
context_ptr WebSocket::on_tls_init()
{
	context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

	try {
		ctx->set_options(boost::asio::ssl::context::default_workarounds |
			boost::asio::ssl::context::no_sslv2 |
			boost::asio::ssl::context::no_sslv3 |
			boost::asio::ssl::context::single_dh_use);
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
	return ctx;
}
#endif