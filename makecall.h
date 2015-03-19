#ifndef MAKECALL_H
#define MAKECALL_H	1

#include <pjsip.h>
#include <pjlib.h>
#include <pjmedia.h>
#include <pjsip_ua.h>

#include "ClientContext.h"

int makeCall(ClientContext *ctx, const std::string &fromuri, const std::string &desturi);

#endif