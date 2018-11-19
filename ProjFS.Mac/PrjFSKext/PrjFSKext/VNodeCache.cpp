#include "VNodeCache.hpp"
#include "../lib/genccont/linear_probing_hash_table.h"
#include "Memory.hpp"
#include "Locks.hpp"
#include "KextLog.hpp"
#include "PrjFSCommon.h"
#include "kernel-header-wrappers/vnode.h"
#include "VirtualizationRoots.hpp"
#include "ProfileSample.hpp"

#include <sys/kernel_types.h>
#include <stdint.h>

static struct genc_linear_probing_hash_table s_vnodeCacheTable;
static RWLock s_vnodeCacheRWLock;
struct TableAllocState
{
    bool safeToBlock;
    bool resizeFailed;
    uint32_t sizeRequested;
    
    void* reservedMemory;
    uint32_t reservedMemorySize;
};
static TableAllocState s_vnodeCacheTableAllocState;

static genc_hash_t CacheEntry_Hash(void* key, void* opaque)
{
    uintptr_t vnodePointerValue = reinterpret_cast<uintptr_t>(key);
    // multiplication witha prime
    return 2305843009213693951lu * vnodePointerValue;
}

static void* CacheEntry_GetKey(void* item, void* opaque)
{
    return static_cast<VNodeCacheEntry*>(item)->vnode;
}
static genc_bool_t CacheEntry_IsEmpty(void* item, void* opaque)
{
    return nullptr == static_cast<VNodeCacheEntry*>(item)->vnode;
}
static void CacheEntry_Clear(void* item, void* opaque)
{
    VNodeCacheEntry* entry = static_cast<VNodeCacheEntry*>(item);
    memset(entry, 0, sizeof(*entry));
}

static size_t RoundupToPow2(size_t num)
{
    if (genc_is_pow2(num))
    {
        return num;
    }
    else
    {
        return 1lu << (1u + (63u - __builtin_clzl(num)));
    }
}

static void* TableRealloc(void* old_ptr, size_t old_size, size_t new_size, void* opaque)
{
    TableAllocState* allocState = static_cast<TableAllocState*>(opaque);
    if (old_ptr == nullptr)
    {
        assert(new_size <= UINT32_MAX);
        assert(allocState->safeToBlock);
        return Memory_Alloc(static_cast<uint32_t>(new_size));
    }
    else if (new_size == 0)
    {
        Memory_Free(old_ptr, static_cast<uint32_t>(old_size));
        return nullptr;
    }
    else
    {
        void* newMemory = nullptr;
        if (allocState->reservedMemorySize == new_size)
        {
            newMemory = allocState->reservedMemory;
            allocState->reservedMemory = nullptr;
            allocState->reservedMemorySize = 0;
        }
        
        if (nullptr == newMemory)
        {
            if (allocState->safeToBlock)
            {
                newMemory = Memory_Alloc(new_size);
            }
            else
            {
                newMemory = Memory_AllocNonBlocking(new_size);
            }
        }
        
        if (nullptr == newMemory)
        {
            allocState->resizeFailed = true;
            allocState->sizeRequested = new_size;
            return nullptr;
        }
        
        memcpy(newMemory, old_ptr, min(new_size, old_size));
        
        Memory_Free(old_ptr, old_size);
        return newMemory;
    }
}



bool VNodeCache_Init()
{
    s_vnodeCacheTableAllocState.safeToBlock = true;
    bool ok = genc_linear_probing_hash_table_init(
        &s_vnodeCacheTable,
        // operation function pointers:
        CacheEntry_Hash,
        CacheEntry_GetKey,
        genc_pointer_keys_equal,
        CacheEntry_IsEmpty,
        CacheEntry_Clear,
        TableRealloc,
        // opaque pointer argument to operations:
        &s_vnodeCacheTableAllocState,
        // entry size
        sizeof(VNodeCacheEntry),
        // initial capacity
        RoundupToPow2(desiredvnodes / 10));
    s_vnodeCacheTableAllocState.safeToBlock = false;
    s_vnodeCacheRWLock = RWLock_Alloc();
    return ok;
}

void VNodeCache_Cleanup()
{
    if (RWLock_IsValid(s_vnodeCacheRWLock))
    {
        RWLock_FreeMemory(&s_vnodeCacheRWLock);
    }
    genc_lpht_destroy(&s_vnodeCacheTable);
}


