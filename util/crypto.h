#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#define AES128_ECB  1

#define CRYPTO_MAX_SIZE  1024

struct cryptor
{
    void* engine;
};


struct cryptor_templete
{
    int id;
    int keysize;
    int (*init)(struct cryptor* cryptor, char* passwd, int passwdlen);
};


int cryptor_init(struct cryptor* self, int theme, char* passwd, int passwdlen);
void cryptor_fini(struct cryptor* self);
int cryptor_encrypt(struct cryptor* self, uint8_t* data, int inlen, int* outlen);
int cryptor_decrypt(struct cryptor* self, uint8_t* data, int inlen, int* outlen);
void simple_decrypt(uint8_t* input, uint8_t* output, uint32_t inlen,
                    uint8_t* pwd, uint32_t pwdlen, uint32_t* outlen);
void simple_encrypt(uint8_t* input, uint8_t* output, uint32_t inlen,
                    uint8_t* pwd, uint32_t pwdlen, uint32_t* outlen);
void md5(uint8_t* input, uint32_t len, uint8_t* digest);

#endif

