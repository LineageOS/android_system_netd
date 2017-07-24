/*
 *Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are
 *met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <binder/IPCThreadState.h>
#include <hidl/HidlTransportSupport.h>
#include <hwbinder/IPCThreadState.h>
#include "Controllers.h"
#include "Fwmark.h"
#include "NetdHwService.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::IPCThreadState;
using android::hardware::Void;

namespace android {
namespace net {

/**
 * This lock exists to make NetdHwService RPCs (which come in on multiple HwBinder threads)
 * coexist with the commands in CommandListener.cpp. These are presumed not thread-safe because
 * CommandListener has only one user (NetworkManagementService), which is connected through a
 * FrameworkListener that passes in commands one at a time.
 */
extern android::RWLock gBigNetdLock;

static INetd::StatusCode toHalStatus(int ret) {
    switch(ret) {
        case 0:
            return INetd::StatusCode::OK;
        case -EINVAL:
            return INetd::StatusCode::INVALID_ARGUMENTS;
        case -EEXIST:
            return INetd::StatusCode::ALREADY_EXISTS;
        case -ENONET:
            return INetd::StatusCode::NO_NETWORK;
        case -EPERM:
            return INetd::StatusCode::PERMISSION_DENIED;
        default:
            ALOGE("HAL service error=%d", ret);
            return INetd::StatusCode::UNKNOWN_ERROR;
    }
}

// Minimal service start.
status_t NetdHwService::start() {
    IPCThreadState::self()->disableBackgroundScheduling(true);
    // Usage of this HAL is anticipated to be thin; one thread should suffice.
    configureRpcThreadpool(1, false /* callerWillNotJoin */);
    // Register hardware service with ServiceManager.
    return INetd::registerAsService();
}

Return<void> NetdHwService::createOemNetwork(createOemNetwork_cb _hidl_cb) {
    unsigned netId;
    Permission permission = PERMISSION_SYSTEM;

    android::RWLock::AutoWLock _lock(gBigNetdLock);
    int ret = gCtls->netCtrl.createPhysicalOemNetwork(permission, &netId);

    Fwmark fwmark;
    fwmark.netId = netId;
    fwmark.explicitlySelected = true;
    fwmark.protectedFromVpn = true;
    fwmark.permission = PERMISSION_SYSTEM;
    _hidl_cb(netIdToNetHandle(netId), fwmark.intValue, toHalStatus(ret));

    return Void();
}

Return<INetd::StatusCode> NetdHwService::destroyOemNetwork(uint64_t netHandle) {
    unsigned netId = netHandleToNetId(netHandle);
    if ((netId < NetworkController::MIN_OEM_ID) ||
            (netId > NetworkController::MAX_OEM_ID)) {
        return INetd::StatusCode::INVALID_ARGUMENTS;
    }

    android::RWLock::AutoWLock _lock(gBigNetdLock);

    return toHalStatus(gCtls->netCtrl.destroyNetwork(netId));
}

}  // namespace net
}  // namespace android
