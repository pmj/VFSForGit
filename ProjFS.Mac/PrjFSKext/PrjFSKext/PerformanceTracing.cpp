#include "PerformanceTracing.hpp"
#include <sys/types.h>

#ifdef PRJFS_PERFORMANCE_TRACING_ENABLE
PerfTracingProbe profile_probes[Probe_Count];

void PerfTracing_Init()
{
    for (size_t i = 0; i < Probe_Count; ++i)
    {
        PerfTracing_ProbeInit(&profile_probes[i]);
    }
}

#endif

IOReturn PerfTracing_ExportDataUserClient(IOExternalMethodArguments* arguments)
{
    return kIOReturnUnsupported;
}

void PerfTracing_ProbeInit(PerfTracingProbe* probe)
{
    *probe = DJ_PROFILE_PROBE_INIT;
}
