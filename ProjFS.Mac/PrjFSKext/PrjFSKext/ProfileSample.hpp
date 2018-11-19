#pragma once

#include "../../Lib/kextgizmos/profiling.h"
#include "PrjFSCommon.h"

#include <mach/mach_time.h>

#ifdef DJ_PROFILE_ENABLE
extern dj_profile_probe profile_probes[Probe_Count];
#endif

class ProfileSample
{
    ProfileSample(const ProfileSample&) = delete;
    ProfileSample() = delete;
    
#ifdef DJ_PROFILE_ENABLE
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


#ifdef DJ_PROFILE_ENABLE

ProfileSample::~ProfileSample()
{
    uint64_t endTimestamp = mach_absolute_time();
    if (this->wholeSampleProbe != Probe_None)
    {
        dj_profile_sample(&profile_probes[this->wholeSampleProbe], this->startTimestamp, endTimestamp);
    }
    
    if (this->finalSplitProbe != Probe_None)
    {
        dj_profile_sample(&profile_probes[this->finalSplitProbe], this->splitTimestamp, endTimestamp);
    }
};

void ProfileSample::takeSplitSample(PrjFS_PerfCounter splitProbe)
{
    uint64_t newSplitTimestamp = mach_absolute_time();
    dj_profile_sample(&profile_probes[splitProbe], this->splitTimestamp, newSplitTimestamp);
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

#else

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

#endif
