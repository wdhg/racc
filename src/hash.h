#ifndef RACC_HASH_H
#define RACC_HASH_H

#include "fixint.h"
#include <ctype.h>

struct md5_hash {
	u64 lo;
	u64 hi;
};

struct md5_hash md5(u8 *data, size_t len);

#endif
