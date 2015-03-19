#ifndef CALLSESSION_H
#define CALLSESSION_H	1

#include <string>
#include <set>
#include <map>

#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia/sdp.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjsua-lib/pjsua.h>

class ClientContext;

class CallSession
{
public:
	int doneState;
	pjsip_inv_session *inv;
	// Call's audio stream
	pjmedia_stream *media_stream;
	pjmedia_stream_info *stream_info;
	const pjmedia_sdp_session *local_sdp;
	const pjmedia_sdp_session *remote_sdp;
	pjmedia_port *media_port;
	pjmedia_port *recorderport;
	pjmedia_port *playerport;
	pjmedia_port *resampleport;
	pjmedia_master_port *masterport;

	std::string playfn;
	std::string recfn;

	void stopMedia();
	CallSession(ClientContext *ctx, pjsip_inv_session *inv);
	~CallSession();
};

typedef std::map<pjsip_inv_session*, CallSession*>  CallSessionMap;
typedef std::set<CallSession*>  CallSessions;

#endif
