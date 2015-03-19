#ifndef SIMPLEUA_H
#define SIMPLEUA_H	1

#include <vector>
#include <map>

#include "CallSession.h"
#include "ConfigClient.h"

int process(ConfigClient &config);
ClientContext *getClientContext();

#endif