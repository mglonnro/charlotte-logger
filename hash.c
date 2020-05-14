#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>

char           *
hashfile(char *filename, char *ret)
{
	unsigned char	c [MD5_DIGEST_LENGTH];
	int		i;
	FILE           *inFile = fopen(filename, "rb");
	MD5_CTX		mdContext;
	int		bytes;
	unsigned char	data[1024];

	if (inFile == NULL) {
		printf("%s can't be opened.\n", filename);
		return NULL;
	}

	MD5_Init(&mdContext);
	while ((bytes = fread(data, 1, 1024, inFile)) != 0)
		MD5_Update(&mdContext, data, bytes);
	MD5_Final(c, &mdContext);
	ret[0] = 0;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
		char		hex       [3];
		sprintf(hex, "%02x", c[i]);
		strcat(ret, hex);
	}
	printf(" %s\n", filename);
	fclose(inFile);

	return ret;
}
