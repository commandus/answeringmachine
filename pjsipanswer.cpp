#include <string>
#include <list>
#include <algorithm>
#include <pjsua.h>

#define  TAG "sip-answer"

// application context forward declaration
struct app_t;

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
	Application context
*/
typedef struct app_t
{
	int sip_af;
	int sip_port;
	pj_bool_t sip_tcp;
	pj_bool_t			 enable_msg_logging;

	pj_caching_pool		cp;
	pj_pool_t			*pool;
	pjsip_endpoint		*sip_endpt;
	pjmedia_endpt		*med_endpt;
	calls_t				calls;
	pj_bool_t			quit;
	pj_thread_t			*worker_thread;
} app_t;

std::string getErrorString(pj_status_t errcode)
{
	char errbuf[80];
	pj_strerror(errcode, errbuf, sizeof(errbuf));
	return std::string((char*) &errbuf);
}

/* Notification on incoming messages */
pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
	PJ_LOG(3, (TAG, "RX %d bytes %s from %s %s:%d:\n%.*s\n--end msg--",
		rdata->msg_info.len,
		pjsip_rx_data_get_info(rdata),
		rdata->tp_info.transport->type_name,
		rdata->pkt_info.src_name,
		rdata->pkt_info.src_port,
		(int)rdata->msg_info.len,
		rdata->msg_info.msg_buf));
	return PJ_FALSE;
}

/* Notification on outgoing messages */
pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
	PJ_LOG(3, (TAG, "TX %d bytes %s to %s %s:%d:\n%.*s\n--end msg--",
		(tdata->buf.cur - tdata->buf.start),
		pjsip_tx_data_get_info(tdata),
		tdata->tp_info.transport->type_name,
		tdata->tp_info.dst_name,
		tdata->tp_info.dst_port,
		(int)(tdata->buf.cur - tdata->buf.start),
		tdata->buf.start));
	return PJ_SUCCESS;
}

int run(void *arg)
{
	 while (!((app_t*)arg)->quit) {
		pj_time_val interval = { 0, 20 };
		pjsip_endpt_handle_events(((app_t*)arg)->sip_endpt, &interval);
	}
	return 0;
}

void hangupAll(app_t *app)
{
	for (calls_t::iterator call(app->calls.begin()); call != app->calls.end(); ++call)
	{
		if (call->inv && call->inv->state <= PJSIP_INV_STATE_CONFIRMED) {
			pj_status_t status;
			pjsip_tx_data *tdata;

			status = pjsip_inv_end_session(call->inv, PJSIP_SC_BUSY_HERE, NULL, &tdata);
			if (status == PJ_SUCCESS && tdata)
				pjsip_inv_send_msg(call->inv, tdata);
		}
	}
}

#define CHECK_STATUS()	do { if (status != PJ_SUCCESS) return status; } while (0)

void call_on_media_update(pjsip_inv_session *inv, pj_status_t status);
void call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);
void call_on_rx_offer(pjsip_inv_session *inv, const pjmedia_sdp_session *offer);
void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);
pj_bool_t on_rx_request(pjsip_rx_data *rdata);

/* This is a PJSIP module to be registered by application to handle
* incoming requests outside any dialogs/transactions. The main purpose
* here is to handle incoming INVITE request message, where we will
* create a dialog and INVITE session for it.
*/
static pjsip_module mod_sipanswer =
{
	NULL, NULL,					    /* prev, next.		*/
	{ "mod-sipanswer", 11 },	    /* Name.			*/
	-1,								/* Id			*/
	PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
	NULL,							/* load()			*/
	NULL,							/* start()			*/
	NULL,							/* stop()			*/
	NULL,							/* unload()			*/
	&on_rx_request,					/* on_rx_request()		*/
	NULL,							/* on_rx_response()		*/
	NULL,							/* on_tx_request.		*/
	NULL,							/* on_tx_response()		*/
	NULL,							/* on_tsx_state()		*/
};

