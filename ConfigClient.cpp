#include "ConfigClient.h"


ConfigClient::ConfigClient()
	:
	binpath(""), host(""), port(5060), isTcp(false), 
	id(""), domain(""), password(""), 
	isDaemon(false), playfilename(""), recfilename(""), severity(0)
{
}


ConfigClient::~ConfigClient()
{
}
