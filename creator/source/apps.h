#ifndef APPS_H
#define APPS_H

#define APP_NAME_MAX 48
#define APP_PATH_MAX 160

typedef struct {
    char name[APP_NAME_MAX];  // from meta.xml, else the folder's name
    char path[APP_PATH_MAX];  // "/apps/<folder>/boot.dol" or boot.elf, without a device
} app;

// Fills `apps` from sd:/apps and usb:/apps, sorted by name, skipping the folder `self` lives in.
// Returns how many it found.
int apps_scan(app *apps, int max, const char *self);

#endif