/* The module instance. */
static pjsip_module mod_msglogger =
{
	NULL, NULL,					/* prev, next.		*/
	{ "mod-msg-log", 13 },		/* Name.		*/
	-1,							/* Id			*/
	PJSIP_MOD_PRIORITY_TRANSPORT_LAYER - 1,/* Priority	        */
	NULL,						/* load()		*/
	NULL,						/* start()		*/
	NULL,						/* stop()		*/
	NULL,						/* unload()		*/
	&logging_on_rx_msg,			/* on_rx_request()	*/
	&logging_on_rx_msg,			/* on_rx_response()	*/
	&logging_on_tx_msg,			/* on_tx_request.	*/
	&logging_on_tx_msg,			/* on_tx_response()	*/
	NULL,						/* on_tsx_state()	*/
};

pj_status_t initStack(app_t *app)
{
	pj_sockaddr addr;
	pjsip_inv_callback inv_cb;
	pj_status_t status;

	pj_log_set_level(3);

	status = pjlib_util_init();
	CHECK_STATUS();

	pj_caching_pool_init(&app->cp, NULL, 0);
	app->pool = pj_pool_create(&app->cp.factory, TAG, 512, 512, 0);

	status = pjsip_endpt_create(&app->cp.factory, NULL, &app->sip_endpt);
	CHECK_STATUS();

	pj_log_set_level(4);
	pj_sockaddr_init((pj_uint16_t)app->sip_af, &addr, NULL, 0); //(pj_uint16_t)app->sip_port
	if (app->sip_af == pj_AF_INET()) 
	{
		if (app->sip_tcp) {
			status = pjsip_tcp_transport_start(app->sip_endpt, &addr.ipv4, 1, NULL);
		}
		else {
			status = pjsip_udp_transport_start(app->sip_endpt, &addr.ipv4, NULL, 1, NULL);
		}
	}
	else if (app->sip_af == pj_AF_INET6()) {
		status = pjsip_udp_transport_start6(app->sip_endpt, &addr.ipv6,
			NULL, 1, NULL);
	}
	else {
		status = PJ_EAFNOTSUP;
	}
	pj_log_set_level(3);
	CHECK_STATUS();

	
	status = pjsip_tsx_layer_init_module(app->sip_endpt) ||
		pjsip_ua_init_module(app->sip_endpt, NULL);
	CHECK_STATUS();

	pj_bzero(&inv_cb, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;
	inv_cb.on_rx_offer = &call_on_rx_offer;

	status = pjsip_inv_usage_init(app->sip_endpt, &inv_cb) ||
		pjsip_100rel_init_module(app->sip_endpt) ||
		pjsip_endpt_register_module(app->sip_endpt, &mod_sipanswer);
	CHECK_STATUS();
	
	if (app->enable_msg_logging)
		status = pjsip_endpt_register_module(app->sip_endpt, &mod_msglogger);
	CHECK_STATUS();

	// pjmedia_endpt_create(&app->cp.factory, pjsip_endpt_get_ioqueue(app->sip_endpt), 0, &app->med_endpt)

	/*
	* Initialize media endpoint. This will implicitly initialize PJMEDIA too.
	*/
#if PJ_HAS_THREADS
	status = pjmedia_endpt_create(&app->cp.factory, NULL, 1, &app->med_endpt);
#else
	status = pjmedia_endpt_create(&app->cp.factory, pjsip_endpt_get_ioqueue(g_endpt), 0, &&app->med_endpt);
#endif
	CHECK_STATUS();

	/*
	* Add PCMA/PCMU codec to the media endpoint.
	*/
#if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0
	status = pjmedia_codec_g711_init(app->med_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
#endif




	status = pj_thread_create(app->pool, TAG, &run, app, 0, 0,	&app->worker_thread);
	CHECK_STATUS();

	return PJ_SUCCESS;
}

void doneStack(app_t *app)
{
	enum { WAIT_CLEAR = 5000, WAIT_INTERVAL = 500 };
	unsigned i;
	PJ_LOG(3, (TAG, "Shutting down.."));
	/* Wait until all clear */
	hangupAll(app);
	for (i = 0; i<WAIT_CLEAR / WAIT_INTERVAL; ++i) {
		unsigned j = 0;
		for (calls_t::iterator call(app->calls.begin()); call != app->calls.end(); ++call)
		{
			j++;
			if (call->inv && call->inv->state <= PJSIP_INV_STATE_CONFIRMED)
				break;
		}
		if (j == app->calls.size())
			return;
		pj_thread_sleep(WAIT_INTERVAL);
	}

	app->quit = PJ_TRUE;
	if (app->worker_thread) {
		pj_thread_join(app->worker_thread);
		app->worker_thread = NULL;
	}

	//if (app->med_endpt)
	//pjmedia_endpt_destroy(app->med_endpt);

	if (app->sip_endpt)
		pjsip_endpt_destroy(app->sip_endpt);
	if (app->pool)
		pj_pool_release(app->pool);
	pj_caching_pool_destroy(&app->cp);
}

void destroy_call(call_t &call)
{
	// calls_t::iterator c = find(app.calls.begin(), app.calls.end(), *call);
	call.app->calls.remove(call);
	// call.inv = NULL;
}

pjmedia_sdp_attr * find_remove_sdp_attrs(unsigned *cnt,
	pjmedia_sdp_attr *attr[],
	unsigned cnt_attr_to_remove,
	const char* attr_to_remove[])
{
	pjmedia_sdp_attr *found_attr = NULL;
	int i;

	for (i = 0; i<(int)*cnt; ++i) {
		unsigned j;
		for (j = 0; j<cnt_attr_to_remove; ++j) {
			if (pj_strcmp2(&attr[i]->name, attr_to_remove[j]) == 0) {
				if (!found_attr) found_attr = attr[i];
				pj_array_erase(attr, sizeof(attr[0]), *cnt, i);
				--(*cnt);
				--i;
				break;
			}
		}
	}
	return found_attr;
}

static pjmedia_sdp_session *create_answer(pj_pool_t *pool, const pjmedia_sdp_session *offer)
{
	const char* dir_attrs[] = { "sendrecv", "sendonly", "recvonly", "inactive" };
	const char *ice_attrs[] = { "ice-pwd", "ice-ufrag", "candidate" };
	pjmedia_sdp_session *answer = pjmedia_sdp_session_clone(pool, offer);
	pjmedia_sdp_attr *sess_dir_attr = NULL;
	unsigned mi;

	PJ_LOG(3, (TAG, "Call %d: creating answer"));

	answer->name = pj_str("sipecho");
	sess_dir_attr = find_remove_sdp_attrs(&answer->attr_count, answer->attr, PJ_ARRAY_SIZE(dir_attrs), dir_attrs);

	for (mi = 0; mi<answer->media_count; ++mi) {
		pjmedia_sdp_media *m = answer->media[mi];
		pjmedia_sdp_attr *m_dir_attr;
		pjmedia_sdp_attr *dir_attr;
		const char *our_dir = NULL;
		pjmedia_sdp_conn *c;

		/* Match direction */
		m_dir_attr = find_remove_sdp_attrs(&m->attr_count, m->attr,
			PJ_ARRAY_SIZE(dir_attrs),
			dir_attrs);
		dir_attr = m_dir_attr ? m_dir_attr : sess_dir_attr;

		if (dir_attr) {
			if (pj_strcmp2(&dir_attr->name, "sendonly") == 0)
				our_dir = "recvonly";
			else if (pj_strcmp2(&dir_attr->name, "inactive") == 0)
				our_dir = "inactive";
			else if (pj_strcmp2(&dir_attr->name, "recvonly") == 0)
				our_dir = "inactive";

			if (our_dir) {
				dir_attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
				dir_attr->name = pj_str((char*)our_dir);
				m->attr[m->attr_count++] = dir_attr;
			}
		}

		/* Remove ICE attributes */
		find_remove_sdp_attrs(&m->attr_count, m->attr, PJ_ARRAY_SIZE(ice_attrs), ice_attrs);

		/* Done */
		c = m->conn ? m->conn : answer->conn;
		PJ_LOG(3, (TAG, "  Media %d, %.*s: %s <--> %.*s:%d",
			mi, (int)m->desc.media.slen, m->desc.media.ptr,
			(our_dir ? our_dir : "sendrecv"),
			(int)c->addr.slen, c->addr.ptr, m->desc.port));
	}

	return answer;
}

void call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
	call_t *call = (call_t*)inv->mod_data[mod_sipanswer.id];
	if (!call)
		return;
	PJ_UNUSED_ARG(e);
	if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
		PJ_LOG(3, (TAG, "Call DISCONNECTED [reason=%d (%s)]", inv->cause, pjsip_get_status_text(inv->cause)->ptr));
		destroy_call(*call);
	}
	else {
		PJ_LOG(3, (TAG, "Call: state changed to %s", pjsip_inv_state_name(inv->state)));
	}
}

