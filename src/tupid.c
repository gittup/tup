#include "tupid.h"
#include "mozilla-sha1/sha1.h"
#include <string.h>

const char *tupid_from_filename(tupid_t tupid, const char *filename)
{
	unsigned char hash[SHA1_HASH_SIZE];
	unsigned int x;
	SHA_CTX ctx;

	if(filename[0] && filename[1] && memcmp(filename, "./", 2) == 0)
		filename += 2;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, filename, strlen(filename));
	SHA1_Final(hash, &ctx);

	for(x=0; x<sizeof(hash); x++) {
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
	return filename;
}
