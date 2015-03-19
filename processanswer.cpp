#include "processanswer.h"
#include "logging.h"
#include "ClientContext.h"
#include "makecall.h"

static ClientContext ctx;

ClientContext *getClientContext()
{
	return &ctx;
}

// Prototypes
// Callback to be called when SDP negotiation is done in the call
static void call_on_media_update(pjsip_inv_session *inv, pj_status_t status);

// Callback to be called when invite session's state has changed
static void call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);

// Callback to be called when dialog has forked
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);

// Callback to be called to handle incoming requests outside dialogs
static pj_bool_t on_rx_request(pjsip_rx_data *rdata);

// logger module instance
static pjsip_module msg_logger =
{
	NULL, NULL,					// prev, next.		
	{ "mod-log", 7 },			// Name.		
	-1,							// Id			
	PJSIP_MOD_PRIORITY_TRANSPORT_LAYER - 1,// Priority	        
	NULL,						// load()		
	NULL,						// start()		
	NULL,						// stop()		
	NULL,						// unload()		
	&logging_on_rx_msg,			// on_rx_request()	
	&logging_on_rx_msg,			// on_rx_response()	
	&logging_on_tx_msg,			// on_tx_request.	
	&logging_on_tx_msg,			// on_tx_response()	
	NULL,						// on_tsx_state()	
};

// handle incoming INVITE request message, where we will create a dialog and INVITE session for it.
static pjsip_module mod_simpleua =
{
	NULL, NULL,			// prev, next
	{ "mod-autoanswer", 14 },	    // Name.			
	-1,				    // Id			
	PJSIP_MOD_PRIORITY_APPLICATION, // Priority			
	NULL,			    // load()			
	NULL,			    // start()			
	NULL,			    // stop()			
	NULL,			    // unload()			
	&on_rx_request,	    // on_rx_request()		
	NULL,			    // on_rx_response()		
	NULL,			    // on_tx_request.		
	NULL,			    // on_tx_response()		
	NULL,			    // on_tsx_state()		
};

void fillCredentials(pjsip_cred_info *cred)
{
	pj_bzero(cred, sizeof(cred));
	cred->realm = pj_str((char *) ctx.domain.c_str());
	cred->scheme = pj_str("digest");
	cred->username = pj_str((char *)ctx.id.c_str());
	cred->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cred->data = pj_str((char *)ctx.password.c_str());
}

// regc callback
static void client_cb(struct pjsip_regc_cbparam *param)
{
	struct client *client = (struct client*) param->token;
	pjsip_regc_info info;
	pj_status_t status;

	status = pjsip_regc_get_info(param->regc, &info);
	pj_assert(status == PJ_SUCCESS);
	/*
	= (param->status != PJ_SUCCESS);
	= param->code;
	have_reg = info.auto_reg && info.interval>0 && param->expiration > 0;
	= param->expiration;
	= param->contact_cnt;
	= info.interval;
	= info.next_reg;
	*/
	if (true)
		pjsip_regc_destroy(param->regc);
}

// Register to SIP server by creating SIP account
pj_status_t doRegister(const std::string registrar_uri, const std::string &id, const std::string &domain, const std::string &password)
{
	pjsip_regc *regc;
	std::string contact = ctx.getContact();
	const pj_str_t aor = pj_str((char *)contact.c_str());
	pjsip_tx_data *tdata;
	pj_status_t status;
	
	status = pjsip_regc_create(ctx.sipEndpoint, &ctx, &client_cb, &regc);
	if (status != PJ_SUCCESS)
		return -100;
	unsigned int contact_cnt = 1;
	unsigned int expires = 60;
	pj_str_t contacts[] = { aor };

	pj_str_t m = pj_str((char *)registrar_uri.c_str());
	status = pjsip_regc_init(regc, &m, &aor, &aor, contact_cnt, contacts, expires ? expires : 60);
	if (status != PJ_SUCCESS)
	{
		app_perror(TAG, "Unable to init registration", status);
		pjsip_regc_destroy(regc);
		return -110;
	}

	pjsip_cred_info cred;
	fillCredentials(&cred);

	status = pjsip_regc_set_credentials(regc, 1, &cred);
	if (status != PJ_SUCCESS) 
	{
		app_perror(TAG, "Unable to set credentials", status);
		pjsip_regc_destroy(regc);
		return -115;
	}

	// Register
	status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
	if (status != PJ_SUCCESS) 
	{
		pjsip_regc_destroy(regc);
		return -120;
	}
	status = pjsip_regc_send(regc, tdata);
	return status;
}

