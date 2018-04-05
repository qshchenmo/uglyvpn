#include <stdint.h>
#include <openssl/rand.h>

void random_generator(uint8_t *buf, int size)
{
	RAND_bytes(buf, size);
	return;
}