void call_on_rx_offer(pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{
	call_t *call = (call_t*)inv->mod_data[mod_sipanswer.id];
	pjsip_inv_set_sdp_answer(inv, create_answer(inv->pool_prov, offer));
}

void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
	PJ_UNUSED_ARG(inv);
	PJ_UNUSED_ARG(e);
}

app_t appAnswer;

static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
	app_t *app = &appAnswer;
	pj_sockaddr hostaddr;
	char temp[80], hostip[PJ_INET6_ADDRSTRLEN];
	pj_str_t local_uri;
	pjsip_dialog *dlg;
	pjsip_rdata_sdp_info *sdp_info;
	pjmedia_sdp_session *answer = NULL;
	pjsip_tx_data *tdata = NULL;
	pj_status_t status;

	PJ_LOG(3, (TAG, "RX %.*s from %s",
		(int)rdata->msg_info.msg->line.req.method.name.slen,
		rdata->msg_info.msg->line.req.method.name.ptr,
		rdata->pkt_info.src_name));

	if (rdata->msg_info.msg->line.req.method.id == PJSIP_REGISTER_METHOD) {
		/* Let me be a registrar! */
		pjsip_hdr hdr_list, *h;
		pjsip_msg *msg;
		int expires = -1;

		pj_list_init(&hdr_list);
		msg = rdata->msg_info.msg;
		h = (pjsip_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);
		if (h) {
			expires = ((pjsip_expires_hdr*)h)->ivalue;
			pj_list_push_back(&hdr_list, pjsip_hdr_clone(rdata->tp_info.pool, h));
			PJ_LOG(3, (TAG, " Expires=%d", expires));
		}
		if (expires != 0) {
			h = (pjsip_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
			if (h)
				pj_list_push_back(&hdr_list, pjsip_hdr_clone(rdata->tp_info.pool, h));
		}

		pjsip_endpt_respond(app->sip_endpt, &mod_sipanswer, rdata, 200, NULL,
			&hdr_list, NULL, NULL);
		return PJ_TRUE;
	}

	if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
		if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
			pj_str_t reason = pj_str("Go away");
			pjsip_endpt_respond_stateless(app->sip_endpt, rdata, 400, &reason, NULL, NULL);
		}
		return PJ_TRUE;
	}

	sdp_info = pjsip_rdata_get_sdp_info(rdata);
	if (!sdp_info || !sdp_info->sdp) {
		pj_str_t reason = pj_str("Require valid offer");
		pjsip_endpt_respond_stateless(app->sip_endpt, rdata, 400, &reason, NULL, NULL);
	}

	/* Generate Contact URI */
	status = pj_gethostip(app->sip_af, &hostaddr);
	if (status != PJ_SUCCESS) {
		fprintf(stderr, "%s Unable to retrieve local host IP, error %d\n", TAG, status);
		return PJ_TRUE;
	}
	pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);
	pj_ansi_sprintf(temp, "<sip:sipecho@%s:%d>", hostip, app->sip_port);
	local_uri = pj_str(temp);

	status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata,
		&local_uri, &dlg);

	
	call_t c;
	c.app = app;
	app->calls.push_back(c);	// call_t()
	pjsip_inv_session *inv = c.inv; // app.calls.end()-1->inv;

	if (status == PJ_SUCCESS)
		answer = create_answer(dlg->pool, sdp_info->sdp);
	if (status == PJ_SUCCESS)
		status = pjsip_inv_create_uas(dlg, rdata, answer, 0, &inv);
	if (status == PJ_SUCCESS)
		status = pjsip_inv_initial_answer(inv, rdata, 100,
		NULL, NULL, &tdata);
	if (status == PJ_SUCCESS)
		status = pjsip_inv_send_msg(inv, tdata);

	if (status == PJ_SUCCESS)
		status = pjsip_inv_answer(inv, 180, NULL,
		NULL, &tdata);
	if (status == PJ_SUCCESS)
		status = pjsip_inv_send_msg(inv, tdata);

	if (status == PJ_SUCCESS)
		status = pjsip_inv_answer(inv, 200, NULL,
		NULL, &tdata);
	if (status == PJ_SUCCESS)
		status = pjsip_inv_send_msg(inv, tdata);

	if (status != PJ_SUCCESS) {
		pjsip_endpt_respond_stateless(app->sip_endpt, rdata,
			500, NULL, NULL, NULL);
		destroy_call(*app->calls.end());
	}
	else {
		inv->mod_data[mod_sipanswer.id] = &app->calls.rbegin()->inv;	// &*app.calls.end();
	}

	return PJ_TRUE;
}

