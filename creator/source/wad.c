#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "wad.h"

// The same steps as retarget.py, so both make byte-identical WADs.
#define MARKER "FWDPATH="
#define FIELD 256
#define WAD_ALIGN 64
#define BLOCK 16
#define CONTENT_RECORD 36
#define CRYPTO_FAILED "the console's AES or SHA-1 engine failed"

static const uint8_t common_key[16] = {
    0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7,
};

typedef struct {
    uint8_t *tik, *tmd, *contents;
    uint32_t tik_size, tmd_size;
} wad_parts;

static uint32_t align(uint32_t n, uint32_t to)
{
    return (n + to - 1) / to * to;
}

static uint16_t get16(const uint8_t *p)
{
    return p[0] << 8 | p[1];
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = value >> 8;
    p[1] = value;
}

static const char *split(uint8_t *wad, uint32_t size, wad_parts *w)
{
    if (size < 0x20 || get32(wad + 4) != 0x49730000)
        return "the template isn't a WAD";

    uint8_t *part[4];
    uint32_t part_size[4], at = align(get32(wad), WAD_ALIGN);
    for (int i = 0; i < 4; i++) {
        part[i] = wad + at;
        part_size[i] = get32(wad + 8 + i * 4);
        at = align(at + part_size[i], WAD_ALIGN);
    }
    if (at + get32(wad + 0x18) > size)
        return "the template is cut short";

    *w = (wad_parts){ part[2], part[3], wad + at, part_size[2], part_size[3] };
    return NULL;
}

static void title_key_iv(const uint8_t *tik, uint8_t iv[16])
{
    memset(iv, 0, 16);
    memcpy(iv, tik + 0x1DC, 8);
}

static uint8_t *find(uint8_t *data, uint32_t size, const char *text)
{
    uint32_t length = strlen(text);
    for (uint32_t i = 0; i + length <= size; i++)
        if (!memcmp(data + i, text, length))
            return data + i;
    return NULL;
}

// Patched IOS only checks that the signed part's SHA-1 starts with a zero byte, so an unused
// field is counted up until it does.
static int fakesign(uint8_t *blob, uint32_t size, uint32_t spare)
{
    uint8_t hash[20];
    memset(blob + 4, 0, 256);
    for (uint32_t value = 0; value < 0x10000; value++) {
        put16(blob + spare, value);
        if (sha1_hash(blob + 0x140, size - 0x140, hash))
            return 0;
        if (hash[0] == 0)
            return 1;
    }
    return 0;
}

static const char *set_title_id(wad_parts *w, const uint8_t key[16], const char *id)
{
    uint8_t iv[16];
    memcpy(w->tmd + 0x190, id, TITLE_ID_LENGTH);
    memcpy(w->tik + 0x1E0, id, TITLE_ID_LENGTH);
    // The title key is encrypted with the title ID as its IV, so it has to follow the new ID.
    title_key_iv(w->tik, iv);
    if (aes_cbc_encrypt(common_key, iv, key, w->tik + 0x1BF, 16))
        return CRYPTO_FAILED;

    if (!fakesign(w->tmd, w->tmd_size, 0x1D4) || !fakesign(w->tik, w->tik_size, 0x262))
        return "couldn't fakesign the WAD";
    return NULL;
}

static void patch_path(uint8_t *field, const char *path)
{
    uint8_t *text = field + strlen(MARKER);
    memset(text, 0, FIELD - strlen(MARKER));
    memcpy(text, path, strlen(path));
}

const char *wad_retarget(uint8_t *wad, uint32_t size, const char *path, const char *id)
{
    if (path[0] != '/' || strlen(path) >= FIELD - strlen(MARKER))
        return "the app's path is too long";
    if (strlen(id) != TITLE_ID_LENGTH)
        return "title IDs are 4 characters";

    wad_parts w;
    const char *error = split(wad, size, &w);
    if (error)
        return error;

    uint8_t key[16], iv[16];
    title_key_iv(w.tik, iv);
    if (aes_cbc_decrypt(common_key, iv, w.tik + 0x1BF, key, 16))
        return CRYPTO_FAILED;

    uint16_t count = get16(w.tmd + 0x1DE);
    if (w.tmd_size < 0x1E4 + count * CONTENT_RECORD)
        return "the template's TMD is cut short";

    // The forwarder is the boot content on a Wii, but sits behind a NAND loader on a vWii.
    uint8_t *content = w.contents;
    for (int i = 0; i < count; i++) {
        uint8_t *record = w.tmd + 0x1E4 + i * CONTENT_RECORD;
        uint32_t plain = get32(record + 12), stored = align(plain, BLOCK);
        if (content + stored > wad + size)
            return "the template is cut short";

        uint8_t content_iv[16] = { record[4], record[5] };
        uint8_t *clear = malloc(stored);
        if (!clear)
            return "out of memory";
        if (aes_cbc_decrypt(key, content_iv, content, clear, stored)) {
            free(clear);
            return CRYPTO_FAILED;
        }

        uint8_t *field = find(clear, plain, MARKER);
        if (field) {
            patch_path(field, path);
            int failed = sha1_hash(clear, plain, record + 16) ||
                         aes_cbc_encrypt(key, content_iv, clear, content, stored);
            free(clear);
            return failed ? CRYPTO_FAILED : set_title_id(&w, key, id);
        }
        free(clear);
        content += align(stored, WAD_ALIGN);
    }
    return "the template has no FWDPATH= field";
}
