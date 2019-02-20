#ifndef VnodeMock_h
#define VnodeMock_h


#ifdef KEXT_UNIT_TESTING_TESTABLE_KEXT_TARGET
// Building the kext code itself for testing
#include <sys/vnode.h>

#else
// Building the test case target; pull in kernel vnode.h, not /usr/include version which is missing some bits

// Dummy declarations to make the kernel vnode.h below compile in user space (Note: these are
typedef struct ipc_port* ipc_port_t;
enum uio_seg {};
enum uio_rw {};

#include <Kernel/sys/vnode.h>
#endif


// Mock declarations should be visible in both testable kext target and test case target
extern "C"
{
    int vnode_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx);
    int vnode_isrecycled(vnode_t vp);
    enum vtype vnode_vtype(vnode_t vp);
}


// No mock definitions when building the kext itself for testing
#ifndef KEXT_UNIT_TESTING_TESTABLE_KEXT_TARGET


struct vnode {
    bool isrecycled;
    int attr;
    vtype vnodeType;
};


// TODO: put function bodies in a .cpp

int vnode_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx)
{
    vap->va_vaflags = vp->attr;
    return 0;
}

int vnode_isrecycled(vnode_t vp)
{
    return vp->isrecycled;
}

enum vtype vnode_vtype(vnode_t vp)
{
    return vp->vnodeType;
}

#endif

#endif
