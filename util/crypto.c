
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdint.h>


#include "log.h"
#include "../cryptoc.h"
#include "crypto.h"

void md5(uint8_t* input, uint32_t len, uint8_t* digest)
{ 
    void* self = new(Md5);
    
    MD5_CalculateDigest(self, input, len, digest);

    delete(self);

    return;
}

static int aes128ecb_init(struct cryptor* self, char* passwd, int passwdlen)
{
    self->engine = new(Aes, AES_TYPE_128);

    uint8_t key[16];

    /* calc password's md5 value */
    md5(passwd, passwdlen, key);

    BlockCipher_SetKey(self->engine, key);

    BlockCipher_SetMode(self->engine, BLOCKCIPHER_MODE_ECB);

    return 0;
}

static struct cryptor_templete cryptor_templetes[] = 
{
	{
		.id = AES128_ECB,
        .keysize = 16,
		.init = aes128ecb_init,
	},
};

#define CRYPTOR_ARRAY_LEN (sizeof(cryptor_templetes)/sizeof(cryptor_templetes[0]))

static struct cryptor_templete* cryptor_templete_lookup(int theme)
{
    struct cryptor_templete* templete = NULL;

    for (int i = 0; i < CRYPTOR_ARRAY_LEN; ++i)
    {
        if (cryptor_templetes[i].id == theme)
        {
            templete = &cryptor_templetes[i];
        }
    }

    return templete;
}

int cryptor_init(struct cryptor* self, int theme, char* passwd, int passwdlen)
{

    struct cryptor_templete* templete = NULL;

    templete = cryptor_templete_lookup(theme);
    if (!templete)
    {
        uvpn_syslog(LOG_ERR,"can't find crypto theme %d", theme);   
        return -1;
    }

    templete->init(self, passwd, passwdlen);

    return 0;
}

void cryptor_fini(struct cryptor* self)
{
    if (self->engine)
    {
        delete(self->engine);
    }
    
    return;
}

int cryptor_encrypt(struct cryptor* self, uint8_t* data, int inlen, int* outlen)
{
    uint8_t buf[CRYPTO_MAX_SIZE];

    memset((void*)buf, 0, CRYPTO_MAX_SIZE);

    memcpy((void*)buf, (void*)data, inlen);

    return BlockCipher_Encryption(self->engine, buf, inlen, data, outlen);
}

int cryptor_decrypt(struct cryptor* self, uint8_t* data, int inlen, int* outlen)
{
    uint8_t buf[CRYPTO_MAX_SIZE];

    memset((void*)buf, 0, CRYPTO_MAX_SIZE);

    memcpy((void*)buf, (void*)data, inlen);

    return BlockCipher_Decryption(self->engine, buf, inlen, data, outlen);
}


void simple_encrypt(uint8_t* input, uint8_t* output, uint32_t inlen,
                    uint8_t* pwd, uint32_t pwdlen, uint32_t* outlen)
{
    void* aes;
    uint8_t digest[16];
    aes = new(Aes, AES_TYPE_128);

    md5(pwd, pwdlen, digest);

    BlockCipher_SetKey(aes, digest);
    BlockCipher_SetPad(aes, BLOCKCIPHER_PAD_PKCS7);
    BlockCipher_SetMode(aes, BLOCKCIPHER_MODE_ECB);

    BlockCipher_Encryption(aes, input, inlen, output, outlen);

    delete(aes);

    return;
}

void simple_decrypt(uint8_t* input, uint8_t* output, uint32_t inlen,
                    uint8_t* pwd, uint32_t pwdlen, uint32_t* outlen)
{
    void* aes;
    uint8_t digest[16];
    aes = new(Aes, AES_TYPE_128);

    md5(pwd, pwdlen, digest);

    BlockCipher_SetKey(aes, digest);
    BlockCipher_SetPad(aes, BLOCKCIPHER_PAD_PKCS7);
    BlockCipher_SetMode(aes, BLOCKCIPHER_MODE_ECB);

    BlockCipher_Decryption(aes, input, inlen, output, outlen);

    delete(aes);

    return;
}