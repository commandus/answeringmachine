#ifndef LOGGING_H
#define LOGGING_H	1

#include <string>
#include <pjsip.h>
#include <pjlib.h>

#define TAG	"answeringmachine"

// Util to display the error message for the specified error code
std::string getErrorString(pj_status_t errcode);
int app_perror(const char *sender, const char *title, pj_status_t status);

// Notification on incoming messages 
pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata);
// Notification on outgoing messages 
pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata);

#endif