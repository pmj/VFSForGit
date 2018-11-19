#include <kern/debug.h>
#include <mach/mach_types.h>

#include "KextLog.hpp"
#include "KauthHandler.hpp"
#include "Locks.hpp"
#include "Memory.hpp"
#include "ProfileSample.hpp"
#include "VNodeCache.hpp"
#include "ProfileSample.hpp"

extern "C" kern_return_t PrjFSKext_Start(kmod_info_t* ki, void* d);
extern "C" kern_return_t PrjFSKext_Stop(kmod_info_t* ki, void* d);

dj_profile_probe profile_probes[Probe_Count];


kern_return_t PrjFSKext_Start(kmod_info_t* ki, void* d)
{
    for (size_t i = 0; i < Probe_Count; ++i)
    {
        profile_probes[i].min_ns = UINT64_MAX;
    }
    
    if (Locks_Init())
    {
        goto CleanupAndFail;
    }
    
    if (Memory_Init())
    {
        goto CleanupAndFail;
    }
    
    if (!KextLog_Init())
    {
        goto CleanupAndFail;
    }

    if (!VNodeCache_Init())
    {
        goto CleanupAndFail;
    }
    
    if (KauthHandler_Init())
    {
        goto CleanupAndFail;
    }
    
    KextLog_Info("PrjFSKext (Start)");
    return KERN_SUCCESS;
    
CleanupAndFail:
    KextLog_Error("PrjFSKext failed to start");
    
    PrjFSKext_Stop(nullptr, nullptr);
    return KERN_FAILURE;
}

kern_return_t PrjFSKext_Stop(kmod_info_t* ki, void* d)
{
    kern_return_t result = KERN_SUCCESS;
    
    if (KauthHandler_Cleanup())
    {
        result = KERN_FAILURE;
    }

    VNodeCache_Cleanup();

    if (Memory_Cleanup())
    {
        result = KERN_FAILURE;
    }
    
    KextLog_Info("PrjFSKext (Stop)");

    KextLog_Cleanup();
    
    if (Locks_Cleanup())
    {
        result = KERN_FAILURE;
    }
    
    return result;
}
