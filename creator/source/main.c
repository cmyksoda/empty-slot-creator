#include <ctype.h>
#include <fat.h>
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>

#include "apps.h"
#include "vwii_wad.h"
#include "wad.h"
#include "wii_wad.h"

#define MAX_APPS 1024
#define ROWS 12
#define WAD_NAME_MAX 80
#define HIGHLIGHT "\x1b[47m\x1b[30m"
#define NORMAL "\x1b[40m\x1b[37m"
// The blue of the Wii Menu's buttons, so a saved WAD stands out.
#define WII_BLUE "\x1b[38;2;52;190;237m"

typedef enum { UNKNOWN, WII, VWII } console;

static const char *const console_names[] = { "not checked", "Wii", "Wii U (vWii)" };

static app apps[MAX_APPS];
static u64 *titles;
static u32 title_count;
static volatile bool power_pressed, reset_pressed;

static void on_power(void)
{
    power_pressed = true;
}

static void on_remote_power(s32 channel)
{
    power_pressed = true;
}

static void on_reset(u32 irq, void *context)
{
    reset_pressed = true;
}

static void init_video(void)
{
    VIDEO_Init();
    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
    console_init(xfb, 20, 20, mode->fbWidth, mode->xfbHeight, mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (mode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
}

// The Wii U's Latte reports 0xCAFE in its chip ID. Reading it needs the AHB access HBC grants for
// <ahb_access/> in meta.xml; without it there's no telling.
static console detect_console(void)
{
    if (*(vu32 *)0xCD800064 != 0xFFFFFFFF)
        return UNKNOWN;
    return (*(vu32 *)0xCD8005A0 >> 16) == 0xCAFE ? VWII : WII;
}

static void load_installed_titles(void)
{
    if (ES_GetNumTitles(&title_count) < 0 || title_count == 0)
        return;
    titles = memalign(32, title_count * sizeof *titles);
    if (!titles || ES_GetTitles(titles, title_count) < 0)
        title_count = 0;
}

static bool installed(const char *id)
{
    u64 wanted = 0x0001000100000000ULL | (u32)id[0] << 24 | id[1] << 16 | id[2] << 8 | id[3];
    for (u32 i = 0; i < title_count; i++)
        if (titles[i] == wanted)
            return true;
    return false;
}

// Made from the app's path, so making a channel for the same app again replaces the old one.
static void title_id_for(const char *path, int variant, char id[TITLE_ID_LENGTH + 1])
{
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    u32 hash = 2166136261u + variant;
    for (const char *p = path; *p; p++)
        hash = (hash ^ tolower((unsigned char)*p)) * 16777619u;
    id[0] = 'E';
    for (int i = 1; i < TITLE_ID_LENGTH; i++) {
        id[i] = digits[hash % 36];
        hash /= 36;
    }
    id[TITLE_ID_LENGTH] = '\0';
}

static u32 buttons_down(void)
{
    static const struct {
        u16 pad;
        u32 wpad;
    } gamecube[] = {
        { PAD_BUTTON_A, WPAD_BUTTON_A },         { PAD_BUTTON_UP, WPAD_BUTTON_UP },
        { PAD_BUTTON_DOWN, WPAD_BUTTON_DOWN },   { PAD_BUTTON_LEFT, WPAD_BUTTON_LEFT },
        { PAD_BUTTON_RIGHT, WPAD_BUTTON_RIGHT }, { PAD_TRIGGER_R, WPAD_BUTTON_PLUS },
        { PAD_TRIGGER_L, WPAD_BUTTON_MINUS },    { PAD_BUTTON_Y, WPAD_BUTTON_1 },
        { PAD_BUTTON_START, WPAD_BUTTON_HOME },
    };

    u32 down;
    do {
        if (power_pressed || reset_pressed)
            return WPAD_BUTTON_HOME;
        VIDEO_WaitVSync();
        WPAD_ScanPads();
        PAD_ScanPads();
        down = WPAD_ButtonsDown(0);
        u16 pad = PAD_ButtonsDown(0);
        for (int i = 0; i < sizeof gamecube / sizeof *gamecube; i++)
            if (pad & gamecube[i].pad)
                down |= gamecube[i].wpad;
    } while (!down);

    return down;
}

// A long name says what the WAD is; the short one is for cards whose wad folder has run out of
// room for long names, which libfat can't add then.
static void wad_file_names(const app *a, const char *id, console target,
                           char names[2][WAD_NAME_MAX])
{
    char name[APP_NAME_MAX];
    int n = 0;
    for (const char *p = a->name; *p; p++)
        name[n++] = strchr("/\\:*?\"<>|", *p) ? '-' : *p;
    name[n] = '\0';
    snprintf(names[0], WAD_NAME_MAX, "Empty Slot - %s - %s%s.wad", name, id,
             target == VWII ? " (vWii)" : "");
    snprintf(names[1], WAD_NAME_MAX, "ES%s%s.WAD", id, target == VWII ? "V" : "");
}

static bool write_file(const char *path, const u8 *data, u32 size)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool written = fwrite(data, 1, size, f) == size;
    if (fclose(f) == 0 && written)
        return true;
    remove(path);
    return false;
}

// Writes the WAD to sd:/wad (or usb:/wad without an SD card). Returns NULL, or what went wrong.
static const char *make_wad(const app *a, const char *id, console target, char *saved, int room)
{
    const u8 *template = target == VWII ? vwii_wad : wii_wad;
    u32 size = target == VWII ? vwii_wad_size : wii_wad_size;
    u8 *wad = memalign(32, size);
    if (!wad)
        return "out of memory";
    memcpy(wad, template, size);
    const char *error = wad_retarget(wad, size, a->path, id);

    char names[2][WAD_NAME_MAX];
    wad_file_names(a, id, target, names);
    for (int d = 0; !error && d < 2; d++) {
        const char *folder = d == 0 ? "sd:/wad" : "usb:/wad";
        mkdir(folder, 0777);
        for (int n = 0; n < 2; n++) {
            snprintf(saved, room, "%s/%.*s", folder, WAD_NAME_MAX - 1, names[n]);
            if (write_file(saved, wad, size)) {
                free(wad);
                return NULL;
            }
        }
    }
    free(wad);
    return error ? error : "couldn't write to sd:/wad or usb:/wad";
}

static void draw(int count, int selected, const char *id, console target, const char *status)
{
    printf("\x1b[2J\x1b[2;0H");
    printf("  Empty Slot Creator%*sfor: %s\n\n", 44 - (int)strlen(console_names[target]), "",
           console_names[target]);
    printf("  Pick a homebrew app. Its channel will look like an unused slot on the\n");
    printf("  Wii Menu, and pressing Start there opens the app.\n\n");

    if (count == 0)
        printf("  No apps found in sd:/apps or usb:/apps.\n");
    int top = selected - ROWS / 2;
    if (top > count - ROWS)
        top = count - ROWS;
    if (top < 0)
        top = 0;
    for (int i = top; i < top + ROWS; i++) {
        if (i < count)
            printf("  %s%-30.30s %-36.36s" NORMAL "\n", i == selected ? HIGHLIGHT : "",
                   apps[i].name, apps[i].path);
        else
            printf("\n");
    }
    if (count > ROWS)
        printf("%66s%d of %d\n", "", selected + 1, count);
    else
        printf("\n");

    if (count > 0)
        printf("  Channel ID: %s%s\n", id,
               installed(id) ? "  (installed already: installing again replaces it)" : "");
    printf("\n  %s\n\n", status);
    printf("  A: make the WAD   +/-: another ID   1: Wii or vWii   HOME: quit\n");
}

int main(int argc, char **argv)
{
    init_video();
    WPAD_Init();
    PAD_Init();
    SYS_SetPowerCallback(on_power);
    SYS_SetResetCallback(on_reset);
    WPAD_SetPowerButtonCallback(on_remote_power);
    fatInitDefault();
    load_installed_titles();

    console target = detect_console();
    int count = apps_scan(apps, MAX_APPS, argc > 0 ? argv[0] : NULL);
    int selected = 0, variant = 0;
    char id[TITLE_ID_LENGTH + 1] = "", status[300] = "";
    if (target == UNKNOWN)
        snprintf(status, sizeof status,
                 "Couldn't check the console. Press 1 to choose Wii or vWii.");

    while (true) {
        if (count > 0)
            title_id_for(apps[selected].path, variant, id);
        draw(count, selected, id, target, status);

        u32 down = buttons_down();
        if (down & WPAD_BUTTON_HOME)
            break;

        if (down & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN | WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT))
            variant = 0;
        if (count > 0 && down & WPAD_BUTTON_UP)
            selected = (selected + count - 1) % count;
        if (count > 0 && down & WPAD_BUTTON_DOWN)
            selected = (selected + 1) % count;
        if (count > 0 && down & WPAD_BUTTON_LEFT)
            selected = selected > ROWS ? selected - ROWS : 0;
        if (count > 0 && down & WPAD_BUTTON_RIGHT)
            selected = selected + ROWS < count ? selected + ROWS : count - 1;

        if (down & WPAD_BUTTON_PLUS)
            variant++;
        if (down & WPAD_BUTTON_MINUS)
            variant--;

        if (down & WPAD_BUTTON_1) {
            target = target == WII ? VWII : WII;
            status[0] = '\0';
        }

        if (count > 0 && down & WPAD_BUTTON_A) {
            char saved[WAD_NAME_MAX + 16];
            const char *error = NULL;
            if (target == UNKNOWN) {
                snprintf(status, sizeof status, "Press 1 to choose Wii or vWii first.");
                continue;
            }

            error = make_wad(&apps[selected], id, target, saved, sizeof saved);
            if (error)
                snprintf(status, sizeof status, "Couldn't make the WAD: %s.", error);
            else
                snprintf(status, sizeof status,
                         WII_BLUE "Saved %s" NORMAL
                         "\n  Install it with a WAD manager such as YAWMM ModMii Edition.",
                         saved);
        }
    }

    if (power_pressed)
        SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    return 0;
}
