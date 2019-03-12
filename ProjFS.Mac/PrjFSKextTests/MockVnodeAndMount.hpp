#pragma once

#include "../PrjFSKext/kernel-header-wrappers/vnode.h"
#include "../PrjFSKext/kernel-header-wrappers/mount.h"
#include "../PrjFSKext/public/FsidInode.h"
#include <memory>
#include <string>

// The struct names mount and vnode are dictated by mount_t and vnode_t's
// definitions as (opaque/forward declared) pointers to those structs.
// As the (testable) kext treats them as entirely opaque, we can implement
// them as we wish for purposes of testing.

struct mount
{
private:
    vfsstatfs statfs;
    uint64_t nextInode;
    
public:
    static std::shared_ptr<mount> Create(const char* fileSystemTypeName = "hfs", fsid_t fsid = fsid_t{}, uint64_t initialInode = 0);

    fsid_t GetFsid() const { return this->statfs.f_fsid; }
    
    friend struct vnode;
    friend vfsstatfs* vfs_statfs(mount_t mountPoint);
};

struct VnodeMockErrors
{
    errno_t getpath = 0;
    errno_t getattr = 0;
};

struct VnodeMockValues
{
    int getattr = 0;
};

struct vnode
{
private:
    std::weak_ptr<vnode> weakSelfPointer;
    std::shared_ptr<mount> mountPoint;
    
    bool isRecycling = false;
    vtype type = VREG;
    uint64_t inode;
    uint32_t vid;
    int32_t ioCount = 0;
    
    std::string path;
    const char* name;
    
    void SetPath(const std::string& path);

    explicit vnode(const std::shared_ptr<mount>& mount);

    vnode(const vnode&) = delete;
    vnode& operator=(const vnode&) = delete;
    
public:
    static std::shared_ptr<vnode> Create(const std::shared_ptr<mount>& mount, const char* path, vtype vnodeType = VREG);
    static std::shared_ptr<vnode> Create(const std::shared_ptr<mount>& mount, const char* path, vtype vnodeType, uint64_t inode);
    ~vnode();

    VnodeMockErrors errors;
    VnodeMockValues values;
    
    uint64_t GetInode() const          { return this->inode; }
    uint32_t GetVid() const            { return this->vid; }
    const char* GetName() const        { return this->name; }
    mount_t GetMountPoint() const      { return this->mountPoint.get(); }
    bool IsRecycling() const           { return this->isRecycling; }
    vtype GetVnodeType() const         { return this->type; }

    void StartRecycling();

    errno_t RetainIOCount();
    void ReleaseIOCount();

    friend int vnode_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx);
    friend int vn_getpath(vnode_t vnode, char* pathBuffer, int* pathLengthInOut);
};


void MockVnodes_CheckAndClear();

