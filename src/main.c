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
static vita2d_pgf *font;

static void fetch_data(void) {
    status = 0;
    int ret = api_fetch_ranked_list("unit_tier", "queue=1100&patch=current", "unit_tier", entries, MAX_ENTRIES, &count);
    if (ret < 0) { status = -1; error_code = ret; return; }
    ranked_entries_sort_by_place(entries, count); status = 1;
}

static void draw(void) {
    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255,255,255,255), 1.0f, "MetaTFT Unit Tier List");
    char buf[160];
    if (status == 1) snprintf(buf, sizeof(buf), "%d entries  (avg place / win%% / top4%%)", count);
    else if (status < 0) snprintf(buf, sizeof(buf), "fetch error: %d  (X: retry)", error_code);
    else strcpy(buf, "loading...");
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
    if (api_client_init() < 0) { status = -1; error_code = -999; } else fetch_data();
    SceCtrlData prev = {0};
    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        int pressed = pad.buttons & ~prev.buttons;
        if (pressed & SCE_CTRL_CROSS) { fetch_data(); scroll = 0; }
        if ((pressed & SCE_CTRL_DOWN) && scroll + 13 < count) ++scroll;
        if ((pressed & SCE_CTRL_UP) && scroll > 0) --scroll;
        if (pressed & SCE_CTRL_RTRIGGER) { scroll += 13; if (scroll > count-13) scroll = count-13; if (scroll < 0) scroll = 0; }
        if (pressed & SCE_CTRL_LTRIGGER) { scroll -= 13; if (scroll < 0) scroll = 0; }
        prev = pad;
        vita2d_start_drawing(); vita2d_clear_screen(); draw(); vita2d_end_drawing(); vita2d_swap_buffers();
        sceKernelDelayThread(16000);
    }
    return 0;
}
