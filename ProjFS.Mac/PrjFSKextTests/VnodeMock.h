#ifndef VnodeMock_h
#define VnodeMock_h

struct vnode {
    bool isrecycled;
    int attr;
    vtype vnodeType;
};

int vnode_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx);
int vnode_isrecycled(vnode_t vp);
enum vtype vnode_vtype(vnode_t vp);

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
