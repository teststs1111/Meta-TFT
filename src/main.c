/* MetaTFT Viewer for PS Vita */
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include "api_client.h"

#define MAX_ENTRIES 128
#define ROW_HEIGHT 34
#define VISIBLE_ROWS 14
#define LIST_TOP 60
#define API_PATH "unit_tier"
#define API_NAME_KEY "unit_tier"
#define API_QUERY "queue=1100&patch=current"

static RankedEntry g_entries[MAX_ENTRIES];
static int g_entry_count, g_scroll, g_status, g_error_code;
static vita2d_font *g_font;

static void pretty_unit_name(const char *raw, char *out, size_t cap) {
    const char *p = raw;
    if (strncmp(p, "TFT", 3) == 0) {
        p += 3; while (*p >= '0' && *p <= '9') p++;
        if (*p == '_') p++;
    } else if (strncmp(p, "DA_", 3) == 0) {
        p += 3; const char *q = p;
        while (*q >= '0' && *q <= '9') q++;
        if (q != p && *q == '_') p = q + 1;
    }
    char base[API_UNIT_NAME_LEN];
    strncpy(base, p, sizeof(base) - 1); base[sizeof(base) - 1] = '\0';
    size_t len = strlen(base);
    int star = 0;
    if (len > 2 && base[len - 2] == '_' && base[len - 1] >= '0' && base[len - 1] <= '9') {
        star = base[len - 1] - '0'; base[len - 2] = '\0';
    }
    if (star) snprintf(out, cap, "%s  %d*", base, star);
    else snprintf(out, cap, "%s", base);
}

static void fetch_data(void) {
    g_status = 0;
    int ret = api_fetch_ranked_list(API_PATH, API_QUERY, API_NAME_KEY, g_entries, MAX_ENTRIES, &g_entry_count);
    if (ret < 0) { g_status = -1; g_error_code = ret; return; }
    ranked_entries_sort_by_place(g_entries, g_entry_count); g_status = 1;
}

static void draw_header(void) {
    vita2d_font_draw_text(g_font, 20, 30, RGBA8(255,255,255,255), 24, "MetaTFT Unit Tier List");
    char sub[128];
    if (g_status == 1) snprintf(sub, sizeof(sub), "%d entries  (avg place / win%% / top4%%)", g_entry_count);
    else if (g_status == -1) snprintf(sub, sizeof(sub), "fetch error: %d  (X: retry)", g_error_code);
    else snprintf(sub, sizeof(sub), "loading...");
    vita2d_font_draw_text(g_font, 20, 48, RGBA8(180,180,180,255), 16, sub);
}

static void draw_list(void) {
    int y = LIST_TOP, end = g_scroll + VISIBLE_ROWS;
    if (end > g_entry_count) end = g_entry_count;
    for (int i = g_scroll; i < end; i++) {
        RankedEntry *e = &g_entries[i]; char name[96], line[160];
        pretty_unit_name(e->name, name, sizeof(name));
        snprintf(line, sizeof(line), "%2d. %-24s  place %.2f   win %4.1f%%   top4 %4.1f%%   n=%d", i+1, name, e->avg_place, e->win_rate*100.0f, e->top4_rate*100.0f, e->n);
        vita2d_font_draw_text(g_font, 20, y, (i%2==0)?RGBA8(235,235,235,255):RGBA8(190,190,190,255), 18, line);
        y += ROW_HEIGHT;
    }
    if (g_entry_count == 0 && g_status == 1)
        vita2d_font_draw_text(g_font, 20, LIST_TOP, RGBA8(255,120,120,255), 18, "0 entries. API_QUERY を見直してください。");
}

static void handle_input(void) {
    static SceCtrlData prev; SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    int pressed = pad.buttons & ~prev.buttons;
    if (pressed & SCE_CTRL_DOWN && g_scroll + VISIBLE_ROWS < g_entry_count) g_scroll++;
    if (pressed & SCE_CTRL_UP && g_scroll > 0) g_scroll--;
    if (pressed & SCE_CTRL_RTRIGGER) { g_scroll += VISIBLE_ROWS; if (g_scroll > g_entry_count - VISIBLE_ROWS) g_scroll = g_entry_count - VISIBLE_ROWS; if (g_scroll < 0) g_scroll = 0; }
    if (pressed & SCE_CTRL_LTRIGGER) { g_scroll -= VISIBLE_ROWS; if (g_scroll < 0) g_scroll = 0; }
    if (pressed & SCE_CTRL_CROSS) { fetch_data(); g_scroll = 0; }
    prev = pad;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vita2d_init(); vita2d_set_clear_color(RGBA8(24,26,30,255));
    g_font = vita2d_load_font_file("app0:/font.ttf");
    if (!g_font) g_font = vita2d_load_default_font();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if (api_client_init() < 0) { g_status = -1; g_error_code = -999; } else fetch_data();
    while (1) {
        handle_input(); vita2d_start_drawing(); vita2d_clear_screen();
        draw_header(); draw_list(); vita2d_end_drawing(); vita2d_swap_buffers();
        sceKernelDelayThread(16000);
    }
    return 0;
}
