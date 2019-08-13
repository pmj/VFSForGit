#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../PrjFSKext/public/PrjFSCommon.h"
#include <unistd.h>
#include <getopt.h>

static CFTypeRef GenerateEventTracingSettings(int argc, char* argv[])
{
    CFStringRef pathFilterString = NULL;
    int tracingEnabled = 0, tracingDisabled = 0;

    int ch;

    /* options descriptor */
    static struct option longopts[] = {
         { "enable",              no_argument,            &tracingEnabled,  1 },
         { "enable",              no_argument,            &tracingDisabled, 1 },
         { "pathfilter",          required_argument,      NULL,            'p' },
         { "vnode-action-filter", required_argument,      NULL,            'a' },
         { NULL,         0,                      NULL,           0 }
    };

    while ((ch = getopt_long(argc, argv, "bf:", longopts, NULL)) != -1)
    {
        switch (ch)
        {
        case 'a':
                
            break;
        case 'p':
            if (pathFilterString != NULL)
            {
                fprintf(stderr, "Currently only one path filter is supported\n");
                goto CleanupAndFail;
            }
            
            pathFilterString = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
            
            break;
         case 0:
                 if (daggerset) {
                         fprintf(stderr,"Buffy will use her dagger to "
                             "apply fluoride to dracula's teeth\n");
                 }
                 break;
         default:
                 usage();
         }
    }
    argc -= optind;
    argv += optind;
    
CleanupAndFail:
    if (pathFilterString != NULL)
    {
        CFRelease(pathFilterString);
    }
    
    return NULL;
}

int main(int argc, char* argv[])
{
    CFTypeRef traceSettings = GenerateEventTracingSettings(argc, argv);
    if (traceSettings == NULL)
    {
        fprintf(stderr, "Failed to generate tracing settings\n");
        return 1;
    }

    CFDictionaryRef matchDict = IOServiceMatching(PrjFSServiceClass);
    io_service_t prjfsService = IOServiceGetMatchingService(kIOMasterPortDefault, matchDict); // matchDict consumed
    
    if (prjfsService == IO_OBJECT_NULL)
    {
        fprintf(stderr, "PrjFS Service object not found.\n");
        return 1;
    }
    
    IORegistryEntrySetCFProperty(prjfsService, CFSTR(PrjFSEventTracingKey), traceSettings);
    
    IOObjectRelease(prjfsService);
    return 0;
}
