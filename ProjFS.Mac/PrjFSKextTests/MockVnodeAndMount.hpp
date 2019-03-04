#pragma once

#include "../PrjFSKext/kernel-header-wrappers/vnode.h"
#include "../PrjFSKext/kernel-header-wrappers/mount.h"
#include "../PrjFSKext/public/FsidInode.h"
#include <memory>
#include <string>

typedef std::shared_ptr<mount> MountPointer;
typedef std::shared_ptr<vnode> VnodePointer;
typedef std::weak_ptr<vnode> VnodeWeakPointer;

struct mount
{
private:
    vfsstatfs statfs;
    uint64_t nextInode;
    
public:
    static MountPointer Create(const char* fileSystemTypeName = "hfs", fsid_t fsid = fsid_t{}, uint64_t initialInode = 0);
    
    inline fsid_t GetFsid() const { return this->statfs.f_fsid; }
    
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
    VnodeWeakPointer weakSelfPointer;
    MountPointer mountPoint;
    
public:
    bool isRecycling = false;
    vtype type = VREG;
    VnodeMockErrors errors;
    VnodeMockValues values;

private:
    uint64_t inode;
    uint32_t vid;
    int32_t ioCount = 0;
    
    std::string path;
    const char* name;
    
    void SetPath(const std::string& path);

    explicit vnode(const MountPointer& mount);
    
    vnode(const vnode&) = delete;
    vnode& operator=(const vnode&) = delete;
    
public:
    static VnodePointer Create(const MountPointer& mount, const char* path, vtype vnodeType = VREG);
    static VnodePointer Create(const MountPointer& mount, const char* path, vtype vnodeType, uint64_t inode);
    ~vnode();
    
    uint64_t GetInode() const          { return this->inode; }
    uint32_t GetVid() const            { return this->vid; }
    const char* GetName() const        { return this->name; }
    mount_t GetMountPoint() const      { return this->mountPoint.get(); }

    void StartRecycling();

    errno_t RetainIOCount();
    void ReleaseIOCount();

    friend int vnode_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx);
    friend int vn_getpath(vnode_t vnode, char* pathBuffer, int* pathLengthInOut);
};


void MockVnodes_CheckAndClear();

