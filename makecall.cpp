
#include "makecall.h"

/*
	Return 
		1	Unable to create UAC dialog
*/
int makeCall(ClientContext *ctx, const std::string &fromuri, const std::string &desturi)
{
	pj_status_t status;

	pj_str_t dst_uri = pj_str((char *) desturi.c_str());
	
	pjsip_dialog *dlg;
	pjmedia_sdp_session *local_sdp;
	pjsip_tx_data *tdata;

	pj_str_t local_uri = pj_str((char*) fromuri.c_str());

	/* Create UAC dialog */
	status = pjsip_dlg_create_uac(pjsip_ua_instance(),
		&local_uri,  /* local URI */
		&local_uri,  /* local Contact */
		&dst_uri,    /* remote URI */
		&dst_uri,    /* remote target */
		&dlg);	    /* dialog */
	if (status != PJ_SUCCESS) 
	{
		// Unable to create UAC dialog
		return 1;
	}

	/* If we expect the outgoing INVITE to be challenged, then we should
	* put the credentials in the dialog here, with something like this:
	*
	{
	pjsip_cred_info	cred[1];

	cred[0].realm	  = pj_str("sip.server.realm");
	cred[0].scheme    = pj_str("digest");
	cred[0].username  = pj_str("theuser");
	cred[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cred[0].data      = pj_str("thepassword");

	pjsip_auth_clt_set_credentials( &dlg->auth_sess, 1, cred);
	}
	*
	*/
	
	/* Get the SDP body to be put in the outgoing INVITE, by asking
	* media endpoint to create one for us.
	*/
	status = pjmedia_endpt_create_sdp(ctx->mediaEndpoint,	    // media endpt
		dlg->pool,	    // pool
		1,   // # of streams
		&ctx->mediaSockInfo,     // RTP sock info
		&local_sdp);	    // the SDP result
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


	// Create the INVITE session, and pass the SDP as the session's initial capability.
	pjsip_inv_session    *newInviteSession;
	status = pjsip_inv_create_uac(dlg, local_sdp, 0, &newInviteSession);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	/* If we want the initial INVITE to travel to specific SIP proxies,
	* then we should put the initial dialog's route set here. The final
	* route set will be updated once a dialog has been established.
	* To set the dialog's initial route set, we do it with something
	* like this:
	*
	{
	pjsip_route_hdr route_set;
	pjsip_route_hdr *route;
	const pj_str_t hname = { "Route", 5 };
	char *uri = "sip:proxy.server;lr";

	pj_list_init(&route_set);

	route = pjsip_parse_hdr( dlg->pool, &hname,
	uri, strlen(uri),
	NULL);
	PJ_ASSERT_RETURN(route != NULL, 1);
	pj_list_push_back(&route_set, route);

	pjsip_dlg_set_route_set(dlg, &route_set);
	}
	*
	* Note that Route URI SHOULD have an ";lr" parameter!
	*/

	// Create initial INVITE request contain a request and an SDP body
	status = pjsip_inv_invite(newInviteSession, &tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	// Send initial INVITE request. The invite session's state will be reported  via the invite session callbacks
	status = pjsip_inv_send_msg(newInviteSession, tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
	return PJ_SUCCESS;
}