/*
 * Copyright (C) 2008 The Android Open Source Project
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
 */

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <linux/wireless.h>

#define LOG_TAG "SoftapController"
#include <cutils/log.h>

extern "C" int ifc_init();
extern "C" int ifc_up(const char *name);

#include "SoftapControllerLibra.h"

#define HOSTAPD_CONF_DIR                        "/data/hostapd"
static const char HOSTAPD_CONF_FILE[]           = HOSTAPD_CONF_DIR "/hostapd.conf";
static const char HOSTAPD_DEFAULT_CONFIG_FILE[] = "/system/etc/firmware/wlan/hostapd_default.conf";
static const char ACCEPT_LIST_FILE[]            = HOSTAPD_CONF_DIR "/hostapd.accept";
static const char DENY_LIST_FILE[]              = HOSTAPD_CONF_DIR "/hostapd.deny";
#define MAX_CONF_LINE_LEN  (156)

int ensure_config_file_exists()
{
    char buf[2048];
    int srcfd, destfd;
    int nread;

    if (access(HOSTAPD_CONF_FILE, R_OK | W_OK) == 0) {
        return 0;
    } else if (errno != ENOENT) {
        LOGE("Cannot access \"%s\": %s", HOSTAPD_CONF_FILE, strerror(errno));
        return -1;
    }

    srcfd = open(HOSTAPD_DEFAULT_CONFIG_FILE, O_RDONLY);
    if (srcfd < 0) {
        LOGE("Cannot open \"%s\": %s", HOSTAPD_DEFAULT_CONFIG_FILE,
                strerror(errno));
        return -1;
    }

    destfd = open(HOSTAPD_CONF_FILE, O_CREAT | O_WRONLY, 0660);
    if (destfd < 0) {
        close(srcfd);
        LOGE("Cannot create \"%s\": %s", HOSTAPD_CONF_FILE, strerror(errno));
        return -1;
    }

    while ((nread = read(srcfd, buf, sizeof(buf))) != 0) {
        if (nread < 0) {
            LOGE("Error reading \"%s\": %s", HOSTAPD_DEFAULT_CONFIG_FILE,
                    strerror(errno));
            close(srcfd);
            close(destfd);
            unlink(HOSTAPD_CONF_FILE);
            return -1;
        }
        write(destfd, buf, nread);
    }

    close(destfd);
    close(srcfd);
//
//    if (chown(HOSTAPD_CONF_FILE, AID_SYSTEM, AID_WIFI) < 0) {
//        LOGE("Error changing group ownership of %s to %d: %s",
//                HOSTAPD_CONF_FILE, AID_WIFI, strerror(errno));
//        unlink(HOSTAPD_CONF_FILE);
//        return -1;
//    }

    return 0;
}

void check_for_configuration_files(void)
{
    FILE * fp;

    /* Check if configuration files are present, if not create the default files */
    mkdir(HOSTAPD_CONF_DIR, 0770);

    ensure_config_file_exists();

    /* If Accept MAC list file does not exist, create an empty file */
    if (NULL == (fp = fopen(ACCEPT_LIST_FILE, "r"))) {
        fp = fopen(ACCEPT_LIST_FILE, "w+");
        if (fp)
            fclose(fp);
    } else {
        fclose(fp);
    }

    /* If deny MAC list file does not exhist, create an empty file */
    if (NULL == (fp = fopen(DENY_LIST_FILE, "r"))) {
        fp = fopen(DENY_LIST_FILE, "w+");
        if (fp)
            fclose(fp);
    } else {
        fclose(fp);
    }

    return;
}


int SoftapController::stopBSS(char *iface) {
#define QCIEEE80211_IOCTL_STOPBSS   (SIOCIWFIRSTPRIV + 6)
    LOGD("stopBSS on %s", iface);
	struct iwreq wrq;
	int ret;
	char cmd[] = "stopbss";

	strncpy(wrq.ifr_name, iface, sizeof(wrq.ifr_name));
	wrq.u.data.length = sizeof(cmd);
	wrq.u.data.pointer = cmd;
	wrq.u.data.flags = 0;

	ret = ioctl(mSock, QCIEEE80211_IOCTL_STOPBSS, &wrq);
    // Discard return value as it's not revelant
	ret = 0;

	sched_yield();
	return 0;
}

SoftapController::SoftapController() {
	mPid = 0;
	mSock = socket(AF_INET, SOCK_DGRAM, 0);
	if (mSock < 0)
		LOGE("Failed to open socket");
	memset(mIface, 0, sizeof(mIface));
}