/*
* If called with argument, treat argument as SIP URL to be called.
* Otherwise wait for incoming calls.
*/
int process(ConfigClient &config)
{
	ctx.registarHost = config.host;
	ctx.registarPort = config.port;
	ctx.registarIsTcp = config.isTcp;
	ctx.id = config.id;
	ctx.domain = config.domain;
	ctx.password = config.password;
	ctx.playfn = config.playfilename;
	ctx.recfn = config.recfilename;
	ctx.severity = config.severity;
	ctx.isDaemon = config.isDaemon;

	// init PJLIB first
	pj_status_t status = pj_init();
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// maximun is 5
	pj_log_set_level(ctx.severity);

	// init PJLIB-UTIL
	status = pjlib_util_init();
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// create a pool factory before we can allocate any memory
	pj_caching_pool_init(&ctx.cachingPool, &pj_pool_factory_default_policy, 0);

	// Create global endpoint
	status = pjsip_endpt_create(&ctx.cachingPool.factory, (char*)ctx.getEndpointName(ctx.sip_af).c_str(), &ctx.sipEndpoint);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
	
	// Add UDP or TCP transport. Alternatively, application can use pjsip_udp_transport_attach() to start UDP transport, if it already has an UDP socket e.g. after it resolves the address with STUN
	{
		pj_sockaddr addr;
		pj_sockaddr_init(ctx.sip_af, &addr, NULL, 0);

		if (ctx.sip_af == pj_AF_INET())
		{
			if (ctx.registarIsTcp)
				status = pjsip_tcp_transport_start(ctx.sipEndpoint, &addr.ipv4, 1, &ctx.tpfactory);
			else
				status = pjsip_udp_transport_start(ctx.sipEndpoint, &addr.ipv4, NULL, 1, &ctx.sipTransport);
		}
		else if (ctx.sip_af == pj_AF_INET6())
		{
			status = pjsip_udp_transport_start6(ctx.sipEndpoint, &addr.ipv6, NULL, 1, NULL);
		}
		else 
		{
			status = PJ_EAFNOTSUP;
		}

		if (status != PJ_SUCCESS) 
		{
			app_perror(TAG, "Unable to start UDP transport", status);
			return 1;
		}
		// get port
		pj_sock_t sock = pjsip_udp_transport_get_socket(ctx.sipTransport);
		pj_sockaddr sock_addr;
		int addr_len;
		addr_len = sizeof(sock_addr);
		status = pj_sock_getsockname(sock, &sock_addr, &addr_len);
		ctx.port = pj_ntohs(sock_addr.ipv4.sin_port);
	}
	
	// Init transaction layer.
	status = pjsip_tsx_layer_init_module(ctx.sipEndpoint);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// Initialize UA layer module.
	status = pjsip_ua_init_module(ctx.sipEndpoint, NULL);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// Init invite session module. The on_state_changed and on_new_session callbacks are mandatory.
	{
		pjsip_inv_callback inv_cb;

		/* Init the callback for INVITE session: */
		pj_bzero(&inv_cb, sizeof(inv_cb));
		inv_cb.on_state_changed = &call_on_state_changed;
		inv_cb.on_new_session = &call_on_forked;
		inv_cb.on_media_update = &call_on_media_update;

		// Initialize invite session module
		status = pjsip_inv_usage_init(ctx.sipEndpoint, &inv_cb);
		PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
	}

	// Initialize 100rel support
	status = pjsip_100rel_init_module(ctx.sipEndpoint);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

	// Register our module to receive incoming requests.
	status = pjsip_endpt_register_module(ctx.sipEndpoint, &mod_simpleua);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// Register message logger module.
	status = pjsip_endpt_register_module(ctx.sipEndpoint, &msg_logger);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// Initialize media endpoint. This will implicitly initialize PJMEDIA too.
#if PJ_HAS_THREADS
	status = pjmedia_endpt_create(&ctx.cachingPool.factory, NULL, 1, &ctx.mediaEndpoint);
#else
	status = pjmedia_endpt_create(&cp.factory,
		pjsip_endpt_get_ioqueue(g_endpt),
		0, &g_med_endpt);
#endif
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
	// Add PCMA/PCMU codec to the media endpoint.
#if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0
	status = pjmedia_codec_g711_init(ctx.mediaEndpoint);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
#endif

	// Create media transport send/receive RTP/RTCP socket. One media transport is needed for each call. Application may opt to re-use the same media transport for subsequent calls.
	status = pjmedia_transport_udp_create3(ctx.mediaEndpoint, ctx.sip_af, NULL, NULL, ctx.rtpport, 0, &ctx.mediaTransport);
	if (status != PJ_SUCCESS) 
	{
		app_perror(TAG, "Unable to create media transport", status);
		return 1;
	}

	// Get socket address, port of the media transport to create SDP
	pjmedia_transport_info_init(&ctx.mediaTransportInfo);
	pjmedia_transport_get_info(ctx.mediaTransport, &ctx.mediaTransportInfo);

	pj_memcpy(&ctx.mediaSockInfo, &ctx.mediaTransportInfo.sock_info, sizeof(pjmedia_sock_info));

	doRegister(ctx.getRegistarUri(), ctx.id, ctx.domain, ctx.password);
	PJ_LOG(3, (TAG, "Ready to accept incoming calls..."));
	/*
		std::string fromuri;
		std::string desturi;
		makeCall(&ctx, fromuri, desturi);
	*/

	// PJSIP event loop
	while (!ctx.isQuit)
	{
		pj_time_val timeout = { 0, 10 };
		pjsip_endpt_handle_events(ctx.sipEndpoint, &timeout);
		
		auto it = ctx.sessions->begin();
		while (it != ctx.sessions->end())
		{
			CallSession *s = it->second;
			if (s->doneState == 1)
			{
				s->doneState = 2;
				pjsip_tx_data *tdata;
				pj_str_t m = { "end", 3 };
				pjsip_inv_end_session(s->inv, 200, &m, &tdata);
				status = pjsip_inv_send_msg(s->inv, tdata);
				s->stopMedia();
			}

			if (s->doneState == 3)
			{
				delete s;
				auto toErase = it;
				++it;
				ctx.sessions->erase(toErase);
			}
			else
				++it;
		}
	}

	// Destroy audio ports before stream since the audio port has threads that get/put frames to the stream.
	for (auto s = ctx.sessions->begin(); s != ctx.sessions->end(); ++s)
	{
		delete s->second;
	}
	ctx.sessions->clear();

	// Destroy media transport
	if (ctx.mediaTransport)
		pjmedia_transport_close(ctx.mediaTransport);

	// Deinit pjmedia endpoint
	if (ctx.mediaEndpoint)
		pjmedia_endpt_destroy(ctx.mediaEndpoint);

	// Deinit pjsip endpoint
	if (ctx.sipEndpoint)
		pjsip_endpt_destroy(ctx.sipEndpoint);

	// Release pool
	if (ctx.pool)
		pj_pool_release(ctx.pool);

	return 0;
}

