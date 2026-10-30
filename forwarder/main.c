#include <fat.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// retarget.py finds this marker and rewrites the path after it, so it stays plain text.
#define MARKER "FWDPATH="
static char target[256] __attribute__((used)) = MARKER "/apps/usbloader_gx/boot.dol";

#define USB_TRIES 20

static bool usb_mounted;

typedef struct {
    u32 text_off[7], data_off[11];
    u32 text_addr[7], data_addr[11];
    u32 text_size[7], data_size[11];
    u32 bss_addr, bss_size, entry;
} dol_header;

typedef struct {
    u8 ident[16];
    u16 type, machine;
    u32 version, entry, phoff, shoff, flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf_header;

typedef struct {
    u32 type, offset, vaddr, paddr, filesz, memsz, flags, align;
} elf_segment;

typedef struct {
    u32 dst, src, size, zeroed;
} segment;

typedef struct {
    u32 entry, count;
    struct __argv args;
    segment seg[18];
} plan;

#define STUB_ADDR 0x93000000
#define STUB_SIZE 0x400

/* Runs from MEM2 after being copied there, so it may overwrite this program.
   It must call nothing and touch no globals to stay position-independent. */
static void __attribute__((noinline, optimize("no-tree-loop-distribute-patterns")))
boot_stub(const plan *p)
{
    for (u32 i = 0; i < p->count; i++) {
        const segment *s = &p->seg[i];
        u8 *dst = (u8 *)s->dst;
        const u8 *src = (const u8 *)s->src;
        for (u32 j = 0; j < s->zeroed; j++)
            dst[j] = j < s->size ? src[j] : 0;
        for (u32 j = 0; j < s->zeroed; j += 32)
            asm volatile("dcbf 0,%0; icbi 0,%0" ::"r"(dst + j));
    }
    u32 *magic = (u32 *)(p->entry + 4);
    if (*magic == ARGV_MAGIC) {
        u32 *dst = magic + 1;
        const u32 *src = (const u32 *)&p->args;
        for (u32 j = 0; j < sizeof(p->args) / 4; j++)
            dst[j] = src[j];
        asm volatile("dcbf 0,%0" ::"r"(dst));
    }
    asm volatile("sync; isync");
    ((void (*)(void))p->entry)();
}

static void add(plan *p, u32 addr, const u8 *src, u32 size, u32 zeroed)
{
    p->seg[p->count++] = (segment){ (addr & 0x3FFFFFFF) | 0x80000000, (u32)src, size, zeroed };
}

static void plan_dol(plan *p, const u8 *file)
{
    const dol_header *h = (const dol_header *)file;
    for (int i = 0; i < 7; i++)
        if (h->text_size[i])
            add(p, h->text_addr[i], file + h->text_off[i], h->text_size[i], h->text_size[i]);
    for (int i = 0; i < 11; i++)
        if (h->data_size[i])
            add(p, h->data_addr[i], file + h->data_off[i], h->data_size[i], h->data_size[i]);
    p->entry = h->entry;
}

static void plan_elf(plan *p, const u8 *file)
{
    const elf_header *h = (const elf_header *)file;
    for (int i = 0; i < h->phnum && p->count < 18; i++) {
        const elf_segment *s = (const elf_segment *)(file + h->phoff + i * h->phentsize);
        if (s->type == 1 && s->memsz)
            add(p, s->paddr, file + s->offset, s->filesz, s->memsz);
    }
    p->entry = h->entry;
}

// Read into MEM2 so the copy never overlaps where the app lands in MEM1.
static u8 *read_file(const char *path, u32 *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *buf = (u8 *)(((u32)SYS_GetArena2Lo() + 31) & ~31);
    SYS_SetArena2Lo(buf + *size);
    u32 got = fread(buf, 1, *size, f);
    fclose(f);
    return got == *size ? buf : NULL;
}

static u8 *find_app(const char *rel, char *full, u32 *size)
{
    static const char *const devices[] = { "sd", "usb" };
    char alt[256];
    strcpy(alt, rel);
    char *ext = strrchr(alt, '.');
    if (ext && !strcmp(ext, ".dol"))
        strcpy(ext, ".elf");

    for (int d = 0; d < 2; d++) {
        if (d == 0 && !fatMountSimple("sd", &__io_wiisd))
            continue;
        if (d == 1) {
            int tries = 0;
            // Hard drives can take a few seconds to spin up after the Menu lets go.
            while (!fatMountSimple("usb", &__io_usbstorage) && ++tries < USB_TRIES)
                usleep(250000);
            if (tries == USB_TRIES)
                break;
            usb_mounted = true;
        }
        for (const char *p = rel; p; p = (p == rel && strcmp(alt, rel)) ? alt : NULL) {
            sprintf(full, "%s:%s", devices[d], p);
            u8 *file = read_file(full, size);
            if (file)
                return file;
        }
    }
    return NULL;
}

static void fail(const char *rel)
{
    VIDEO_Init();
    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    void *fb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
    console_init(fb, 20, 20, mode->fbWidth, mode->xfbHeight, mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(fb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    printf("\n\n    Couldn't find %s on SD or USB.\n    Returning to the Wii Menu...", rel);
    sleep(5);
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

int main(void)
{
    const char *rel = target + strlen(MARKER);
    char full[300];
    u32 size;
    u8 *file = find_app(rel, full, &size);
    if (!file)
        fail(rel);

    plan *p = (plan *)(((u32)SYS_GetArena2Hi() - 0x1000) & ~31);
    memset(p, 0, sizeof(*p));
    if (!memcmp(file, "\x7F" "ELF", 4))
        plan_elf(p, file);
    else
        plan_dol(p, file);

    // Hand over the path as argv[0], the way the Homebrew Channel does; apps find their folder
    // by it.
    char *cmd = (char *)(p + 1);
    int len = strlen(full) + 2;
    memset(cmd, 0, len);
    strcpy(cmd, full);
    p->args = (struct __argv){ ARGV_MAGIC, cmd, len, 0, NULL, NULL };
    DCFlushRange(p, sizeof(*p) + len);

    memcpy((void *)STUB_ADDR, boot_stub, STUB_SIZE);
    DCFlushRange((void *)STUB_ADDR, STUB_SIZE);
    ICInvalidateRange((void *)STUB_ADDR, STUB_SIZE);

    fatUnmount("sd:");
    fatUnmount("usb:");
    __io_wiisd.shutdown();
    if (usb_mounted)
        __io_usbstorage.shutdown();
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    u32 level __attribute__((unused));
    _CPU_ISR_Disable(level);
    ((void (*)(const plan *))STUB_ADDR)(p);
    return 0;
}
