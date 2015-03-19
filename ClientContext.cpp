#include "ClientContext.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>

ClientContext::ClientContext()
	: sip_af(PJ_AF_INET), isQuit(PJ_FALSE), id(""), domain(""), password(""), rtpport(4000),  port(5060), playfn(""), recfn(""),
	registarHost(""), registarPort(5060), registarIsTcp(false), severity(0), isDaemon(false)
{
	sessions = new CallSessionMap();
}

ClientContext::~ClientContext()
{
	delete sessions;
}

/*
	Parameters:
		af		pj_AF_INET() or pj_AF_INET6()
	Usage:
		pj_ansi_sprintf(temp, "<sip:simpleuac@%s:%d>", getHostName(pj_AF_INET()).c_str(), 5060);
*/
std::string ClientContext::getHostName(pj_uint16_t af)
{
	pj_sockaddr hostaddr;
	if (pj_gethostip(af, &hostaddr) != PJ_SUCCESS)
	{
		return "";
	}
	char hostip[PJ_INET6_ADDRSTRLEN + 2];
	pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);
	return std::string(hostip);
}

std::string ClientContext::getContact()
{
	std::ostringstream b;
	b << "<sip:" << id << "@" << domain << ":" << port << ">";
	return b.str();
}

std::string ClientContext::getRegistarUri()
{
	std::ostringstream b;
	b << "sip:" << registarHost << ":" << registarPort;
	return b.str();
}

std::string ClientContext::getEndpointName(pj_uint16_t af)
{
	return getHostName(af);
}
