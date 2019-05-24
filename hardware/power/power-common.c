/*
 * Copyright (C) 2015 The CyanogenMod Project
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
#define LOG_TAG "PowerHAL"

#include <hardware/hardware.h>
#include <hardware/power.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utils/Log.h>

#define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))

#define LOW_POWER_MAX_FREQ "1026000"
#define LOW_POWER_MIN_FREQ "384000"
#define NORMAL_MAX_FREQ "1512000"

#define MAX_FREQ_LIMIT_PATH "/sys/kernel/cpufreq_limit/limited_max_freq"
#define MIN_FREQ_LIMIT_PATH "/sys/kernel/cpufreq_limit/limited_min_freq"

#define TS_SOCKET_LOCATION "/dev/socket/tsdriver"

static pthread_mutex_t low_power_mode_lock = PTHREAD_MUTEX_INITIALIZER;
static bool low_power_mode = false;
static int last_state = -1;

/* connects to the touchscreen socket */
static void send_ts_socket(char *send_data) {
    struct sockaddr_un unaddr;
    int ts_fd, len;

    ts_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (ts_fd >= 0) {
        unaddr.sun_family = AF_UNIX;
        strcpy(unaddr.sun_path, TS_SOCKET_LOCATION);
        len = strlen(unaddr.sun_path) + sizeof(unaddr.sun_family);
        if (connect(ts_fd, (struct sockaddr *)&unaddr, len) >= 0) {
            send(ts_fd, send_data, sizeof(*send_data), 0);
        }
        close(ts_fd);
    }
}

static int sysfs_write_str(char *path, char *s)
{
    char buf[80];
    int len;
    int ret = 0;
    int fd;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1 ;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
        ret = -1;
    }

    close(fd);

    return ret;
}

void power_init(void)
{
    ALOGI("Tenderloin power HAL initing.");
}

void power_set_interactive(int on)
{
    if (last_state != on) {
        send_ts_socket(on ? "O" : "C");
        last_state = on;
    }
}

void power_hint(power_hint_t hint, void *data)
{
    switch (hint) {
    case POWER_HINT_INTERACTION:
        break;
    case POWER_HINT_LOW_POWER:
        pthread_mutex_lock(&low_power_mode_lock);
        if (*(int32_t *)data) {
            low_power_mode = true;
            sysfs_write_str(MIN_FREQ_LIMIT_PATH, LOW_POWER_MIN_FREQ);
            sysfs_write_str(MAX_FREQ_LIMIT_PATH, LOW_POWER_MAX_FREQ);
        } else {
            low_power_mode = false;
            sysfs_write_str(MAX_FREQ_LIMIT_PATH, NORMAL_MAX_FREQ);
        }
        pthread_mutex_unlock(&low_power_mode_lock);
        break;
    default:
        break;
    }
}

void set_feature(feature_t UNUSED(feature), int UNUSED(state))
{
}
