#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include "api_client.h"

#define MAX_ENTRIES 128
static RankedEntry entries[MAX_ENTRIES];
static int count, scroll, status, error_code;
static int api_ready = 0;
static vita2d_pgf *font;

static int init_and_fetch(void) {
    int ret;

    /* Always start from a clean client state. */
    if (api_ready) {
        api_client_shutdown();
        api_ready = 0;
    }

    status = 0;
    error_code = 0;
    count = 0;
    scroll = 0;

    ret = api_client_init();
    if (ret < 0) {
        status = -2;
        error_code = ret;
        return ret;
    }

    ret = api_fetch_ranked_list("unit_tier", "queue=1100&patch=current", "unit_tier", entries, MAX_ENTRIES, &count);
    if (ret < 0) {
        api_client_shutdown();
        status = -1;
        error_code = ret;
        api_ready = 0;
        return ret;
    }

    api_ready = 1;
    ranked_entries_sort_by_place(entries, count);
    status = 1;
    return 0;
}

static void draw(void) {
    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255,255,255,255), 1.0f, "MetaTFT Unit Tier List");
    char buf[160];
    if (status == 1) {
        snprintf(buf, sizeof(buf), "%d entries  (avg place / win%% / top4%%)", count);
    } else if (status == -2) {
        snprintf(buf, sizeof(buf), "init error: %d [%s]  (X: retry)", error_code, api_client_init_stage());
    } else if (status < 0) {
        snprintf(buf, sizeof(buf), "fetch error: %d ssl:%d detail:%02X  (X: retry)", error_code, api_client_last_ssl_error(), api_client_last_ssl_detail());
    } else {
        strcpy(buf, "loading...");
    }
    vita2d_pgf_draw_text(font, 20, 50, RGBA8(180,180,180,255), 0.75f, buf);

    int y = 75, end = scroll + 13;
    if (end > count) end = count;
    for (int i = scroll; i < end; ++i) {
        snprintf(buf, sizeof(buf), "%2d. %-30s place %.2f  win %.1f%%  top4 %.1f%%  n=%d", i+1, entries[i].name, entries[i].avg_place, entries[i].win_rate*100.0f, entries[i].top4_rate*100.0f, entries[i].n);
        vita2d_pgf_draw_text(font, 20, y, RGBA8(235,235,235,255), 0.62f, buf);
        y += 34;
    }
}

int main(void) {
    vita2d_init();
    vita2d_set_clear_color(RGBA8(24,26,30,255));
    font = vita2d_load_default_pgf();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    init_and_fetch();

    SceCtrlData prev = {0};
    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        int pressed = pad.buttons & ~prev.buttons;

        if (pressed & SCE_CTRL_CROSS) {
            init_and_fetch();
        }

        if (api_ready) {
            if ((pressed & SCE_CTRL_DOWN) && scroll + 13 < count) ++scroll;
            if ((pressed & SCE_CTRL_UP) && scroll > 0) --scroll;
            if (pressed & SCE_CTRL_RTRIGGER) {
                scroll += 13;
                if (scroll > count-13) scroll = count-13;
                if (scroll < 0) scroll = 0;
            }
            if (pressed & SCE_CTRL_LTRIGGER) {
                scroll -= 13;
                if (scroll < 0) scroll = 0;
            }
        }

        prev = pad;
        vita2d_start_drawing();
        vita2d_clear_screen();
        draw();
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceKernelDelayThread(16000);
    }
    return 0;
}
