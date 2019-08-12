#include <IOKit/IOKitLib.h>
#include <cstdio>
#include <CoreFoundation/CoreFoundation.h>
#include "../PrjFSKext/public/PrjFSCommon.h"

int main(int argc, const char * argv[])
{
    CFDictionaryRef matchDict = IOServiceMatching(PrjFSServiceClass);
    io_service_t prjfsService = IOServiceGetMatchingService(kIOMasterPortDefault, matchDict); // matchDict consumed
    
    if (prjfsService == IO_OBJECT_NULL)
    {
        fprintf(stderr, "PrjFS Service object not found.\n");
        return 1;
    }
    
    IORegistryEntrySetCFProperty(prjfsService, CFSTR(PrjFSEventTracingKey), kCFBooleanTrue);
    
    IOObjectRelease(prjfsService);
    return 0;
}
