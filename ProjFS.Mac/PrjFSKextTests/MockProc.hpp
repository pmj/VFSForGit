#include "../PrjFSKext/kernel-header-wrappers/vnode.h"

enum
{
    KAUTH_RESULT_ALLOW = 1,
    KAUTH_RESULT_DENY,
    KAUTH_RESULT_DEFER
};

extern "C" int proc_pid(proc_t);
extern "C" void proc_name(int pid, char * buf, int size);
extern "C" proc_t vfs_context_proc(vfs_context_t ctx);

void SetProcName(const char* procName);
