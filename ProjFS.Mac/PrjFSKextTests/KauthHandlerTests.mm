#import <XCTest/XCTest.h>
#include "../PrjFSKextTests/VnodeMock.h"
#include "../PrjFSKext/KauthHandlerTestable.hpp"

@interface KauthHandlerTests : XCTestCase
@end

@implementation KauthHandlerTests

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
    XCTAssertTrue(ShouldIgnoreVnodeType(VNON, NULL));
    XCTAssertTrue(ShouldIgnoreVnodeType(VBLK, NULL));
    XCTAssertTrue(ShouldIgnoreVnodeType(VCHR, NULL));
    XCTAssertTrue(ShouldIgnoreVnodeType(VSOCK, NULL));
    XCTAssertTrue(ShouldIgnoreVnodeType(VFIFO, NULL));
    XCTAssertTrue(ShouldIgnoreVnodeType(VBAD, NULL));
    XCTAssertFalse(ShouldIgnoreVnodeType(VREG, NULL));
    XCTAssertFalse(ShouldIgnoreVnodeType(VDIR, NULL));
    XCTAssertFalse(ShouldIgnoreVnodeType(VLNK, NULL));
    XCTAssertFalse(ShouldIgnoreVnodeType(VSTR, NULL));
    XCTAssertFalse(ShouldIgnoreVnodeType(VCPLX, NULL));
}

- (void)testFileFlaggedInRoot {
    bool fileFlaggedInRoot;
    vnode mockVnode;

    // These values are only used for logging and don't impact the test
    mockVnode.isrecycled = false;
    mockVnode.vnodeType = VNON;

    mockVnode.attr = FileFlags_IsInVirtualizationRoot;
    XCTAssertTrue(TryGetFileIsFlaggedAsInRoot(&mockVnode, NULL, &fileFlaggedInRoot));
    XCTAssertTrue(fileFlaggedInRoot);

    mockVnode.attr = FileFlags_IsEmpty;
    XCTAssertTrue(TryGetFileIsFlaggedAsInRoot(&mockVnode, NULL, &fileFlaggedInRoot));
    XCTAssertTrue(fileFlaggedInRoot);
}

@end
