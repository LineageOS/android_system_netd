/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gtest/gtest.h>
#include <vintf/VintfObject.h>

#include <fstream>
#include <string>

#include "bpf/KernelUtils.h"

namespace android {
namespace net {

namespace {

using ::android::vintf::RuntimeInfo;
using ::android::vintf::VintfObject;

class KernelConfigVerifier final {
  public:
    KernelConfigVerifier() : mRuntimeInfo(VintfObject::GetRuntimeInfo()) {}

    bool hasOption(const std::string& option) const {
        const auto& configMap = mRuntimeInfo->kernelConfigs();
        auto it = configMap.find(option);
        if (it != configMap.cend()) {
            return it->second == "y";
        }
        return false;
    }

    bool hasModule(const std::string& option) const {
        const auto& configMap = mRuntimeInfo->kernelConfigs();
        auto it = configMap.find(option);
        if (it != configMap.cend()) {
            return (it->second == "y") || (it->second == "m");
        }
        return false;
    }

  private:
    std::shared_ptr<const RuntimeInfo> mRuntimeInfo;
};

}  // namespace

/**
 * If this test fails, enable the following kernel modules in your kernel config:
 * CONFIG_NET_CLS_MATCHALL=y
 * CONFIG_NET_ACT_POLICE=y
 * CONFIG_NET_ACT_BPF=y
 * CONFIG_BPF_JIT=y
 */
TEST(KernelTest, TestRateLimitingSupport) {
    KernelConfigVerifier configVerifier;
    ASSERT_TRUE(configVerifier.hasOption("CONFIG_NET_CLS_MATCHALL"));
    ASSERT_TRUE(configVerifier.hasOption("CONFIG_NET_ACT_POLICE"));
    ASSERT_TRUE(configVerifier.hasOption("CONFIG_NET_ACT_BPF"));
    ASSERT_TRUE(configVerifier.hasOption("CONFIG_BPF_JIT"));
}

TEST(KernelTest, TestBpfJitAlwaysOn) {
    // 32-bit arm & x86 kernels aren't capable of JIT-ing all of our BPF code,
    if (bpf::isKernel32Bit()) GTEST_SKIP() << "Exempt on 32-bit kernel.";
    KernelConfigVerifier configVerifier;
    ASSERT_TRUE(configVerifier.hasOption("CONFIG_BPF_JIT_ALWAYS_ON"));
}

/* Android 14/U should only launch on 64-bit kernels
 *   T launches on 5.10/5.15
 *   U launches on 5.15/6.1
 * So >=5.16 implies isKernel64Bit()
 */
TEST(KernelTest, TestKernel64Bit) {
    if (!bpf::isAtLeastKernelVersion(5, 16, 0)) GTEST_SKIP() << "Exempt on < 5.16 kernel.";
    ASSERT_TRUE(bpf::isKernel64Bit());
}

// Android V requires 4.19+
TEST(KernelTest, TestKernel419) {
    ASSERT_TRUE(bpf::isAtLeastKernelVersion(4, 19, 0));
}

TEST(KernelTest, TestSupportsCommonUsbEthernetDongles) {
    KernelConfigVerifier configVerifier;
    if (!configVerifier.hasModule("CONFIG_USB")) GTEST_SKIP() << "Exempt without USB support.";
    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_NET_AX8817X"));
    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_NET_AX88179_178A"));
    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_NET_CDCETHER"));
    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_NET_CDC_EEM"));
    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_NET_CDC_NCM"));
    if (bpf::isAtLeastKernelVersion(5, 4, 0))
        ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_NET_AQC111"));

    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_RTL8152"));
    ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_RTL8150"));
    if (bpf::isAtLeastKernelVersion(5, 15, 0)) {
        ASSERT_TRUE(configVerifier.hasModule("CONFIG_USB_RTL8153_ECM"));
        ASSERT_TRUE(configVerifier.hasModule("CONFIG_AX88796B_PHY"));
    }
}

}  // namespace net
}  // namespace android
