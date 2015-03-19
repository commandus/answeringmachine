#ifndef CLIENTCONTEXT_H
#define CLIENTCONTEXT_H	1

#include <string>
#include <list>

#include <pjsip.h>
#include <pjlib.h>
#include <pjmedia.h>
#include <pjsip_ua.h>

#include "CallSession.h"

/*
Call
*/
typedef struct call_t
{
	pjsip_inv_session	*inv;
	// application context
	struct app_t		*app;
	bool operator == (const call_t &v) const
	{
		return inv == v.inv;
	}
} call_t;

/*
Call list
*/
typedef std::list<call_t> calls_t;

/*
	PJSIP application application context
*/
class ClientContext
{
public:
	//  PJ_AF_INET, PJ_AF_INET6
	pj_uint16_t sip_af;
	int rtpport;
	std::string id;
	std::string domain;
	std::string password;
	pj_uint16_t port;

	std::string registarHost;
	int registarPort;
	bool registarIsTcp;
	std::string getRegistarUri();

	pj_bool_t			isTcp;
	pj_bool_t			isLogging;

	pj_caching_pool		cachingPool;
	pj_pool_t			*pool;
	pjsip_tpfactory		*tpfactory;
	pjsip_transport		*sipTransport;
	pjsip_endpoint		*sipEndpoint;
	pjmedia_endpt		*mediaEndpoint;
	// calls_t			calls;
	pj_bool_t			isQuit;
	pj_thread_t			*workerThread;

	pjmedia_transport    *mediaTransport;
	pjmedia_transport_info mediaTransportInfo;
	pjmedia_sock_info     mediaSockInfo;

	// Call variables
	CallSessionMap		*sessions;

	std::string playfn;
	std::string recfn;

	int severity;
	bool isDaemon;

	ClientContext();
	~ClientContext();
	static std::string getHostName(pj_uint16_t af);
	static std::string getEndpointName(pj_uint16_t af);
	std::string getContact();
};

#endif
