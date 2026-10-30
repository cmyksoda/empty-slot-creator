#include <openssl/evp.h>
#include <openssl/sha.h>

#include "../source/crypto.h"

static int cbc(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
               uint32_t size, int encrypt)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int written, ok = ctx && EVP_CipherInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv, encrypt) &&
                      EVP_CIPHER_CTX_set_padding(ctx, 0) &&
                      EVP_CipherUpdate(ctx, out, &written, in, size);
    EVP_CIPHER_CTX_free(ctx);
    return ok ? 0 : -1;
}

int aes_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
                    uint32_t size)
{
    return cbc(key, iv, in, out, size, 0);
}

int aes_cbc_encrypt(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
                    uint32_t size)
{
    return cbc(key, iv, in, out, size, 1);
}

int sha1_hash(const uint8_t *data, uint32_t size, uint8_t digest[20])
{
    SHA1(data, size, digest);
    return 0;
}
