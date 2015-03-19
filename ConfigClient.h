#pragma once
#include <string>

class ConfigClient
{
public:
	std::string binpath;
	std::string host;
	int port;
	bool		isTcp;
	std::string id;
	std::string domain;
	std::string password;
	bool		isDaemon;
	std::string playfilename;
	std::string recfilename;
	int severity;

	ConfigClient();
	~ConfigClient();
};

