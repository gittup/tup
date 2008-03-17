#include "tupid.h"
#include "mozilla-sha1/sha1.h"
#include <string.h>

static void tupid_from_hash(tupid_t tupid, const unsigned char *hash);

void tupid_from_filename(tupid_t tupid, const char *path)
{
	unsigned char hash[SHA1_HASH_SIZE];
	SHA_CTX ctx;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, path, strlen(path));
	SHA1_Final(hash, &ctx);

	tupid_from_hash(tupid, hash);
}

static void tupid_from_hash(tupid_t tupid, const unsigned char *hash)
{
	unsigned int x;

	for(x=0; x<SHA1_HASH_SIZE; x++) {
		unsigned char c1 = (hash[x] & 0xf0) >> 4;
		unsigned char c2 = (hash[x] & 0x0f);

		if(c1 >= 10)
			c1 += 'a' - 10;
		else
			c1 += '0';
		if(c2 >= 10)
			c2 += 'a' - 10;
		else
			c2 += '0';

		tupid[x<<1] = c1;
		tupid[(x<<1) + 1] = c2;
	}
}
