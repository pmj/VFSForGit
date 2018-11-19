#pragma once

#include <sys/kernel_types.h>

enum VnodeCacheFileState : uint8_t
{
    VnodeCacheFileState_Invalid = 0,

    VnodeCacheFileState_Empty = 1,
    VnodeCacheFileState_Hydrated,      // file has received its contents from provider
    VnodeCacheFileState_Modified,      // caught a write access to a hydrated file
    VnodeCacheFileState_Transitioning, // waiting for completion of the transition from one of the states to another.
    VnodeCacheFileState_Unknown,
};


struct rootNodes
{
    vnode_t vnode;
    int16_t providerIndex;
};

struct VNodeCacheEntry
{
    vnode_t  vnode;
    uint32_t vnid;
    // -1: not in a root
    // -2: virtualization root membership unknown
    int16_t  rootIndex;
    VnodeCacheFileState state;
    // Currently unused
    uint8_t flags;
    
    // TODO: add list of child vnode cache entries so we can invalidate subtrees when they are moved
};
static_assert(sizeof(VNodeCacheEntry) == 16, "Vnode cache entries should be as small as possible");

int16_t VNodeCache_FindVnodeVirtualizationRoot(const vnode_t vnode, vnode_t parentVnode, int16_t expectedRoot);
bool VNodeCache_Init();
void VNodeCache_Cleanup();
void VNodeCache_ResetAndRegisterRootVnodes(struct rootNodes* rn, int numRootNodes);
void VNodeCache_DeregisterVirtualizationRootForProvider(int16_t providerIndex);
void VNodeCache_SetRootIndexForVnode_Locked(const vnode_t vnode, uint32_t vid, int16_t rootIndex);
void VNodeCache_LockExclusive();
void VNodeCache_UnlockExclusive();
