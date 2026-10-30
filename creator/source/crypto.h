#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>

// Each returns 0 on success. The console build uses IOS's /dev/aes and /dev/sha; the host test,
// OpenSSL.
int aes_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
                    uint32_t size);
int aes_cbc_encrypt(const uint8_t key[16], const uint8_t iv[16], const uint8_t *in, uint8_t *out,
                    uint32_t size);
int sha1_hash(const uint8_t *data, uint32_t size, uint8_t digest[20]);

#endif
