#pragma once
#include <Arduino.h>
#include "web/WebContext.h"
void WebHandlersInit(WebHandlerContext *);
bool WebHandlersIsOtaBusy();
void WebHandlersPollDeferred();
inline String wifi_label_safe(const String &s) { return s; }