void call_on_media_update(pjsip_inv_session *inv, pj_status_t status)
{
	PJ_UNUSED_ARG(inv);
	PJ_UNUSED_ARG(status);
}

// pj_str(argv[pj_optind]);
int makeCall(app_t &app, pj_str_t dst_uri)
{
	pj_sockaddr hostaddr;
	char hostip[PJ_INET6_ADDRSTRLEN + 2];
	char temp[80];
	call_t *call;
	pj_str_t local_uri;
	pjsip_dialog *dlg;
	pj_status_t status;
	pjsip_tx_data *tdata;

	if (pj_gethostip(app.sip_af, &hostaddr) != PJ_SUCCESS) {
		PJ_LOG(1, (TAG, "Unable to retrieve local host IP"));
		return 1;
	}
	pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);

	pj_ansi_sprintf(temp, "<sip:sipecho@%s:%d>", hostip, app.sip_port);
	local_uri = pj_str(temp);
	
	app.calls.push_back(call_t());
	call = &*app.calls.end();
	call->inv = NULL;

	status = pjsip_dlg_create_uac(pjsip_ua_instance(),
		&local_uri,  /* local URI */
		&local_uri,  /* local Contact */
		&dst_uri,    /* remote URI */
		&dst_uri,    /* remote target */
		&dlg);	    /* dialog */
	if (status != PJ_SUCCESS) {
		// Unable to create UAC dialog
		return 1;
	}

	status = pjsip_inv_create_uac(dlg, NULL, 0, &call->inv);
	if (status != PJ_SUCCESS)
		return 2;

	call->inv->mod_data[mod_sipanswer.id] = &call;
	status = pjsip_inv_invite(call->inv, &tdata);
	if (status != PJ_SUCCESS)
		return 3;
	status = pjsip_inv_send_msg(call->inv, tdata);
	if (status != PJ_SUCCESS)
		return 4;
	return 0;
}

