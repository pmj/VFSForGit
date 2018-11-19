#include "../PrjFSUser.hpp"
#include "../../PrjFSKext/public/PrjFSLogClientShared.h"
#include "../../Lib/kextgizmos/profiling.h"
#include <iostream>
#include <dispatch/queue.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach_time.h>


static const char* KextLogLevelAsString(KextLog_Level level);
static dispatch_source_t start_kext_profiling_data_polling(io_connect_t connection);
static bool fetch_kext_profiling_data(io_connect_t connection);

static mach_timebase_info_data_t s_machTimebase;

#define PCNAME(SUFFIX) [Probe_ ## SUFFIX] = #SUFFIX
static const char* const PerfCounterNames[Probe_Count] =
{
    PCNAME(VnodeOp),
    PCNAME(VnodeOp_EarlyOut),
    PCNAME(VnodeOp_NoVirtualizationRootFlag),
    PCNAME(VnodeOp_EmptyFlag),
    PCNAME(VnodeOp_DenyCrawler),
    PCNAME(VnodeOp_Offline),
    PCNAME(VnodeOp_Provider),
    PCNAME(VnodeOp_PopulatePlaceholderDirectory),
    PCNAME(VnodeOp_HydratePlaceholderFile),

    PCNAME(VnodeOpSplit_EarlyChecks),
    PCNAME(VnodeOpSplit_ReadFileFlagsSplit),
    PCNAME(VnodeOpSplit_NoFlagCacheLookup),
    PCNAME(VnodeOpSplit_FindRootCached),
    PCNAME(VnodeOpSplit_FindRootStandard),
    
    PCNAME(VirtualizationRoot_FindIteration),
            
    PCNAME(VnodeCache_FindRoot),
    PCNAME(VnodeCache_FindRootFastpathOnly),
    PCNAME(VnodeCacheSplit_FindRootFailedFastpath),
    PCNAME(VnodeCacheSplit_FindRootTreeWalk),
    PCNAME(VnodeCacheSplit_FindRootCheckDirsForRoot),
    PCNAME(VnodeCacheSplit_FindRootUpdateCache),
    PCNAME(VnodeCacheSplit_FindRootEnd),
    PCNAME(VnodeCacheSplit_FindRootEndResizeFailed),
};

int main(int argc, const char * argv[])
{
    mach_timebase_info(&s_machTimebase);

    io_connect_t connection = PrjFSService_ConnectToDriver(UserClientType_Log);
    if (connection == IO_OBJECT_NULL)
    {
        std::cerr << "Failed to connect to kernel service.\n";
        return 1;
    }
    
    DataQueueResources dataQueue = {};
    if (!PrjFSService_DataQueueInit(&dataQueue, connection, LogPortType_MessageQueue, LogMemoryType_MessageQueue, dispatch_get_main_queue()))
    {
        std::cerr << "Failed to set up shared data queue.\n";
        return 1;
    }

    dispatch_source_set_event_handler(dataQueue.dispatchSource, ^{
        struct {
            mach_msg_header_t	msgHdr;
            mach_msg_trailer_t	trailer;
        } msg;
        mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), dataQueue.notificationPort, 0, MACH_PORT_NULL);
        
        while(true)
        {
            IODataQueueEntry* entry = IODataQueuePeek(dataQueue.queueMemory);
            if(entry == nullptr)
            {
                break;
            }
            int messageSize = entry->size;
            if (messageSize >= sizeof(KextLog_MessageHeader) + 2)
            {
                struct KextLog_MessageHeader message = {};
                memcpy(&message, entry->data, sizeof(KextLog_MessageHeader));
                const char* messageType = KextLogLevelAsString(message.level);
                int logStringLength = messageSize - sizeof(KextLog_MessageHeader) - 1;
                printf("%s: %.*s\n", messageType, logStringLength, entry->data + sizeof(KextLog_MessageHeader));
            }
            IODataQueueDequeue(dataQueue.queueMemory, nullptr, nullptr);
        }
    });
    dispatch_resume(dataQueue.dispatchSource);

    dispatch_source_t timer = nullptr;
    if (fetch_kext_profiling_data(connection))
    {
        timer = start_kext_profiling_data_polling(connection);
    }

    CFRunLoopRun();
    
    if (nullptr != timer)
    {
        dispatch_cancel(timer);
        dispatch_release(timer);
    }

    return 0;
}

static const char* KextLogLevelAsString(KextLog_Level level)
{
    switch (level)
    {
    case KEXTLOG_ERROR:
        return "Error";
    case KEXTLOG_INFO:
        return "Info";
    case KEXTLOG_NOTE:
        return "Note";
    default:
        return "Unknown";
    }
}

static uint64_t nanosecondsFromAbsoluteTime(uint64_t machAbsoluteTime)
{
    return static_cast<__uint128_t>(machAbsoluteTime) * s_machTimebase.numer / s_machTimebase.denom;
}

static bool fetch_kext_profiling_data(io_connect_t connection)
{
    uint64_t num_probes = 0;
    uint32_t num_outputs = 1;
    dj_profile_probe probes[Probe_Count];
    size_t out_size = sizeof(probes);
    IOReturn ret = IOConnectCallMethod(connection, LogSelector_FetchProfilingData, nullptr, 0, nullptr, 0, &num_probes, &num_outputs, probes, &out_size);
    if (ret == kIOReturnUnsupported)
    {
        return false;
    }
    else if (ret == kIOReturnSuccess)
    {
        if (out_size / sizeof(probes[0]) < num_probes)
        {
            printf("More probes than expected (%llu), update client code.\n", num_probes);
            num_probes = out_size / sizeof(probes[0]);
        }
        for (unsigned i = 0; i < num_probes; ++i)
        {
            double samples = probes[i].num_samples_1;
            double sum_abs = probes[i].sum_ns;
            double stddev_abs = samples > 1 ? sqrt((samples * probes[i].sum_sq_ns - sum_abs * sum_abs) / (samples * (samples - 1))) : 0.0;

            double sum_ns = nanosecondsFromAbsoluteTime(sum_abs);
            double stddev_ns = nanosecondsFromAbsoluteTime(stddev_abs);
            double mean_ns = samples > 0 ? sum_ns / samples : 0;
            printf("%2u %40s  %8llu [%8llu] samples, total time: %15.0f ns, mean: %10.2f ns +/- %11.2f",
                i, PerfCounterNames[i], probes[i].num_samples_1, probes[i].num_samples_2, sum_ns, mean_ns, stddev_ns);
            if (probes[i].min_ns != UINT64_MAX)
            {
                printf(", min: %7llu ns, max: %10llu ns\n",  nanosecondsFromAbsoluteTime(probes[i].min_ns), nanosecondsFromAbsoluteTime(probes[i].max_ns));
            }
            else
            {
                printf("\n");
            }
        }
    }
    else
    {
        fprintf(stderr, "fetching profiling data from kernel failed: 0x%x\n", ret);
        return false;
    }
    fflush(stdout);
    return true;
}

static dispatch_source_t start_kext_profiling_data_polling(io_connect_t connection)
{
	dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
	dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC, 10 * NSEC_PER_SEC);
	dispatch_source_set_event_handler(timer, ^{
        fetch_kext_profiling_data(connection);
    });
    dispatch_resume(timer);
    return timer;
}

