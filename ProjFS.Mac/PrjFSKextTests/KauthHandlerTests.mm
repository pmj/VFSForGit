#include "../PrjFSKext/kernel-header-wrappers/vnode.h"
#include "../PrjFSKext/KauthHandlerTestable.hpp"
#include "../PrjFSKext/PerformanceTracing.hpp"
#import <XCTest/XCTest.h>
#include "MockVnodeAndMount.hpp"
#include "MockProc.hpp"

using std::shared_ptr;

proc_t vfs_context_proc(vfs_context_t ctx)
{
    return NULL;
}

@interface KauthHandlerTests : XCTestCase
@end

@implementation KauthHandlerTests

- (void) tearDown
{
    MockVnodes_CheckAndClear();
}

- (void)testActionBitIsSet {
    XCTAssertTrue(ActionBitIsSet(KAUTH_VNODE_READ_DATA, KAUTH_VNODE_READ_DATA));
    XCTAssertTrue(ActionBitIsSet(KAUTH_VNODE_WRITE_DATA, KAUTH_VNODE_WRITE_DATA));
    XCTAssertTrue(ActionBitIsSet(KAUTH_VNODE_WRITE_DATA, KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA));
    XCTAssertTrue(ActionBitIsSet(KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA, KAUTH_VNODE_WRITE_DATA));
    XCTAssertFalse(ActionBitIsSet(KAUTH_VNODE_WRITE_DATA, KAUTH_VNODE_READ_DATA));
}

- (void)testIsFileSystemCrawler {
    XCTAssertTrue(IsFileSystemCrawler("mds"));
    XCTAssertTrue(IsFileSystemCrawler("mdworker"));
    XCTAssertTrue(IsFileSystemCrawler("mds_stores"));
    XCTAssertTrue(IsFileSystemCrawler("fseventsd"));
    XCTAssertTrue(IsFileSystemCrawler("Spotlight"));
    XCTAssertFalse(IsFileSystemCrawler("mds_"));
    XCTAssertFalse(IsFileSystemCrawler("spotlight"));
    XCTAssertFalse(IsFileSystemCrawler("git"));
}

- (void)testFileFlagsBitIsSet {
    XCTAssertTrue(FileFlagsBitIsSet(FileFlags_IsEmpty, FileFlags_IsEmpty));
    XCTAssertTrue(FileFlagsBitIsSet(FileFlags_IsInVirtualizationRoot, FileFlags_IsInVirtualizationRoot));
    XCTAssertFalse(FileFlagsBitIsSet(FileFlags_IsInVirtualizationRoot, FileFlags_IsEmpty));
    XCTAssertFalse(FileFlagsBitIsSet(FileFlags_IsInVirtualizationRoot, FileFlags_Invalid));
}

- (void)testShouldIgnoreVnodeType {
    shared_ptr<mount> testMount = mount::Create("hfs", fsid_t{}, 0);
    shared_ptr<vnode> testVnode = vnode::Create(testMount, "/foo");
    XCTAssertTrue(ShouldIgnoreVnodeType(VNON, testVnode.get()));
    XCTAssertTrue(ShouldIgnoreVnodeType(VBLK, testVnode.get()));
    XCTAssertTrue(ShouldIgnoreVnodeType(VCHR, testVnode.get()));
    XCTAssertTrue(ShouldIgnoreVnodeType(VSOCK, testVnode.get()));
    XCTAssertTrue(ShouldIgnoreVnodeType(VFIFO, testVnode.get()));
    XCTAssertTrue(ShouldIgnoreVnodeType(VBAD, testVnode.get()));
    XCTAssertFalse(ShouldIgnoreVnodeType(VREG, testVnode.get()));
    XCTAssertFalse(ShouldIgnoreVnodeType(VDIR, testVnode.get()));
    XCTAssertFalse(ShouldIgnoreVnodeType(VLNK, testVnode.get()));
    XCTAssertFalse(ShouldIgnoreVnodeType(VSTR, testVnode.get()));
    XCTAssertFalse(ShouldIgnoreVnodeType(VCPLX, testVnode.get()));
    XCTAssertFalse(ShouldIgnoreVnodeType(static_cast<vtype>(1000), testVnode.get()));
}

