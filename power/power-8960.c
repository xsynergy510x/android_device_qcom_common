/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2015, The CyanogenMod Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NIDEBUG 0

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define LOG_TAG "QCOM PowerHAL"
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"

static int current_power_profile = PROFILE_BALANCED;

int get_number_of_profiles() {
    return 3;
}

/** 
 * High Performance profile
 * All cores online
 * Max freqency set
 */

static int profile_high_performance[5] = {
    CPUS_ONLINE_MIN_4,
    0x212, 0x312,
    0x412, 0x512
};

/**
 * Balanced profile
 * 4 cores in hotplug
 * 1.67GHz for CPU0, 1.45GHz for CPU1-3
 */

static int profile_balanced[5] = {
    CPUS_ONLINE_MAX_LIMIT_4,
    0x1510, 0x160E,
    0x170E, 0x180E
};

/**
 * High Performance profile
 * 2 cores in hotplug
 * 1.026GHz for CPU0-1
 */

static int profile_power_save[5] = {
    CPUS_ONLINE_MAX_LIMIT_2,
    CPU0_MAX_FREQ_NONTURBO_MAX, CPU1_MAX_FREQ_NONTURBO_MAX,
    CPU2_MAX_FREQ_NONTURBO_MAX, CPU3_MAX_FREQ_NONTURBO_MAX
};

static void set_power_profile(int profile) {

    if (profile == current_power_profile)
        return;

    ALOGV("%s: profile=%d", __func__, profile);

    if (current_power_profile != PROFILE_BALANCED) {
        undo_hint_action(DEFAULT_PROFILE_HINT_ID);
        ALOGV("%s: hint undone", __func__);
    }

    if (profile == PROFILE_HIGH_PERFORMANCE) {
        int *resource_values = profile_high_performance;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
        ALOGD("%s: set performance mode", __func__);
    } else if (profile == PROFILE_BALANCED) {
        int* resource_values = profile_balanced;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
        ALOGD("%s: set balanced mode", __func__);
    } else if (profile == PROFILE_POWER_SAVE) {
        int* resource_values = profile_power_save;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
        ALOGD("%s: set powersave mode", __func__);
    }

    current_power_profile = profile;
}

int power_hint_override(__attribute__((unused)) struct power_module *module,
        power_hint_t hint, void *data)
{
    if (hint == POWER_HINT_SET_PROFILE) {
        set_power_profile(*(int32_t *)data);
        return HINT_HANDLED;
    }

    if (hint == POWER_HINT_LOW_POWER) {
        if (current_power_profile == PROFILE_POWER_SAVE) {
            set_power_profile(PROFILE_BALANCED);
        } else {
            set_power_profile(PROFILE_POWER_SAVE);
        }
    }

    // Skip other hints in custom power modes
    if (current_power_profile != PROFILE_BALANCED) {
        return HINT_HANDLED;
    }

    return HINT_NONE;
}
