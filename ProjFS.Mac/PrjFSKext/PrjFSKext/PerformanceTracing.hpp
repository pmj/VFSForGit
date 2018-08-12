#pragma once

#include "PrjFSCommon.h"

#include <mach/mach_time.h>
#include <IOKit/IOReturn.h>

void PerfTracing_Init();
void PerfTracing_ProbeInit(PerfTracingProbe* probe);
void PerfTracing_RecordSample(PerfTracingProbe* probe, uint64_t startTime, uint64_t endTime);

struct IOExternalMethodArguments;
IOReturn PerfTracing_ExportDataUserClient(IOExternalMethodArguments* arguments);

#ifdef PRJFS_PERFORMANCE_TRACING_ENABLE
extern PerfTracingProbe profile_probes[Probe_Count];
#endif

class ProfileSample
{
    ProfileSample(const ProfileSample&) = delete;
    ProfileSample() = delete;
    
#ifdef PRJFS_PERFORMANCE_TRACING_ENABLE
    const uint64_t startTimestamp;
    PrjFS_PerfCounter wholeSampleProbe;
    PrjFS_PerfCounter finalSplitProbe;
    uint64_t splitTimestamp;
#endif
    
public:
    inline ProfileSample(PrjFS_PerfCounter defaultProbe);
    inline void setProbe(PrjFS_PerfCounter probe);
    inline void takeSplitSample(PrjFS_PerfCounter splitProbe);
    inline void takeSplitSample(PrjFS_PerfCounter splitProbe, PrjFS_PerfCounter finalSplitProbe);
    inline void setFinalSplitProbe(PrjFS_PerfCounter finalSplitProbe);
    inline ~ProfileSample();
};


#ifdef PRJFS_PERFORMANCE_TRACING_ENABLE

// With tracing enabled, actually record timing samples

ProfileSample::~ProfileSample()
{
    uint64_t endTimestamp = mach_absolute_time();
    if (this->wholeSampleProbe != Probe_None)
    {
        PerfTracing_RecordSample(&profile_probes[this->wholeSampleProbe], this->startTimestamp, endTimestamp);
    }
    
    if (this->finalSplitProbe != Probe_None)
    {
        PerfTracing_RecordSample(&profile_probes[this->finalSplitProbe], this->splitTimestamp, endTimestamp);
    }
};

void ProfileSample::takeSplitSample(PrjFS_PerfCounter splitProbe)
{
    uint64_t newSplitTimestamp = mach_absolute_time();
    PerfTracing_RecordSample(&profile_probes[splitProbe], this->splitTimestamp, newSplitTimestamp);
    this->splitTimestamp = newSplitTimestamp;
}

void ProfileSample::takeSplitSample(PrjFS_PerfCounter splitProbe, PrjFS_PerfCounter finalSplitProbe)
{
    this->takeSplitSample(splitProbe);
    this->finalSplitProbe = finalSplitProbe;
}

void ProfileSample::setFinalSplitProbe(PrjFS_PerfCounter finalSplitProbe)
{
    this->finalSplitProbe = finalSplitProbe;
}


ProfileSample::ProfileSample(PrjFS_PerfCounter defaultProbe) :
    startTimestamp(mach_absolute_time()),
    wholeSampleProbe(defaultProbe),
    finalSplitProbe(Probe_None),
    splitTimestamp(this->startTimestamp)
{
}

void ProfileSample::setProbe(PrjFS_PerfCounter probe)
{
    this->wholeSampleProbe = probe;
}

inline void PerfTracing_RecordSample(PerfTracingProbe* probe, uint64_t startTime, uint64_t endTime)
{
    dj_profile_sample(probe, startTime, endTime);
}

#else

// With tracing disabled, try to compile down to nothing:

ProfileSample::ProfileSample(PrjFS_PerfCounter defaultProbe)
{}

void ProfileSample::setProbe(PrjFS_PerfCounter probe)
{}

void ProfileSample::takeSplitSample(PrjFS_PerfCounter splitProbe)
{}

void ProfileSample::takeSplitSample(PrjFS_PerfCounter splitProbe, PrjFS_PerfCounter finalSplitProbe)
{}

void ProfileSample::setFinalSplitProbe(PrjFS_PerfCounter finalSplitProbe)
{}

ProfileSample::~ProfileSample()
{}

inline void PerfTracing_Init()
{}

#endif
