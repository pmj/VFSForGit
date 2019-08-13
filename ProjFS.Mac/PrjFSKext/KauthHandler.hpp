#ifndef KauthHandler_h
#define KauthHandler_h

#include "public/Message.h"
#include "VirtualizationRoots.hpp"

kern_return_t KauthHandler_Init();
kern_return_t KauthHandler_Cleanup();
bool KauthHandler_EnableTraceListeners(bool vnodeTraceEnabled, bool fileopTraceEnabled);

#endif /* KauthHandler_h */
