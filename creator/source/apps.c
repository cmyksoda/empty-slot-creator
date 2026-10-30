#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "apps.h"

static const char *const devices[] = { "sd", "usb" };
static const char *const executables[] = { "boot.dol", "boot.elf" };

static const struct {
    const char *escape;
    char text;
} entities[] = {
    { "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' }, { "&quot;", '"' }, { "&apos;", '\'' },
};

static bool is_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// Copies XML text into `out`, undoing escapes. The console font is ASCII only, so each other
// character becomes one '?'.
static void plain_text(const char *from, const char *to, char *out, int room)
{
    int n = 0;
    while (from < to && n < room - 1) {
        unsigned char c = *from;
        bool escaped = false;
        for (int i = 0; c == '&' && i < sizeof entities / sizeof *entities; i++) {
            int length = strlen(entities[i].escape);
            if (!strncmp(from, entities[i].escape, length)) {
                out[n++] = entities[i].text;
                from += length;
                escaped = true;
                break;
            }
        }
        if (escaped)
            continue;
        if (c >= 0x80 && c < 0xC0) {  // the rest of a UTF-8 character already shown as '?'
            from++;
            continue;
        }
        out[n++] = c < ' ' ? ' ' : c >= 0x80 ? '?' : c;
        from++;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = '\0';
}

static bool meta_name(const char *folder, char *name)
{
    char path[APP_PATH_MAX + 16], xml[4096];
    snprintf(path, sizeof path, "%s/meta.xml", folder);
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    size_t length = fread(xml, 1, sizeof xml - 1, f);
    fclose(f);
    xml[length] = '\0';

    char *start = strstr(xml, "<name>");
    char *end = start ? strstr(start, "</name>") : NULL;
    if (!end)
        return false;
    start += strlen("<name>");
    while (start < end && (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t'))
        start++;
    plain_text(start, end, name, APP_NAME_MAX);
    return name[0] != '\0';
}

static bool listed(const app *apps, int count, const char *path)
{
    for (int i = 0; i < count; i++)
        if (!strcasecmp(apps[i].path, path))
            return true;
    return false;
}

static int by_name(const void *a, const void *b)
{
    return strcasecmp(((const app *)a)->name, ((const app *)b)->name);
}

int apps_scan(app *apps, int max, const char *self)
{
    int count = 0;
    for (int d = 0; d < sizeof devices / sizeof *devices; d++) {
        char root[16];
        snprintf(root, sizeof root, "%s:/apps", devices[d]);
        DIR *dir = opendir(root);
        if (!dir)
            continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) && count < max) {
            char folder[APP_PATH_MAX];
            if (entry->d_name[0] == '.' ||
                snprintf(folder, sizeof folder, "%s/%s", root, entry->d_name) >= sizeof folder)
                continue;
            int folder_length = strlen(folder);
            if (self && !strncasecmp(self, folder, folder_length) && self[folder_length] == '/')
                continue;

            for (int e = 0; e < sizeof executables / sizeof *executables; e++) {
                char file[APP_PATH_MAX + 16];
                snprintf(file, sizeof file, "%s/%s", folder, executables[e]);
                if (!is_file(file))
                    continue;
                app *a = &apps[count];
                int length = snprintf(a->path, sizeof a->path, "/apps/%s/%s", entry->d_name,
                                      executables[e]);
                if (length < sizeof a->path && !listed(apps, count, a->path)) {
                    if (!meta_name(folder, a->name))
                        snprintf(a->name, sizeof a->name, "%.*s", APP_NAME_MAX - 1, entry->d_name);
                    count++;
                }
                break;
            }
        }
        closedir(dir);
    }
    qsort(apps, count, sizeof *apps, by_name);
    return count;
}
