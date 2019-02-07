#ifndef KauthHandler_h
#define KauthHandler_h

#include "public/Message.h"
#include "VirtualizationRoots.hpp"

kern_return_t KauthHandler_Init();
kern_return_t KauthHandler_Cleanup();

void KauthHandler_HandleKernelMessageResponse(VirtualizationRootHandle providerVirtualizationRootHandle, uint64_t messageId, MessageType responseType, const void* resultData, size_t resultDataSize);
void KauthHandler_AbortOutstandingEventsForProvider(VirtualizationRootHandle providerVirtualizationRootHandle);

#endif /* KauthHandler_h */