/*
* Callback when INVITE session state has changed.
* This callback is registered when the invite session module is initialized.
* We mostly want to know when the invite session has been disconnected,
* so that we can quit the application.
*/
static void call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
	PJ_UNUSED_ARG(e);

	if (inv->state == PJSIP_INV_STATE_DISCONNECTED) 
	{
		PJ_LOG(3, (TAG, "Call DISCONNECTED [reason=%d (%s)]", inv->cause, pjsip_get_status_text(inv->cause)->ptr));
		ctx.sessions->at(inv)->doneState = 3;
	}
	else 
	{
		PJ_LOG(3, (TAG, "Call state changed to %s", pjsip_inv_state_name(inv->state)));
	}
}

// This callback is called when dialog has forked
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
}

/*
* Callback when incoming requests outside any transactions and any
* dialogs are received. We're only interested to hande incoming INVITE
* request, and we'll reject any other requests with 500 response.
*/
static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
	pj_str_t local_uri;
	pjsip_dialog *dlg;
	pjmedia_sdp_session *local_sdp;
	pjsip_tx_data *tdata;
	unsigned options = 0;
	pj_status_t status;
	
	// Respond (statelessly) any non-INVITE requests with 500
	if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) 
	{
		if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) 
		{
			pj_str_t reason = pj_str("Unable to handle this request");
			pjsip_endpt_respond_stateless(ctx.sipEndpoint, rdata, 500, &reason, NULL, NULL);
		}
		return PJ_TRUE;
	}

	// Reject INVITE if we already have an INVITE session in progress.
	if (false) 
	{
		pj_str_t reason = pj_str("Another call is in progress");
		pjsip_endpt_respond_stateless(ctx.sipEndpoint, rdata, 500, &reason, NULL, NULL);
		return PJ_TRUE;
	}

	// Verify that we can handle the request.
	status = pjsip_inv_verify_request(rdata, &options, NULL, NULL, ctx.sipEndpoint, NULL);
	if (status != PJ_SUCCESS) 
	{
		pj_str_t reason = pj_str("Can not handle this INVITE");
		pjsip_endpt_respond_stateless(ctx.sipEndpoint, rdata, 500, &reason, NULL, NULL);
		return PJ_TRUE;
	}

	// Contact
	std::string contact(ctx.getContact());
	local_uri = pj_str((char *) contact.c_str());

	// Create UAS dialog.
	status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, &local_uri, &dlg);
	if (status != PJ_SUCCESS) 
	{
		pjsip_endpt_respond_stateless(ctx.sipEndpoint, rdata, 500, NULL, NULL, NULL);
		app_perror(TAG, "Unable to create UAS dialog", status);
		return PJ_TRUE;
	}

	{
		pjsip_cred_info	cred;
		fillCredentials(&cred);
		pjsip_auth_clt_set_credentials( &dlg->auth_sess, 1, &cred);
	}

	// Get media capability from media endpoint:
	status = pjmedia_endpt_create_sdp(ctx.mediaEndpoint, rdata->tp_info.pool, 1, &ctx.mediaSockInfo, &local_sdp);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

	pjsip_inv_session    *g_inv;
	// Create invite session, and pass both the UAS dialog and the SDP
	status = pjsip_inv_create_uas(dlg, rdata, local_sdp, 0, &g_inv);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

	// The very first response to an INVITE must be created with pjsip_inv_initial_answer(). Subsequent responses to the same transaction MUST use pjsip_inv_answer().
	status = pjsip_inv_initial_answer(g_inv, rdata, 180, NULL, NULL, &tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

	// Send the 180 response
	status = pjsip_inv_send_msg(g_inv, tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

	// Now create 200 response.
	status = pjsip_inv_answer(g_inv,
		200, NULL,	/* st_code and st_text */
		NULL,		/* SDP already specified */
		&tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

	// Send the 200 response.
	status = pjsip_inv_send_msg(g_inv, tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);
	return PJ_TRUE;
}

pj_status_t onWaveEOF(pjmedia_port *port, void *data)
{
	CallSession *s = (CallSession*) data;
	if ((s) && (s->doneState == 0))
		s->doneState = 1;
	return !PJ_SUCCESS;	// stop
}

/*
* Callback when SDP negotiation has completed.
* We are interested with this callback because we want to start media
* as soon as SDP negotiation is completed.
*/
static void call_on_media_update(pjsip_inv_session *inv, pj_status_t status)
{
	if (status != PJ_SUCCESS) 
	{
		app_perror(TAG, "SDP negotiation has failed", status);
		// Here we should disconnect call if we're not in the middle of initializing an UAS dialog and if this is not a re-INVITE.
		return;
	}

	CallSession *sw = new CallSession(&ctx, inv);

	// Get both local and remote SDP
	status = pjmedia_sdp_neg_get_active_local(inv->neg, &sw->local_sdp);
	status = pjmedia_sdp_neg_get_active_remote(inv->neg, &sw->remote_sdp);

	// Create stream info based on the media audio SDP
	status = pjmedia_stream_info_from_sdp(sw->stream_info, inv->dlg->pool, ctx.mediaEndpoint, sw->local_sdp, sw->remote_sdp, 0);
	if (status != PJ_SUCCESS) 
	{
		app_perror(TAG, "Unable to create audio stream info", status);
		return;
	}

	// todo change some settings in the stream info: jitter buffer, codec, etc) before create the stream

	// Create new audio media stream using stream info, media socket created earlier
	status = pjmedia_stream_create(ctx.mediaEndpoint, inv->dlg->pool, sw->stream_info, ctx.mediaTransport, NULL, &sw->media_stream);
	if (status != PJ_SUCCESS) 
	{
		app_perror(TAG, "Unable to create audio stream", status);
		return;
	}

	// Start the audio stream
	status = pjmedia_stream_start(sw->media_stream);
	if (status != PJ_SUCCESS) 
	{
		app_perror(TAG, "Unable to start audio stream", status);
		return;
	}

	// Get the media port interface of the audio stream (containing get_frame() and put_frame() function.
	status = pjmedia_stream_get_port(sw->media_stream, &sw->media_port);

	// Create WAVE file writer port
	status = pjmedia_wav_writer_port_create(inv->pool, sw->recfn.c_str(),
		PJMEDIA_PIA_SRATE(&sw->media_port->info),// clock rate
		PJMEDIA_PIA_CCNT(&sw->media_port->info),// channel count
		PJMEDIA_PIA_SPF(&sw->media_port->info), // samples per frame
		PJMEDIA_PIA_BITS(&sw->media_port->info),// bits per sample
		0, 0,
		&sw->recorderport);

	status = pjmedia_wav_player_port_create(inv->pool, sw->playfn.c_str(), PJMEDIA_PIA_PTIME(&sw->media_port->info), 0, 0, &sw->playerport);
	if (status != PJ_SUCCESS)
	{
		app_perror(TAG, "Unable to create wave player", status);
		return;
	}
	pjmedia_wav_player_set_eof_cb(sw->playerport, sw, onWaveEOF);
	
	// resample port 
	status = pjmedia_resample_port_create(inv->pool, sw->playerport, PJMEDIA_PIA_SRATE(&sw->media_port->info), 0, &sw->resampleport);

	// master port connect resampled source to media 
	status = pjmedia_master_port_create(inv->pool, sw->resampleport, sw->media_port, 0, &sw->masterport);
	if (status != PJ_SUCCESS)
	{
		app_perror(TAG, "Unable to create master media port", status);
		return;
	}

	status = pjmedia_master_port_start(sw->masterport);
	if (status != PJ_SUCCESS) {
		app_perror(TAG, "Unable to create sound port", status);
		PJ_LOG(3, (TAG, "%d %d %d %d",
			PJMEDIA_PIA_SRATE(&sw->media_port->info),/* clock rate	    */
			PJMEDIA_PIA_CCNT(&sw->media_port->info),/* channel count    */
			PJMEDIA_PIA_SPF(&sw->media_port->info), /* samples per frame*/
			PJMEDIA_PIA_BITS(&sw->media_port->info) /* bits per sample  */
			));
		return;
	}
}