SoftapController::~SoftapController() {
	if (mSock >= 0)
		close(mSock);
}

int SoftapController::startDriver(char *iface) {
	return 0;
}

int SoftapController::stopDriver(char *iface) {
	return 0;
}

int SoftapController::startSoftap() {
    pid_t pid = 1;
    int ret = 0;

    if (mPid) {
        LOGE("Softap already started");
        return 0;
    }
    if (mSock < 0) {
        LOGE("Softap startap - failed to open socket");
        return -1;
    }
    if ((pid = fork()) < 0) {
        LOGE("fork failed (%s)", strerror(errno));
        return -1;
    }

    if (!pid) {
        check_for_configuration_files();

        ifc_init();
        ifc_up("softpa.0");

        if (execl("/system/bin/hostapd", "/system/bin/hostapd",
            HOSTAPD_CONF_FILE, (char *) NULL)) {
            LOGE("execl failed (%s)", strerror(errno));
        }
        LOGE("Should never get here!");
        return -1;
    } else {
        mPid=pid;
        usleep(AP_BSS_START_DELAY);
        LOGD("Softap service started");
        return 0;
    }
}

int SoftapController::stopSoftap() {
    int ret = 0;

    if (mPid == 0) {
        LOGE("Softap already stopped");
        return 0;
    }

    stopBSS(mIface);
    sleep(1);

    LOGD("Stopping Softap service");
    kill(mPid, SIGTERM);
    waitpid(mPid, NULL, 0);

    if (mSock < 0) {
        LOGE("Softap stopap - failed to open socket");
        return -1;
    }
    *mBuf = 0;
    mPid = 0;
    usleep(AP_BSS_STOP_DELAY);

    LOGD("Softap service stopped: %d", ret);
    return ret;
}

bool SoftapController::isSoftapStarted() {
    return (mPid != 0 ? true : false);
}

/*
 * Arguments:
 *      argv[2] - wlan interface
 *      argv[3] - softap interface
 *      argv[4] - SSID
 *	argv[5] - Security
 *	argv[6] - Key
 *	argv[7] - Channel
 *	argv[8] - Preamble
 *	argv[9] - Max SCB
 */
int SoftapController::setSoftap(int argc, char *argv[]) {
    int ret = 0;
    char buf[128];
    char newSSID[128];
    char newWPA[1024];

    LOGD("setSoftap %s - %s - %s - %s - %s - %s",argv[2],argv[3],argv[4],argv[5],argv[6],argv[7]);

    if (mPid == 0) {
        LOGE("Softap set - hostapd not started");
        return -1;
    }
    if (argc < 4) {
        LOGE("Softap set - missing arguments");
        return -1;
    }

    strncpy(mIface, argv[3], sizeof(mIface));

    FILE* fp = fopen("/system/etc/firmware/wlan/hostapd_default.conf", "r");
    if (!fp) {
       LOGE("Softap set - hostapd temp file read failed");
       return -1;
    }

    FILE* fp2 = fopen("/data/hostapd/hostapd.conf", "w");
    if (!fp2) {
       LOGE("Softap set - hostapd.conf file read failed");
       fclose(fp);
       return -1;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        if((strncmp(buf,"ssid=",5) == 0) ||
           (strncmp(buf,"wpa=",4) == 0) ||
           (strncmp(buf,"wpa_passphrase=",15) == 0) ||
           (strncmp(buf,"wpa_key_mgmt=",12) == 0) ||
           (strncmp(buf,"wpa_pairwise=",12) == 0) ||
           (strncmp(buf,"rsn_pairwise=",12) == 0)) {
           continue;
        }
        fputs(buf,fp2);
    }

    // Update SSID
    sprintf(newSSID,"ssid=%s\n",argv[4]);
    fputs(newSSID,fp2);

    // Update security
    if(strncmp(argv[5],"open",4) != 0) {
        sprintf(newWPA,"wpa=2\nwpa_passphrase=%s\nwpa_key_mgmt=WPA-PSK\nwpa_pairwise=CCMP\nrsn_pairwise=CCMP\n",argv[6]);
        fputs(newWPA,fp2);
    }

    fclose(fp);
    fclose(fp2);

    if (mPid) {
        LOGD("HostApd already loaded, restarting");
        stopSoftap();
        startSoftap();
    }

    return ret;
}

/*
 * Arguments:
 *	argv[2] - interface name
 *	argv[3] - AP or STA
 */
int SoftapController::fwReloadSoftap(int argc, char *argv[])
{
    return 0;
}
