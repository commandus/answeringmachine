#include "CallSession.h"
#include "ClientContext.h"

CallSession::CallSession(ClientContext *ctx, pjsip_inv_session *inv) :
	doneState(0), inv(NULL), media_stream(NULL), local_sdp(NULL), remote_sdp(NULL),
	media_port(NULL), recorderport(NULL), playerport(NULL), resampleport(NULL), masterport(NULL), 
	playfn(""), recfn("")
{
	stream_info = new pjmedia_stream_info();
	(*ctx->sessions)[inv] = this;
	this->inv = inv;
	recfn = ctx->recfn;
	playfn = ctx->playfn;
}

void CallSession::stopMedia()
{
	// Destroy audio ports before stream since the audio port has threads that get/put frames to the stream.
	/*
	if (recorderport)
	{
		pjmedia_port_destroy(recorderport);
		recorderport = NULL;
	}
		
	if (masterport)
	{
		pjmedia_master_port_destroy(masterport, PJ_TRUE);
		masterport = NULL;
	}
	
	if (resampleport)
	{
		pjmedia_port_destroy(resampleport);
		resampleport = NULL;
	}
	if (playerport)
	{
		pjmedia_port_destroy(playerport);
		playerport = NULL;
	}
	*/

	if (masterport)
	{
		pjmedia_master_port_destroy(masterport, PJ_TRUE);
		masterport = NULL;
	}

	// Destroy streams
	if (media_stream)
	{
		pjmedia_stream_destroy(media_stream);
		media_stream = NULL;
	}
}

CallSession::~CallSession()
{
	stopMedia();
	delete stream_info;
}
