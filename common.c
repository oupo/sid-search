#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define uint uint32_t
static const int TRUE = 1;
static const int FALSE = 0;

const char path_result[] = "result",
           path_sorted[] = "result-sorted",
           path_tmp1[]   = "tmp1",
           path_tmp2[]   = "tmp2",
           path_table[]  = "table",
           path_cache[]  = "cache";

typedef struct {
	uint seed;
	uint rand0;
} ROW;

#define SIZEOF_ROW (sizeof(ROW))

typedef struct {
	uint seed;
	uint val;
} CACHE_ENTRY;

static const uint table_length = 256 * 24 * 65536;

#define cache_depth 20
#define cache_size (1 << cache_depth) - 1

#define NEXT_MT_ELEM(a,i) (1812433253 * (a ^ (a >> 30)) + (i))

static uint genrand(uint mt0, uint mt1, uint mt397) {
	uint value;
	value = (mt0 & 0x80000000) | (mt1 & 0x7fffffff);
	value = mt397 ^ (value >> 1) ^ ((value & 1) ? 0x9908b0df : 0);
	value ^=  value >> 11;
	value ^= (value <<  7) & 0x9d2c5680;
	value ^= (value << 15) & 0xefc60000;
	value ^=  value >> 18;
	return value;
}

static void get_mt_result(uint seed, uint *ret1, uint *ret2) {
	uint mt, mt0, mt1, mt2, mt397, mt398;
	int i;
	mt0 = seed;
	mt1 = NEXT_MT_ELEM(mt0, 1);
	mt2 = NEXT_MT_ELEM(mt1, 2);
	mt = mt2;
	for (i = 3; i <= 397; i++) {
		mt = NEXT_MT_ELEM(mt, i);
	}
	mt397 = mt;
	mt398 = NEXT_MT_ELEM(mt, 398);
	
	*ret1 = genrand(mt0, mt1, mt397);
	*ret2 = genrand(mt1, mt2, mt398);
	return;
}

static uint get_first_mt_result(uint seed) {
	uint ret1, ret2;
	get_mt_result(seed, &ret1, &ret2);
	return ret1;
}

static uint get_second_mt_result(uint seed) {
	uint ret1, ret2;
	get_mt_result(seed, &ret1, &ret2);
	return ret2;
}

static uint fetch_table(FILE *fp, uint pos) {
	uint val;
	fseek(fp, pos * sizeof(uint), SEEK_SET);
	fread(&val, sizeof(uint), 1, fp);
	return val;
}
