/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2011 <kang@insecure.ws>
 * Copyright (C) 2012 James Sullins <jcsullins@gmail.com>
 * Copyright (C) 2013 Micha LaQua <micha.laqua@gmail.com>
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

//#define LOG_NDEBUG 0
#define LOG_TAG "lights"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <utils/Log.h>

char const *const LM8502_FILE = "/dev/lm8502";

/* copied from kernel include/linux/i2c_lm8502_led.h */
#define LM8502_DOWNLOAD_MICROCODE       1
#define LM8502_START_ENGINE             9
#define LM8502_STOP_ENGINE              3
#define LM8502_WAIT_FOR_ENGINE_STOPPED  8

/* LM8502 engine programs */
static const uint16_t lm8502_pulse_quick[] = {
    0x9c0f, 0x9c8f, 0xe004, 0x4000, 0x047f, 0x4c00, 0x047f, 0x4c00,
    0x047f, 0x4c00, 0x057f, 0x4c00, 0xa30a, 0x0000, 0x0000, 0x0007,
    0x9c1f, 0x9c9f, 0xe080, 0x03ff, 0xc800
};

static const uint16_t lm8502_pulse_quickshort[] = {
    0x9c0f, 0x9c8f, 0xe004, 0x4000, 0x047f, 0x4c00, 0x057f, 0x4c00,
    0x057f, 0x4c00, 0x057f, 0x4c00, 0xa30a, 0x0000, 0x0000, 0x0007,
    0x9c1f, 0x9c9f, 0xe080, 0x03ff, 0xc800
};

static const uint16_t lm8502_pulse_long[] = {
    0x9c0f, 0x9c8f, 0xe004, 0x4000, 0x047f, 0x4c00, 0x047f, 0x4c00,
    0x057f, 0x4c00, 0x057f, 0x7c00, 0xa30a, 0x0000, 0x0000, 0x0007,
    0x9c1f, 0x9c9f, 0xe080, 0x03ff, 0xc800
};

static const uint16_t lm8502_pulse_longshort[] = {
    0x9c0f, 0x9c8f, 0xe004, 0x4000, 0x047f, 0x4c00, 0x057f, 0x4c00,
    0x057f, 0x4c00, 0x057f, 0x6c00, 0xa30a, 0x0000, 0x0000, 0x0007,
    0x9c1f, 0x9c9f, 0xe080, 0x03ff, 0xc800
};

static const uint16_t lm8502_pulse_double[] = {
    0x9c0f, 0x9c8f, 0xe004, 0x4000, 0x047f, 0x4c00, 0x057f, 0x4c00,
    0x047f, 0x4c00, 0x057f, 0x7c00, 0xa30a, 0x0000, 0x0000, 0x0007,
    0x9c1f, 0x9c9f, 0xe080, 0x03ff, 0xc800
};

static const uint16_t lm8502_reset[] = {
    0x9c0f, 0x9c8f, 0x03ff, 0xc000
};

int program_lm8502_led(int state)
{
    uint16_t microcode[96];
    int fd, ret;

    ALOGV("%sabling notification light", state ? "En" : "Dis");

    memset(microcode, 0, sizeof(microcode));

    if (state == 1) {
        memcpy(microcode, lm8502_pulse_quick, sizeof(lm8502_pulse_quick));
    } else if (state == 2) {
        memcpy(microcode, lm8502_pulse_quickshort, sizeof(lm8502_pulse_quickshort));
    } else if (state == 3) {
        memcpy(microcode, lm8502_pulse_long, sizeof(lm8502_pulse_long));
    } else if (state == 4) {
        memcpy(microcode, lm8502_pulse_longshort, sizeof(lm8502_pulse_longshort));
    } else if (state == 5) {
        memcpy(microcode, lm8502_pulse_double, sizeof(lm8502_pulse_double));
    } else {
        memcpy(microcode, lm8502_reset, sizeof(lm8502_reset));
    }

    fd = open(LM8502_FILE, O_RDWR);
    if (fd < 0) {
        ALOGE("Opening %s failed - %d", LM8502_FILE, errno);
        ret = -errno;
    } else {
        ret = 0;

        /* download microcode */
        if (ioctl(fd, LM8502_DOWNLOAD_MICROCODE, microcode) < 0) {
            ALOGE("Copying lm8502 microcode failed - %d", errno);
            ret = -errno;
        }
        if (ret == 0 && state) {
            if (ioctl(fd, LM8502_START_ENGINE, 2) < 0) {
                ALOGE("Starting lm8502 engine 2 failed - %d", errno);
                ret = -errno;
            }
        }
        if (ret == 0) {
            if (ioctl(fd, LM8502_START_ENGINE, 1) < 0) {
                ALOGE("Starting lm8502 engine 1 failed - %d", errno);
                ret = -errno;
            }
        }
        if (ret == 0 && !state) {
            ALOGV("stop engine 1");
            if (ioctl(fd, LM8502_STOP_ENGINE, 1) < 0) {
                ALOGE("Stopping lm8502 engine 1 failed - %d", errno);
                ret = -errno;
            }
            if (ioctl(fd, LM8502_STOP_ENGINE, 2) < 0) {
                ALOGE("Stopping lm8502 engine 2 failed - %d", errno);
                ret = -errno;
            }
        }
        if (ret == 0 && !state) {
            ALOGV("Waiting for lm8502 engine to stop after reset");
            int state;
            /* make sure the reset is complete */
            if (ioctl(fd, LM8502_WAIT_FOR_ENGINE_STOPPED, &state) < 0) {
                ALOGW("Waiting for lm8502 reset failed - %d", errno);
                ret = -errno;
            }
            ALOGV("lm8502 reset finished with stop state %d", state);
        }

        close(fd);
    }

    return ret;
}

void init_lm8502_led(void)
{
    uint16_t microcode[96];
    int fd;

    memset(microcode, 0, sizeof(microcode));

    fd = open(LM8502_FILE, O_RDWR);
    if (fd < 0) {
        ALOGE("Cannot open lm8502 device - %d", errno);
        return;
    }

    /* download microcode */
    if (ioctl(fd, LM8502_DOWNLOAD_MICROCODE, microcode) < 0) {
        ALOGE("Cannot download lm8502 microcode - %d", errno);
        return;
    }
    if (ioctl(fd, LM8502_STOP_ENGINE, 1) < 0) {
        ALOGE("Cannot stop lm8502 engine 1 - %d", errno);
    }
    if (ioctl(fd, LM8502_STOP_ENGINE, 2) < 0) {
        ALOGE("Cannot stop lm8502 engine 2 - %d", errno);
    }

    close(fd);
}
