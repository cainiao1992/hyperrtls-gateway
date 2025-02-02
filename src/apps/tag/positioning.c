#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <models/tag.h>
#include <uwb/uwb.h>
#include <uwb/tag.h>

#include "rtls/rtls.h"

LOG_MODULE_REGISTER(positioning);

static const struct rtls_anchor anchors[] = {
    { // shelf
        .addr = 1,
        .pos = {
            .x = 2.34,
            .y = 1.57,
            .z = 4.31
        }
    },
    { // corner of window
        .addr = 2,
        .pos = {
            .x = 0.27,
            .y = 2.31,
            .z = 5.01
        }
    },
    { // wardrobe
        .addr = 3,
        .pos = {
            .x = 2.56,
            .y = 2.36,
            .z = 0.58
        }
    },
    { // door
        .addr = 4,
        .pos = {
            .x = 0.14,
            .y = 0.05,
            .z = 0.02
        }
    },
};

int perform_positioning(struct rtls_result *out_result, size_t repetitions) {
    static struct rtls_pos last_pos = { 0 };

    size_t anchor_indices[4];
    rtls_select_nearby_anchors(anchors, ARRAY_SIZE(anchors), last_pos, anchor_indices);

    // order of iteration is enforced by the fact, that
    // tags are getting overwhelmed if they're pinged in a row
    struct rtls_measurement measurements[4] = { 0 };
    for (size_t i = 0; i < repetitions; i++) {
        for (size_t j = 0; j < 4; j++) {
            const struct rtls_anchor *anchor = &anchors[anchor_indices[j]];
            float distance;
            int twr_res = uwb_tag_twr(CONFIG_HRTLS_PAN_ID, CONFIG_HRTLS_UWB_ADDR, anchor->addr, &distance);
            if (twr_res) {
                LOG_WRN("TWR fail with anchor %" PRIu16 ", res: %d", anchor->addr, twr_res);
                return -1;
            }
            measurements[j].distance += distance;
            k_sleep(K_MSEC(2));
        }
    }
    for (size_t i = 0; i < 4; i++) {
        measurements[i].distance /= repetitions;
        measurements[i].anchor_pos = anchors[anchor_indices[i]].pos;
    }

    int64_t start_time = k_uptime_get();
    struct rtls_result result;
    int fp_res = rtls_find_position(measurements, &result);
    if (fp_res) {
        LOG_WRN("Find position fail, res: %d", fp_res);
        return -2;
    }
    last_pos = result.pos;

    LOG_INF("Find position res %f/%f/%f/%f/%" PRId64,
            result.pos.x, result.pos.y, result.pos.z,
            result.error,
            k_uptime_get() - start_time);

    *out_result = result;
    return 0;
}