- (void)testFileFlaggedInRoot {
    bool fileFlaggedInRoot;
    shared_ptr<mount> testMount = mount::Create();
    shared_ptr<vnode> testVnode = vnode::Create(testMount, "/foo");
    
    testVnode->attrValues.va_flags = FileFlags_IsInVirtualizationRoot;
    XCTAssertTrue(TryGetFileIsFlaggedAsInRoot(testVnode.get(), NULL, &fileFlaggedInRoot));
    XCTAssertTrue(fileFlaggedInRoot);
    
    testVnode->attrValues.va_flags = FileFlags_IsEmpty;
    XCTAssertTrue(TryGetFileIsFlaggedAsInRoot(testVnode.get(), NULL, &fileFlaggedInRoot));
    XCTAssertFalse(fileFlaggedInRoot);
    
    testVnode->attrValues.va_flags = FileFlags_Invalid;
    XCTAssertTrue(TryGetFileIsFlaggedAsInRoot(testVnode.get(), NULL, &fileFlaggedInRoot));
    XCTAssertFalse(fileFlaggedInRoot);
    
    testVnode->attrValues.va_flags = 0x00000100;
    XCTAssertTrue(TryGetFileIsFlaggedAsInRoot(testVnode.get(), NULL, &fileFlaggedInRoot));
    XCTAssertFalse(fileFlaggedInRoot);

    testVnode->errors.getattr = EBADF;
    XCTAssertFalse(TryGetFileIsFlaggedAsInRoot(testVnode.get(), NULL, &fileFlaggedInRoot));
}

- (void)testShouldHandleVnodeOpEvent {
    // In Parameters
    shared_ptr<mount> testMount = mount::Create();
    shared_ptr<vnode> testVnode = vnode::Create(testMount, "/foo");
    testVnode->attrValues.va_flags = FileFlags_IsInVirtualizationRoot;
    PerfTracer perfTracer;
    vfs_context_t _Nonnull context = vfs_context_create(NULL);
    kauth_action_t action = KAUTH_VNODE_READ_DATA;
    
    // Out Parameters
    vtype vnodeType;
    uint32_t vnodeFileFlags;
    int pid;
    char procname[MAXCOMLEN + 1] = "";
    int kauthResult;
    int kauthError;

    
    // Test Success Case
    XCTAssertTrue(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnode.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DEFER);
    
    
    // Test Invalid Action Bit
    XCTAssertFalse(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnode.get(),
            KAUTH_VNODE_ACCESS,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DEFER);

    
    // Test invalid File System
    shared_ptr<mount> testMountHfs = mount::Create("hfs", fsid_t{}, 0);
    shared_ptr<vnode> testVnodeHfs = vnode::Create(testMountHfs, "/hfs");
    XCTAssertFalse(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnodeHfs.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DEFER);
    
    
    // Test invalid VNODE Type
    shared_ptr<vnode> testVnodeInvalidType = vnode::Create(testMount, "/foo2", VNON);
    XCTAssertFalse(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnodeInvalidType.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DEFER);

    
    // Test failure reading attr
    testVnode->errors.getattr = EBADF;
    XCTAssertFalse(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnode.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DENY);
    // reset to valid value
    testVnode->errors.getattr = 0;

    
    // Test invalid file flag
    testVnode->attrValues.va_flags = FileFlags_IsEmpty;
    XCTAssertFalse(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnode.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DEFER);
    // reset to valid value
    testVnode->attrValues.va_flags = FileFlags_IsInVirtualizationRoot;
    

    // Test with file crawler trying to populate an empty file
    testVnode->attrValues.va_flags = FileFlags_IsEmpty | FileFlags_IsInVirtualizationRoot;
    char mds[] = "mds";
    SetProcName(mds);
    XCTAssertFalse(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnode.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DENY);

    
    // Test with finder trying to populate an empty file
    char finder[] = "Finder";
    SetProcName(finder);
    XCTAssertTrue(
        ShouldHandleVnodeOpEvent(
            &perfTracer,
            context,
            testVnode.get(),
            action,
            &vnodeType,
            &vnodeFileFlags,
            &pid,
            procname,
            &kauthResult,
            &kauthError));
    XCTAssertEqual(kauthResult, KAUTH_RESULT_DEFER);
}

@end
