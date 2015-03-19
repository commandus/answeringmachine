#include "logging.h"

std::string getErrorString(pj_status_t errcode)
{
	char errbuf[PJ_ERR_MSG_SIZE];
	pj_strerror(errcode, errbuf, sizeof(errbuf));
	return std::string((char*)&errbuf);
}


// Util to display the error message for the specified error code
int app_perror(const char *sender, const char *title, pj_status_t status)
{
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(3, (sender, "%s: %s [code=%d]", title, errmsg, status));
	return 1;
}

/* Notification on incoming messages */
pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
	PJ_LOG(4, (TAG, "RX %d bytes %s from %s %s:%d:\n"
		"%.*s\n"
		"--end msg--",
		rdata->msg_info.len,
		pjsip_rx_data_get_info(rdata),
		rdata->tp_info.transport->type_name,
		rdata->pkt_info.src_name,
		rdata->pkt_info.src_port,
		(int)rdata->msg_info.len,
		rdata->msg_info.msg_buf));

	/* Always return false, otherwise messages will not get processed! */
	return PJ_FALSE;
}

/* Notification on outgoing messages */
pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{

	/* Important note:
	*	tp_info field is only valid after outgoing messages has passed
	*	transport layer. So don't try to access tp_info when the module
	*	has lower priority than transport layer.
	*/

	PJ_LOG(4, (TAG, "TX %d bytes %s to %s %s:%d:\n"
		"%.*s\n"
		"--end msg--",
		(tdata->buf.cur - tdata->buf.start),
		pjsip_tx_data_get_info(tdata),
		tdata->tp_info.transport->type_name,
		tdata->tp_info.dst_name,
		tdata->tp_info.dst_port,
		(int)(tdata->buf.cur - tdata->buf.start),
		tdata->buf.start));

	/* Always return success, otherwise message will not get sent! */
	return PJ_SUCCESS;
}
