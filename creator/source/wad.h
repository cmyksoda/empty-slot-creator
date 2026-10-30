#ifndef WAD_H
#define WAD_H

#include <stdint.h>

#define TITLE_ID_LENGTH 4

// Rewrites an Empty Slot WAD in place so its forwarder boots `path` (like "/apps/wiiflow/boot.dol")
// and the channel installs as 00010001-`id`. Returns NULL, or a message saying what went wrong.
const char *wad_retarget(uint8_t *wad, uint32_t size, const char *path, const char *id);

#endif
