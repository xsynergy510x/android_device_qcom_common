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

#define BUFFER_LENGTH 80

static int sysfs_write_str(char *path, char *s)
{
    char buf[BUFFER_LENGTH];
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

static int sysfs_write_int(char *path, int value)
{
    char buf[BUFFER_LENGTH];
    snprintf(buf, BUFFER_LENGTH, "%d", value);
    return sysfs_write_str(path, buf);
}

typedef struct governor_settings {
    // CPU settings
    char *cpu0_scaling_gov;
    char *cpu1_scaling_gov;
    char *cpu2_scaling_gov;
    char *cpu3_scaling_gov;
    int cpu0_scaling_max_freq;
    int cpu1_scaling_max_freq;
    int cpu2_scaling_max_freq;
    int cpu3_scaling_max_freq;

    // Interactive
    char *interactive_above_hispeed_delay;
    int interactive_align_windows;
    int interactive_boost;
    int interactive_boostpulse;
    int interactive_boostpulse_duration;
    int interactive_go_hispeed_load;
    int interactive_hispeed_freq;
    int interactive_io_is_busy;
    int interactive_max_freq_hysteresis;
    int interactive_min_sample_time;
    char *interactive_target_loads;
    int interactive_timer_rate;
    int interactive_timer_slack;

    // Ondemand
    int ondemand_down_differential;
    int ondemand_down_differential_multi_core;
    int ondemand_enable_turbo_mode;
    int ondemand_freq_step;
    int ondemand_ignore_nice_load;
    int ondemand_input_boost;
    int ondemand_io_is_busy;
    int ondemand_optimal_freq;
    int ondemand_powersave_bias;
    int ondemand_sampling_down_factor;
    int ondemand_sampling_early_factor;
    int ondemand_sampling_interim_factor;
    int ondemand_sampling_rate;
    int ondemand_sampling_rate_min;
    int ondemand_step_up_early_hispeed;
    int ondemand_step_up_interim_hispeed;
    int ondemand_sync_freq;
    int ondemand_up_threshold;
    int ondemand_up_threshold_any_cpu_load;
    int ondemand_up_threshold_multi_core;
} power_profile;

static power_profile profiles[PROFILE_MAX] = {
    [PROFILE_POWER_SAVE] = {
        .cpu0_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu1_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu2_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu3_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu0_scaling_max_freq = 1350000,
        .cpu1_scaling_max_freq = 1350000,
        .cpu2_scaling_max_freq = 1350000,
        .cpu3_scaling_max_freq = 1350000,
        .ondemand_down_differential = 10,
        .ondemand_down_differential_multi_core = 3,
        .ondemand_enable_turbo_mode = 0,
        .ondemand_freq_step = 25,
        .ondemand_ignore_nice_load = 0,
        .ondemand_input_boost = 0,
        .ondemand_io_is_busy = 0,
        .ondemand_optimal_freq 918000,
        .ondemand_powersave_bias = 0,
        .ondemand_sampling_down_factor = 4,
        .ondemand_sampling_early_factor = 1,
        .ondemand_sampling_interim_factor = 1,
        .ondemand_sampling_rate = 50000,
        .ondemand_sampling_rate_min = 10000,
        .ondemand_step_up_early_hispeed = 1134000,
        .ondemand_step_up_interim_hispeed = 1134000,
        .ondemand_sync_freq = 1026000,
        .ondemand_up_threshold = 90,
        .ondemand_up_threshold_any_cpu_load = 80,
        .ondemand_up_threshold_multi_core = 70,
    },
    [PROFILE_BALANCED] = {
        .cpu0_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu1_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu2_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu3_scaling_gov = ONDEMAND_GOVERNOR,
        .cpu0_scaling_max_freq = 1674000,
        .cpu1_scaling_max_freq = 1458000,
        .cpu2_scaling_max_freq = 1458000,
        .cpu3_scaling_max_freq = 1458000,
        .ondemand_down_differential = 10,
        .ondemand_down_differential_multi_core = 3,
        .ondemand_enable_turbo_mode = 0,
        .ondemand_freq_step = 25,
        .ondemand_ignore_nice_load = 0,
        .ondemand_input_boost = 0,
        .ondemand_io_is_busy = 0,
        .ondemand_optimal_freq 918000,
        .ondemand_powersave_bias = 0,
        .ondemand_sampling_down_factor = 4,
        .ondemand_sampling_early_factor = 1,
        .ondemand_sampling_interim_factor = 1,
        .ondemand_sampling_rate = 50000,
        .ondemand_sampling_rate_min = 10000,
        .ondemand_step_up_early_hispeed = 1134000,
        .ondemand_step_up_interim_hispeed = 1134000,
        .ondemand_sync_freq = 1026000,
        .ondemand_up_threshold = 90,
        .ondemand_up_threshold_any_cpu_load = 80,
        .ondemand_up_threshold_multi_core = 70,
    },
    [PROFILE_HIGH_PERFORMANCE] = {
        .cpu0_scaling_gov = INTERACTIVE_GOVERNOR,
        .cpu1_scaling_gov = INTERACTIVE_GOVERNOR,
        .cpu2_scaling_gov = INTERACTIVE_GOVERNOR,
        .cpu3_scaling_gov = INTERACTIVE_GOVERNOR,
        .cpu0_scaling_max_freq = 1890000,
        .cpu1_scaling_max_freq = 1890000,
        .cpu2_scaling_max_freq = 1890000,
        .cpu3_scaling_max_freq = 1890000,
        .interactive_above_hispeed_delay = "20000 1400000:40000 1800000:20000",
        .interactive_align_windows = 1,
        .interactive_boost = 1,
        .interactive_boostpulse = 1134000,
        .interactive_boostpulse_duration = 40,
        .interactive_go_hispeed_load = 95,
        .interactive_hispeed_freq 1134000,
        .interactive_io_is_busy = 1,
        .interactive_max_freq_hysteresis = 100000,
        .interactive_min_sample_time = 80000,
        .interactive_target_loads = "85 1350000:90 1800000:99",
        .interactive_timer_rate = 30000,
        .interactive_timer_slack = 80000,
    },
};

static int profile_high_performance[5] = {
    CPUS_ONLINE_MIN_4,
    CPU0_MIN_FREQ_TURBO_MAX, CPU1_MIN_FREQ_TURBO_MAX,
    CPU2_MIN_FREQ_TURBO_MAX, CPU3_MIN_FREQ_TURBO_MAX
};

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
        
	// Set scaling governor
	sysfs_write_str(CPU0_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu0_scaling_gov);
        sysfs_write_str(CPU1_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu1_scaling_gov);
        sysfs_write_str(CPU2_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu2_scaling_gov);
        sysfs_write_str(CPU3_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu3_scaling_gov);

	// Set max frequency
        sysfs_write_str(CPU0_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu0_scaling_max_freq);
        sysfs_write_str(CPU1_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu1_scaling_max_freq);
        sysfs_write_str(CPU2_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu2_scaling_max_freq);
        sysfs_write_str(CPU3_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu3_scaling_max_freq);

	// Set governor parameters
        sysfs_write_str(INTERACTIVE_PATH "above_hispeed_delay",
            		profiles[profile].interactive_above_hispeed_delay);
        sysfs_write_int(INTERACTIVE_PATH "align_windows",
            		profiles[profile].interactive_align_windows);
        sysfs_write_int(INTERACTIVE_PATH "boost",
            		profiles[profile].interactive_boost);
        sysfs_write_int(INTERACTIVE_PATH "boostpulse",
            		profiles[profile].interactive_boostpulse);
        sysfs_write_int(INTERACTIVE_PATH "boostpulse_duration",
            		profiles[profile].interactive_boostpulse_duration);
        sysfs_write_int(INTERACTIVE_PATH "go_hispeed_load",
            		profiles[profile].interactive_go_hispeed_load);
        sysfs_write_int(INTERACTIVE_PATH "hispeed_freq",
            		profiles[profile].interactive_hispeed_freq);
        sysfs_write_int(INTERACTIVE_PATH "io_is_busy",
            		profiles[profile].interactive_io_is_busy);
        sysfs_write_int(INTERACTIVE_PATH "max_freq_hysteresis",
            		profiles[profile].interactive_max_freq_hysteresis);
        sysfs_write_int(INTERACTIVE_PATH "min_sample_time",
            		profiles[profile].interactive_min_sample_time);
        sysfs_write_str(INTERACTIVE_PATH "target_loads",
            		profiles[profile].interactive_target_loads);
        sysfs_write_int(INTERACTIVE_PATH "timer_rate",
            		profiles[profile].interactive_timer_rate);
        sysfs_write_int(INTERACTIVE_PATH "timer_slack",
            		profiles[profile].interactive_timer_slack);

        ALOGD("%s: set performance mode", __func__);
    } else if (profile == PROFILE_BALANCED) {
        int *resource_values = profile_high_performance;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));

	// Set scaling governor
        sysfs_write_str(CPU0_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu0_scaling_gov);
        sysfs_write_str(CPU1_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu1_scaling_gov);
        sysfs_write_str(CPU2_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu2_scaling_gov);
        sysfs_write_str(CPU3_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu3_scaling_gov);

        // Set max frequency
        sysfs_write_str(CPU0_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu0_scaling_max_freq);
        sysfs_write_str(CPU1_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu1_scaling_max_freq);
        sysfs_write_str(CPU2_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu2_scaling_max_freq);
        sysfs_write_str(CPU3_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu3_scaling_max_freq);

        // Set governor parameters
	sysfs_write_int(ONDEMAND_PATH "down_differential",
                        profiles[profile].ondemand_down_differential);
        sysfs_write_int(ONDEMAND_PATH "down_differential_multi_core",
                        profiles[profile].ondemand_down_differential_multi_core);
        sysfs_write_int(ONDEMAND_PATH "enable_turbo_mode",
                        profiles[profile].ondemand_enable_turbo_mode);
        sysfs_write_int(ONDEMAND_PATH "freq_step",
                        profiles[profile].ondemand_freq_step);
        sysfs_write_int(ONDEMAND_PATH "ignore_nice_load",
                        profiles[profile].ondemand_ignore_nice_load);
        sysfs_write_int(ONDEMAND_PATH "input_boost",
                        profiles[profile].ondemand_input_boost);
        sysfs_write_int(ONDEMAND_PATH "io_is_busy",
                        profiles[profile].ondemand_io_is_busy);
        sysfs_write_int(ONDEMAND_PATH "optimal_freq",
                        profiles[profile].ondemand_optimal_freq);
        sysfs_write_int(ONDEMAND_PATH "powersave_bias",
                        profiles[profile].ondemand_powersave_bias);
        sysfs_write_int(ONDEMAND_PATH "sampling_down_factor",
                        profiles[profile].ondemand_sampling_down_factor);
        sysfs_write_int(ONDEMAND_PATH "sampling_early_factor",
                        profiles[profile].ondemand_sampling_early_factor);
        sysfs_write_int(ONDEMAND_PATH "sampling_interim_factor",
                        profiles[profile].ondemand_sampling_interim_factor);
        sysfs_write_int(ONDEMAND_PATH "sampling_rate",
                        profiles[profile].ondemand_sampling_rate);
        sysfs_write_int(ONDEMAND_PATH "sampling_rate_min",
                        profiles[profile].ondemand_sampling_rate_min);
        sysfs_write_int(ONDEMAND_PATH "step_up_early_hispeed",
                        profiles[profile].ondemand_step_up_early_hispeed);
        sysfs_write_int(ONDEMAND_PATH "step_up_interim_hispeed",
                        profiles[profile].ondemand_step_up_interim_hispeed);
        sysfs_write_int(ONDEMAND_PATH "sync_freq",
                        profiles[profile].ondemand_sync_freq);
        sysfs_write_int(ONDEMAND_PATH "up_threshold",
                        profiles[profile].ondemand_up_threshold);
        sysfs_write_int(ONDEMAND_PATH "up_threshold_any_cpu_load",
                        profiles[profile].ondemand_up_threshold_any_cpu_load);
        sysfs_write_int(ONDEMAND_PATH "up_threshold_multi_core",
                        profiles[profile].ondemand_up_threshold_multi_core);

        ALOGD("%s: set balanced mode", __func__);
    } else if (profile == PROFILE_POWER_SAVE) {
        int* resource_values = profile_power_save;

        perform_hint_action(DEFAULT_PROFILE_HINT_ID,
            resource_values, sizeof(resource_values)/sizeof(resource_values[0]));

	// Set scaling governor
        sysfs_write_str(CPU0_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu0_scaling_gov);
        sysfs_write_str(CPU1_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu1_scaling_gov);
        sysfs_write_str(CPU2_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu2_scaling_gov);
        sysfs_write_str(CPU3_CPUFREQ_PATH "scaling_governor",
                        profiles[profile].cpu3_scaling_gov);

        // Set max frequency
        sysfs_write_str(CPU0_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu0_scaling_max_freq);
        sysfs_write_str(CPU1_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu1_scaling_max_freq);
        sysfs_write_str(CPU2_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu2_scaling_max_freq);
        sysfs_write_str(CPU3_CPUFREQ_PATH "scaling_max_freq",
                        profiles[profile].cpu3_scaling_max_freq);

        // Set governor parameters
	sysfs_write_int(ONDEMAND_PATH "down_differential",
                        profiles[profile].ondemand_down_differential);
        sysfs_write_int(ONDEMAND_PATH "down_differential_multi_core",
                        profiles[profile].ondemand_down_differential_multi_core);
        sysfs_write_int(ONDEMAND_PATH "enable_turbo_mode",
                        profiles[profile].ondemand_enable_turbo_mode);
        sysfs_write_int(ONDEMAND_PATH "freq_step",
                        profiles[profile].ondemand_freq_step);
        sysfs_write_int(ONDEMAND_PATH "ignore_nice_load",
                        profiles[profile].ondemand_ignore_nice_load);
        sysfs_write_int(ONDEMAND_PATH "input_boost",
                        profiles[profile].ondemand_input_boost);
        sysfs_write_int(ONDEMAND_PATH "io_is_busy",
                        profiles[profile].ondemand_io_is_busy);
        sysfs_write_int(ONDEMAND_PATH "optimal_freq",
                        profiles[profile].ondemand_optimal_freq);
        sysfs_write_int(ONDEMAND_PATH "powersave_bias",
                        profiles[profile].ondemand_powersave_bias);
        sysfs_write_int(ONDEMAND_PATH "sampling_down_factor",
                        profiles[profile].ondemand_sampling_down_factor);
        sysfs_write_int(ONDEMAND_PATH "sampling_early_factor",
                        profiles[profile].ondemand_sampling_early_factor);
        sysfs_write_int(ONDEMAND_PATH "sampling_interim_factor",
                        profiles[profile].ondemand_sampling_interim_factor);
        sysfs_write_int(ONDEMAND_PATH "sampling_rate",
                        profiles[profile].ondemand_sampling_rate);
        sysfs_write_int(ONDEMAND_PATH "sampling_rate_min",
                        profiles[profile].ondemand_sampling_rate_min);
        sysfs_write_int(ONDEMAND_PATH "step_up_early_hispeed",
                        profiles[profile].ondemand_step_up_early_hispeed);
        sysfs_write_int(ONDEMAND_PATH "step_up_interim_hispeed",
                        profiles[profile].ondemand_step_up_interim_hispeed);
        sysfs_write_int(ONDEMAND_PATH "sync_freq",
                        profiles[profile].ondemand_sync_freq);
        sysfs_write_int(ONDEMAND_PATH "up_threshold",
                        profiles[profile].ondemand_up_threshold);
        sysfs_write_int(ONDEMAND_PATH "up_threshold_any_cpu_load",
                        profiles[profile].ondemand_up_threshold_any_cpu_load);
        sysfs_write_int(ONDEMAND_PATH "up_threshold_multi_core",
                        profiles[profile].ondemand_up_threshold_multi_core);

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
