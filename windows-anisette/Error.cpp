#include <vector>
#include <string>
#include <map>
#include <exception>
#include <memory>
#include <functional>
#include <iostream>

#pragma region Error
class Error : public std::exception
{
public:
	Error(int code) : _code(code), _userInfo(std::map<std::string, std::string>())
	{
	}

	Error(int code, std::map<std::string, std::string> userInfo) : _code(code), _userInfo(userInfo)
	{
	}

	virtual std::string localizedDescription() const
	{
		return "";
	}

	int code() const
	{
		return _code;
	}

	std::map<std::string, std::string> userInfo() const
	{
		return _userInfo;
	}

	virtual std::string domain() const
	{
		return "com.rileytestut.AltServer.Error";
	}

	friend std::ostream& operator<<(std::ostream& os, const Error& error)
	{
		os << "Error: (" << error.domain() << "): " << error.localizedDescription() << " (" << error.code() << ")";
		return os;
	}

private:
	int _code;
	std::map<std::string, std::string> _userInfo;
};