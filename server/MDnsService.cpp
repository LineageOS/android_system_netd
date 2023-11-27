/**
 * Copyright (c) 2022, The Android Open Source Project
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

#define LOG_TAG "MDnsService"

#include "MDnsService.h"

#include <binder/Status.h>
#include <binder_utils/BinderUtil.h>

using android::net::mdns::aidl::DiscoveryInfo;
using android::net::mdns::aidl::GetAddressInfo;
using android::net::mdns::aidl::IMDnsEventListener;
using android::net::mdns::aidl::RegistrationInfo;
using android::net::mdns::aidl::ResolutionInfo;

namespace android::net {

status_t MDnsService::start() {
    IPCThreadState::self()->disableBackgroundScheduling(true);
    const status_t ret = BinderService<MDnsService>::publish();
    if (ret != android::OK) {
        return ret;
    }
    return android::OK;
}

binder::Status MDnsService::startDaemon() {
    // TODO(b/298594687): switch from EX_SERVICE_SPECIFIC to DEPRECATED when tethering module
    // for 2024-02 release is fully rolled out and prebuilt updated in AP1A.xxxxxx.yy build.
    // Return EX_SERVICE_SPECIFIC for short-term only because callers in tethering module do not
    // catch the EX_UNSUPPORTED_OPERATION. It will throw an exception and cause a fatal exception.
    // The EX_UNSUPPORTED_OPERATION has been catched in tethering module since 2024-02 release.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::stopDaemon() {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::registerService(const RegistrationInfo&) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::discover(const DiscoveryInfo&) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::resolve(const ResolutionInfo&) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::getServiceAddress(const GetAddressInfo&) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::stopOperation(int32_t) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::registerEventListener(const android::sp<IMDnsEventListener>&) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

binder::Status MDnsService::unregisterEventListener(const android::sp<IMDnsEventListener>&) {
    // TODO(b/298594687): switch to DEPRECATED.
    return binder::Status::fromExceptionCode(binder::Status::EX_SERVICE_SPECIFIC);
    // DEPRECATED;
}

}  // namespace android::net
