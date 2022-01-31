/**
 * Copyright (c) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "OemNetd"

#include <vector>

#include <android-base/strings.h>
#include <android-base/stringprintf.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>

#include "Controllers.h"
#include "RouteController.h"
#include "OemNetdListener.h"
#include "binder_utils/NetdPermissions.h"

using android::base::StringPrintf;
using android::net::gCtls;

namespace com {
namespace android {
namespace internal {
namespace net {

// The input permissions should be equivalent that this function would return ok if any of them is
// granted.
::android::binder::Status checkAnyPermission(const std::vector<const char*>& permissions) {
    pid_t pid = ::android::IPCThreadState::self()->getCallingPid();
    uid_t uid = ::android::IPCThreadState::self()->getCallingUid();

    // TODO: Do the pure permission check in this function. Have another method
    // (e.g. checkNetworkStackPermission) to wrap AID_SYSTEM and
    // AID_NETWORK_STACK uid check.
    // If the caller is the system UID, don't check permissions.
    // Otherwise, if the system server's binder thread pool is full, and all the threads are
    // blocked on a thread that's waiting for us to complete, we deadlock. http://b/69389492
    //
    // From a security perspective, there is currently no difference, because:
    // 1. The system server has the NETWORK_STACK permission, which grants access to all the
    //    IPCs in this file.
    // 2. AID_SYSTEM always has all permissions. See ActivityManager#checkComponentPermission.
    if (uid == AID_SYSTEM) {
        return ::android::binder::Status::ok();
    }
    // AID_NETWORK_STACK own MAINLINE_NETWORK_STACK permission, don't IPC to system server to check
    // MAINLINE_NETWORK_STACK permission. Cross-process(netd, networkstack and system server)
    // deadlock: http://b/149766727
    if (uid == AID_NETWORK_STACK) {
        for (const char* permission : permissions) {
            if (std::strcmp(permission, PERM_MAINLINE_NETWORK_STACK) == 0) {
                return ::android::binder::Status::ok();
            }
        }
    }

    for (const char* permission : permissions) {
        if (checkPermission(::android::String16(permission), pid, uid)) {
            return ::android::binder::Status::ok();
        }
    }

    auto err = StringPrintf("UID %d / PID %d does not have any of the following permissions: %s",
                            uid, pid, ::android::base::Join(permissions, ',').c_str());
    return ::android::binder::Status::fromExceptionCode(::android::binder::Status::EX_SECURITY, err.c_str());
}

#define ENFORCE_ANY_PERMISSION(...)                                           \
    do {                                                                      \
        ::android::binder::Status status = checkAnyPermission({__VA_ARGS__}); \
        if (!status.isOk()) {                                                 \
            return status;                                                    \
        }                                                                     \
    } while (0)

#define ENFORCE_NETWORK_STACK_PERMISSIONS() \
    ENFORCE_ANY_PERMISSION(PERM_NETWORK_STACK, PERM_MAINLINE_NETWORK_STACK)

::android::binder::Status asBinderStatus(const ::android::netdutils::Status& status) {
    if (isOk(status)) {
        return ::android::binder::Status::ok();
    }
    return ::android::binder::Status::fromServiceSpecificError(status.code(), status.msg().c_str());
}

::android::sp<::android::IBinder> OemNetdListener::getListener() {
    // Thread-safe initialization.
    static ::android::sp<OemNetdListener> listener = ::android::sp<OemNetdListener>::make();
    static ::android::sp<::android::IBinder> sIBinder = ::android::IInterface::asBinder(listener);
    return sIBinder;
}

::android::binder::Status OemNetdListener::isAlive(bool* alive) {
    *alive = true;
    return ::android::binder::Status::ok();
}

::android::binder::Status OemNetdListener::registerOemUnsolicitedEventListener(
        const ::android::sp<IOemNetdUnsolicitedEventListener>& listener) {
    registerOemUnsolicitedEventListenerInternal(listener);
    listener->onRegistered();
    return ::android::binder::Status::ok();
}

void OemNetdListener::registerOemUnsolicitedEventListenerInternal(
        const ::android::sp<IOemNetdUnsolicitedEventListener>& listener) {
    std::lock_guard lock(mOemUnsolicitedMutex);

    // Create the death listener.
    class DeathRecipient : public ::android::IBinder::DeathRecipient {
      public:
        DeathRecipient(OemNetdListener* oemNetdListener,
                       ::android::sp<IOemNetdUnsolicitedEventListener> listener)
            : mOemNetdListener(oemNetdListener), mListener(std::move(listener)) {}
        ~DeathRecipient() override = default;
        void binderDied(const ::android::wp<::android::IBinder>& /* who */) override {
            mOemNetdListener->unregisterOemUnsolicitedEventListenerInternal(mListener);
        }

      private:
        OemNetdListener* mOemNetdListener;
        ::android::sp<IOemNetdUnsolicitedEventListener> mListener;
    };
    ::android::sp<::android::IBinder::DeathRecipient> deathRecipient =
            new DeathRecipient(this, listener);

    ::android::IInterface::asBinder(listener)->linkToDeath(deathRecipient);

    mOemUnsolListenerMap.insert({listener, deathRecipient});
}

void OemNetdListener::unregisterOemUnsolicitedEventListenerInternal(
        const ::android::sp<IOemNetdUnsolicitedEventListener>& listener) {
    std::lock_guard lock(mOemUnsolicitedMutex);
    mOemUnsolListenerMap.erase(listener);
}

::android::binder::Status OemNetdListener::trafficSetRestrictedInterfaceForUid(
        int32_t uid, const std::string& ifName, bool restricted) {
    ENFORCE_NETWORK_STACK_PERMISSIONS();
    auto ifIndex = ::android::net::RouteController::getIfIndex(ifName.c_str());
    return asBinderStatus(gCtls->trafficCtrl.updateUidInterfaceRestrictedMap(uid, ifIndex,
                                                                             restricted));
}

}  // namespace net
}  // namespace internal
}  // namespace android
}  // namespace com
