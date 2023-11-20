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
    DEPRECATED;
}

binder::Status MDnsService::stopDaemon() {
    DEPRECATED;
}

binder::Status MDnsService::registerService(const RegistrationInfo&) {
    DEPRECATED;
}

binder::Status MDnsService::discover(const DiscoveryInfo&) {
    DEPRECATED;
}

binder::Status MDnsService::resolve(const ResolutionInfo&) {
    DEPRECATED;
}

binder::Status MDnsService::getServiceAddress(const GetAddressInfo&) {
    DEPRECATED;
}

binder::Status MDnsService::stopOperation(int32_t) {
    DEPRECATED;
}

binder::Status MDnsService::registerEventListener(const android::sp<IMDnsEventListener>&) {
    DEPRECATED;
}

binder::Status MDnsService::unregisterEventListener(const android::sp<IMDnsEventListener>&) {
    DEPRECATED;
}

}  // namespace android::net