void VNodeCache_ResetAndRegisterRootVnodes(struct rootNodes* rn, int numRootNodes)
{
    genc_lpht_clear(&s_vnodeCacheTable);
    for(int i = 0; i < numRootNodes; i++)
    {
        uint32_t vid = vnode_vid(rn[i].vnode);
        VNodeCacheEntry entry = {.vnode = rn[i].vnode, .vnid = vid, .rootIndex = rn[i].providerIndex};
        genc_lpht_insert_or_update_item(&s_vnodeCacheTable, &entry);
    }
}

void VNodeCache_DeregisterVirtualizationRootForProvider(int16_t rootIndex)
{
    genc_lpht_for_each_obj(VNodeCacheEntry, foundEntry, &s_vnodeCacheTable)
    {
        if(rootIndex == foundEntry->rootIndex)
        {
            foundEntry->rootIndex = -2;
        }
    }
}

struct vnodeInfo
{
    vnode_t vnode;
    uint32_t vid;
    int16_t rootIndex;
};

void VNodeCache_LockExclusive()
{
    RWLock_AcquireExclusive(s_vnodeCacheRWLock);
}
void VNodeCache_UnlockExclusive()
{
    RWLock_ReleaseExclusive(s_vnodeCacheRWLock);
}

void VNodeCache_SetRootIndexForVnode_Locked(const vnode_t vnode, uint32_t vid, int16_t rootIndex)
{
    VNodeCacheEntry* entry = genc_lpht_find_obj(&s_vnodeCacheTable, vnode, VNodeCacheEntry);
    if (nullptr != entry)
    {
        if (entry->vnid != vid)
        {
            // stale entry, update
            entry->vnid = vid;
            entry->flags = 0;
            entry->state = VnodeCacheFileState_Unknown;
        }
        entry->rootIndex = rootIndex;
    }
    else
    {
        VNodeCacheEntry entry = {.vnode = vnode, .vnid = vid, .rootIndex = rootIndex, .flags = 0, .state = VnodeCacheFileState_Unknown };
        genc_lpht_insert_item(&s_vnodeCacheTable, &entry);
    }
}