/* Register to SIP server by creating SIP account. */

pj_status_t doRegister(const std::string &user, const std::string &domain, const std::string &password)
{
	pjsua_acc_config cfg;
	pjsua_acc_id acc_id;

	pjsua_acc_config_default(&cfg);
	std::string sid("sip:" + user + "@" + domain);
	cfg.id = pj_str((char *) sid.c_str());
	std::string sdom("sip:" + domain);
	cfg.reg_uri = pj_str((char *)sdom.c_str());
	cfg.cred_count = 1;
	cfg.cred_info[0].realm = pj_str((char *)domain.c_str());
	cfg.cred_info[0].scheme = pj_str("digest");
	cfg.cred_info[0].username = pj_str((char *)user.c_str());
	cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cfg.cred_info[0].data = pj_str((char *)password.c_str());

	pj_status_t status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
	return status;
}

int process2()
{
	app_t *app = &appAnswer;

	pj_log_set_level(5);
	pj_init();
	app->sip_af = pj_AF_INET(); // pj_AF_INET6();
	app->sip_port = 5060;
	app->sip_tcp = PJ_FALSE;
	app->enable_msg_logging = PJ_TRUE;

	int r = initStack(app);
	if (r != 0)
		return r;
	
	std::string user("103");
	std::string domain("192.168.43.113");
	std::string password("password");
	pj_status_t status;
	if ((status = doRegister(user, domain, password)) != PJ_SUCCESS)
	{
		fprintf(stderr, "Error %d: %s\n", status, getErrorString(status).c_str());
		doneStack(app);
		return 1;
	}

	char s[10];
	printf("\nq    Quit\n");

	for (;;) {
		if (fgets(s, sizeof(s), stdin) == NULL)
			continue;
		if (s[0] == 'q')
			break;
	}

	doneStack(app);
	return 0;
}

