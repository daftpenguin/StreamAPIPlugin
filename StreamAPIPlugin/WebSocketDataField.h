#pragma once

#include <string>
#include <sstream>
#include <mutex>

class WebSocketDataField
{
public:
	WebSocketDataField(std::string fieldName) : fieldName(fieldName), data(""), isDirty(false) {};

	void set(std::string data)
	{
		if (data[0] == '{') {
			this->data = data;
		}
		else {
			this->data = '"' + data + '"';
		}
		isDirty = true;
	}

	void append(std::stringstream &ss)
	{
		ss << "\"" << fieldName << "\":" << this->data;
		this->isDirty = false;
	}

	bool isDirty;

private:
	std::string fieldName;
	std::string data;
};