int16_t VNodeCache_FindVnodeVirtualizationRoot(const vnode_t vnode, vnode_t parentVnode, int16_t expectedRoot)
{
    ProfileSample functionSample(Probe_VnodeCache_FindRoot);

    bool vnodeIsDir = vnode_isdir(vnode);
    int16_t rootIndex = -2;

    // Fast path: read-only access to the cache, check if vnode or its parent
    // are in the cache with known root membership state, and if so return result.
    RWLock_AcquireShared(s_vnodeCacheRWLock);
    {
        VNodeCacheEntry* entry = genc_lpht_find_obj(&s_vnodeCacheTable, vnode, VNodeCacheEntry);
        if (nullptr != entry && vnode_vid(vnode) == entry->vnid)
        {
            rootIndex = entry->rootIndex;
            if (rootIndex != expectedRoot && rootIndex != -2)
            {
                KextLog_FileNote(vnode, "VNodeCache_FindVnodeVirtualizationRoot: mismatched root in fast-path cache hit: cache says %d, expected %d", rootIndex, expectedRoot);
            }
        }
        else if (!vnodeIsDir)
        {
            bool mustPutParent = false;
            if (nullptr == parentVnode)
            {
                parentVnode = vnode_getparent(vnode);
                mustPutParent = true;
            }
            
            if (nullptr != parentVnode)
            {
                VNodeCacheEntry* parentEntry = genc_lpht_find_obj(&s_vnodeCacheTable, parentVnode, VNodeCacheEntry);
                if (nullptr != parentEntry && vnode_vid(parentVnode) == parentEntry->vnid)
                {
                    if (rootIndex != expectedRoot && rootIndex != -2)
                    {
                        KextLog_FileNote(parentVnode, "VNodeCache_FindVnodeVirtualizationRoot: mismatched root in fast-path parent cache hit: cache says %d, expected %d", rootIndex, expectedRoot);
                    }
                    
                    rootIndex = parentEntry->rootIndex;
                }
                
                if (mustPutParent)
                {
                    vnode_put(parentVnode);
                }
            }
        }
        
        if (nullptr != entry && rootIndex == -1 && !vnodeIsDir)
        {
            functionSample.setProbe(Probe_VnodeCache_FindRootFastpathOnly);
            // stale cache entry for a file not in a root, try to clear it
            if (RWLock_AcquireSharedToExclusive(s_vnodeCacheRWLock))
            {
                genc_lpht_remove(&s_vnodeCacheTable, entry);
                RWLock_ReleaseExclusive(s_vnodeCacheRWLock);
            }
            
            return rootIndex;
        }
    }
    RWLock_ReleaseShared(s_vnodeCacheRWLock);

    if (rootIndex >= -1)
    {
        functionSample.setProbe(Probe_VnodeCache_FindRootFastpathOnly);
        return rootIndex;
    }
    
    functionSample.takeSplitSample(Probe_VnodeCacheSplit_FindRootFailedFastpath);
    
    // Vnode or parent directory are not in cache, take the slow path up the
    // directory hierarchy until we find a cache entry or the root, and back
    // down again to add everything to the cache.
    
    rootIndex = -1;

    vfs_context_t context = vfs_context_create(nullptr);

    // index = 0 leaf node, 1… parent directories
    vnodeInfo vnodeArray[50] = {};
    int32_t vnodeArrayCount = 0;

    RWLock_AcquireShared(s_vnodeCacheRWLock);
    {
        vnode_t vn = vnode;
        vnode_get(vn); // to match put on every iteration
        
        while (true)
        {
            if (NULLVP == vn)
            {
                //hit the root of the file system, therefore not part of any virtualisation root
                rootIndex = -1;
                if (rootIndex != expectedRoot)
                    KextLog_FileNote(vnode, "VNodeCache_FindVnodeVirtualizationRoot: reached FS root without cache hit. Expected virtualization root %d\n", expectedRoot);
                break;
            }
            
            VNodeCacheEntry* vnEntry = genc_lpht_find_obj(&s_vnodeCacheTable, vn, VNodeCacheEntry);
            uint32_t vid = vnode_vid(vn);
            if (nullptr != vnEntry && -2 != vnEntry->rootIndex)
            {
                //found an entry in the cache, check if it is stale
                if(vid == vnEntry->vnid)
                {
                    //not stale, have our result
                    rootIndex = vnEntry->rootIndex;
                    if (rootIndex != expectedRoot)
                        KextLog_FileInfo(vn, "VNodeCache_FindVnodeVirtualizationRoot: Possible mismatched root in slow-path cache hit? Expected virtualization root %d, got %d. Nesting depth: %d, directories along breadcrumb tail could still turn out to be roots.", expectedRoot, rootIndex, vnodeArrayCount);
                    vnode_put(vn);
                    break;
                }
            }
     
            // Keep trace of visited nodes so they can all be cached
            if (vnodeArrayCount < 50)
            {
                vnodeArray[vnodeArrayCount] = {.vnode = vn, .rootIndex = rootIndex, .vid = vid };
                vnodeArrayCount++;

                if (rootIndex >= 0)
                {
                    // found root that needs to be cached
                    KextLog_FileError(vn, "VNodeCache_FindVnodeVirtualizationRoot: rootIndex >= 0 for no discernible reason, this should probably not be possible.");
                    break;
                }

                vn = vnode_getparent(vn);
            }
            else
            {
                if (rootIndex >= 0)
                {
                    // found root
                    KextLog_FileError(vn, "VNodeCache_FindVnodeVirtualizationRoot: rootIndex >= 0 for no discernible reason, this should probably not be possible.");
                    vnode_put(vn);
                    break;
                }

                vnode_t parent = vnode_getparent(vn);
                vnode_put(vn);
                vn = parent;
            }
        }
    }
    RWLock_ReleaseShared(s_vnodeCacheRWLock);

    functionSample.takeSplitSample(Probe_VnodeCacheSplit_FindRootTreeWalk);

    // Perform the I/O without the lock held

    // Check if any of the directories visited are roots which are not yet in the cache

    const int32_t directoryVnodesStartIndex = vnodeIsDir ? 0 : 1;
    
    for (int32_t i = vnodeArrayCount - 1; i >= directoryVnodesStartIndex; --i)
    {
        vnode_t dirVnode = vnodeArray[i].vnode;
        if (dirVnode == nullptr)
        {
            KextLog_FileError(vnode, "vnode at index %d is null!?", i);
            for (unsigned i = 0; i < vnodeArrayCount; ++i)
            {
                KextLog_FileError(vnodeArray[i].vnode, "%2u: ", i);
            }
            return rootIndex;
        }
 
        // Directory may be a virtualisation root which is not in the cache, or
        // an offline one which has not even been detected yet.
        // Note that this may hit disk (xattr read) and will acquire the root lock
        // in shared (fast path) or exclusive (root xattr found) mode.
        int16_t directoryRootIndex = VirtualizationRoots_FindRootWithVnode(dirVnode, context);
        if (directoryRootIndex >= 0)
        {
            rootIndex = directoryRootIndex;
        }
        
        vnodeArray[i].rootIndex = rootIndex;
    }

    functionSample.takeSplitSample(Probe_VnodeCacheSplit_FindRootCheckDirsForRoot);

    uint32_t reallocSize = 0;

    RWLock_AcquireExclusive(s_vnodeCacheRWLock);
    {
        // TODO: check for any operations that may have invalidated the cache while we dropped the lock.
        // In this unlikely case, the whole operation needs to be repeated.
        
        // add/update all visited directory vnodes to the cache, and if below a virtualisation root, also add the file.
        uint32_t i = 1;
        if (vnodeIsDir || rootIndex >= 0)
        {
            i = 0;
        }
        else
        {
            // Remove file's existing entry from cache if there is one, as it's not in a virtualisation root
            VNodeCacheEntry* entry = genc_lpht_find_obj(&s_vnodeCacheTable, vnode, VNodeCacheEntry);
            if (nullptr != entry)
            {
                genc_lpht_remove(&s_vnodeCacheTable, entry);
            }
        }
        
        for(; i < vnodeArrayCount; i++)
        {
            vnode_t vnode = vnodeArray[i].vnode;
            uint32_t vid = vnodeArray[i].vid;
            VNodeCacheEntry* vnodeEntry = genc_lpht_find_obj(&s_vnodeCacheTable, vnode, VNodeCacheEntry);
            // If the vid matches, the cache entry refers to the same file, otherwise the vnode was recycled, so wipe the entry.
            if (nullptr != vnodeEntry && vid == vnodeEntry->vnid)
            {
                vnodeEntry->rootIndex = vnodeArray[i].rootIndex;
            }
            else
            {
                //update all info
                VNodeCacheEntry newEntry = { .vnode = vnode, .vnid = vid, .rootIndex = vnodeArray[i].rootIndex, .flags = 0, .state = VnodeCacheFileState_Unknown };
                if (nullptr != vnodeEntry)
                {
                    *vnodeEntry = newEntry;
                }
                else
                {
                    genc_lpht_insert_obj(&s_vnodeCacheTable, &newEntry, VNodeCacheEntry);
                }
            }
            
            vnode_put(vnode);
        }

        functionSample.takeSplitSample(Probe_VnodeCacheSplit_FindRootUpdateCache, Probe_VnodeCacheSplit_FindRootEnd);

        // During the insertions, was there a (failed) attempt at resizing the hash table?
        if (s_vnodeCacheTableAllocState.resizeFailed)
        {
            functionSample.setFinalSplitProbe(Probe_VnodeCacheSplit_FindRootEndResizeFailed);
            
            s_vnodeCacheTableAllocState.resizeFailed = false;
            reallocSize = s_vnodeCacheTableAllocState.sizeRequested;
            s_vnodeCacheTableAllocState.sizeRequested = 0;
            KextLog_Note("VNodeCache_FindVnodeVirtualizationRoot: Non-blocking resizing table failed, %u bytes requested. table items: %lu/%lu",
                reallocSize, genc_lpht_count(&s_vnodeCacheTable), genc_lpht_capacity(&s_vnodeCacheTable));
        }
    }

    RWLock_ReleaseExclusive(s_vnodeCacheRWLock);
 
    if (reallocSize > 0)
    {
        // Allocate memory for resizing the table without a lock held, then perform the resize locked
        void* tableMemory = Memory_Alloc(reallocSize);
        if (nullptr != tableMemory)
        {
            RWLock_AcquireExclusive(s_vnodeCacheRWLock);
            assert(nullptr == s_vnodeCacheTableAllocState.reservedMemory);
            
            s_vnodeCacheTableAllocState.reservedMemory = tableMemory;
            s_vnodeCacheTableAllocState.reservedMemorySize = reallocSize;
            
            genc_lphtl_reserve_space(&s_vnodeCacheTable.table, &s_vnodeCacheTable.desc, &s_vnodeCacheTableAllocState, s_vnodeCacheTable.table.item_count);
            
            if (nullptr != s_vnodeCacheTableAllocState.reservedMemory)
            {
                Memory_Free(s_vnodeCacheTableAllocState.reservedMemory, s_vnodeCacheTableAllocState.reservedMemorySize);
                s_vnodeCacheTableAllocState.reservedMemory = nullptr;
                s_vnodeCacheTableAllocState.reservedMemorySize = 0;
            }
            else
            {
                KextLog_Note("VNodeCache_FindVnodeVirtualizationRoot: Resizing appears to have succeeded, table items: %lu/%lu",
                    genc_lpht_count(&s_vnodeCacheTable), genc_lpht_capacity(&s_vnodeCacheTable));
            }
            
            RWLock_ReleaseExclusive(s_vnodeCacheRWLock);
        }
    }
    
    vfs_context_rele(context);
    
    return rootIndex;
}
