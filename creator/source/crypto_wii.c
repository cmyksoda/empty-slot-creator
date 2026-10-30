#include <malloc.h>
#include <string.h>

#include <gccore.h>
#include <ogc/aes.h>
#include <ogc/sha.h>

#include "crypto.h"

// IOS reaches every buffer by DMA, so everything goes through aligned copies. It also writes the
// running IV back, which must not touch the caller's.
static u8 key_copy[16] ATTRIBUTE_ALIGN(32);
static u8 iv_copy[16] ATTRIBUTE_ALIGN(32);
static u8 digest_copy[20] ATTRIBUTE_ALIGN(32);

static int aes(bool encrypt, const uint8_t key[16], const uint8_t iv[16], const uint8_t *in,
               uint8_t *out, uint32_t size)
{
    static bool open;
    if (!open)
        open = AES_Init() >= 0;

    u8 *source = memalign(32, size), *result = memalign(32, size);
    s32 ret = -1;
    if (open && source && result) {
        memcpy(key_copy, key, 16);
        memcpy(iv_copy, iv, 16);
        memcpy(source, in, size);
        ret = encrypt ? AES_Encrypt(key_copy, 16, iv_copy, 16, source, result, size)
                      : AES_Decrypt(key_copy, 16, iv_copy, 16, source, result, size);
        if (ret >= 0)
            memcpy(out, result, size);
    }
    free(source);
    free(result);
    return ret < 0 ? -1 : 0;
}

int aes_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
                    uint32_t size)
{
    return aes(false, key, iv, in, out, size);
}

int aes_cbc_encrypt(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
                    uint32_t size)
{
    return aes(true, key, iv, in, out, size);
}

int sha1_hash(const uint8_t *data, uint32_t size, uint8_t digest[20])
{
    static bool open;
    if (!open)
        open = SHA_Init() >= 0;
    if (!open || SHA_Calculate(data, size, digest_copy) < 0)
        return -1;
    memcpy(digest, digest_copy, 20);
    return 0;
}
