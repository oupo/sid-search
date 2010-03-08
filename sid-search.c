#include <stdarg.h>
#include "common.c"

static const int SEARCH_MAX = 100;

static int search_mt_seed(uint *result, FILE *fp, uint search_value, CACHE_ENTRY *cache) {
	uint left = 0, right = table_length - 1;
	int n = 0;
	uint mid;
	uint node_pos = 0;
	while (left <= right) {
		uint seed, val;
		mid = (left + right) / 2;
		if (node_pos < cache_size) {
			seed = cache[node_pos].seed;
			val = cache[node_pos].val;
		} else {
			seed = fetch_table(fp, mid);
			val = get_first_mt_result(seed);
		}
		node_pos = node_pos * 2 + 1;
		if (val == search_value) {
			result[n++] = seed;
			break;
		} else if (val < search_value) {
			left = mid + 1;
			node_pos += 1;
		} else {
			if (mid == 0) break;
			right = mid - 1;
		}
	}
	if (n > 0) {
		uint i;
		for (i = mid - 1; ; i --) {
			uint seed = fetch_table(fp, i);
			if (get_first_mt_result(seed) != search_value) {
				break;
			}
			if (i == 0) break;
			result[n++] = seed;
		}
		for (i = mid + 1; i < table_length; i ++) {
			uint seed = fetch_table(fp, i);
			if (get_first_mt_result(seed) != search_value) {
				break;
			}
			result[n++] = seed;
		}
	}
	return n;
}

static uint prev_higawari_seed(uint seed) {
	return seed * 0x9638806d + 0x69c77f93;
}

static CACHE_ENTRY *load_cache(void) {
	CACHE_ENTRY *cache = malloc(sizeof(CACHE_ENTRY) * cache_size);
	FILE *fp = fopen(path_cache, "rb");
	fread(cache, sizeof(CACHE_ENTRY), cache_size, fp);
	fclose(fp);
	return cache;
}

typedef struct {
	char *ptr;
	size_t len;
	size_t capa;
} STR_BUILDER;

static void init_builder(STR_BUILDER *builder) {
	builder->capa = 256;
	builder->len = 0;
	builder->ptr = malloc(builder->capa);
	builder->ptr[0] = '\0';
}

static void bprintf(STR_BUILDER *builder, const char *fmt, ...) {
	va_list args;
	
	for (;;) {
		size_t space = builder->capa - builder->len;
		int n;
		va_start(args, fmt);
		n = vsnprintf(builder->ptr + builder->len, space, fmt, args);
		va_end(args);
		if (0 <= n && (size_t)n < space) {
			builder->len += n;
			return;
		}
		if (0 <= n) {
			while (builder->capa - builder->len < (size_t)n + 1) {
				builder->capa *= 2;
			}
		} else {
			builder->capa *= 2;
		}
		builder->ptr = realloc(builder->ptr, builder->capa);
	}
}

static char *search_sid(uint higawariseed, uint tid, int search_max) {
	int i;
	FILE *fp = fopen(path_table, "rb");
	CACHE_ENTRY *cache = load_cache();
	STR_BUILDER b;
	init_builder(&b);
	bprintf(&b, "result:");
	for (i = 0; i < search_max; i ++) {
		uint seeds[8];
		int j, n;
		n = search_mt_seed(seeds, fp, higawariseed, cache);
		for (j = 0; j < n; j ++) {
			uint seed = seeds[j];
			uint trainer_id = get_second_mt_result(seed);
			if ((trainer_id & 0xffff) == tid) {
				bprintf(&b, "%d %.8x %.4x,", i, seed, trainer_id >> 16);
			}
		}
		higawariseed = prev_higawari_seed(higawariseed);
	}
	bprintf(&b, "\n");
	bprintf(&b, "next:%d %.8x\n", i, higawariseed);
	fclose(fp);
	free(cache);
	return b.ptr;
}

static void search_sample(void) {
	uint result[8];
	FILE *fp = fopen(path_table, "rb");
	int i, n;
	CACHE_ENTRY *cache = load_cache();
	n = search_mt_seed(result, fp, 0x8c7f0aac, cache);
	printf("n = %d\n", n);
	for (i = 0; i < n; i ++) {
		printf("0x%.8" PRIx32 "\n", result[i]);
	}
}

static void search_test(void) {
	uint seed = 0;
	FILE *fp = fopen(path_table, "rb");
	CACHE_ENTRY *cache = load_cache();
	for (seed = 0; seed < 0x10000; seed ++) {
		uint val = get_first_mt_result(seed);
		int i, n;
		uint result[8];
		n = search_mt_seed(result, fp, val, cache);
		for (i = 0; i < n; i ++) {
			if (result[i] == seed) break;
		}
		printf("\r0x%.8" PRIx32, seed);
		fflush(stdout);
		if (i == n) {
			printf("\rnot found: 0x%.8" PRIx32 "\n", seed);
		}
	}
	printf("\nfinish\n");
}

static void parse_query(char *query, uint *seed, uint *tid, int *search_max) {
	char *s = query, *s2;
	*seed = 0, *tid = 0; *search_max = SEARCH_MAX;
	for (;;) {
		s2 = strchr(s, '=');
		if (!s2) return;
		if (strncmp(s, "seed", s2 - s) == 0) {
			s = s2 + 1;
			*seed = strtoul(s, &s, 16);
		} else if (strncmp(s, "tid", s2 - s) == 0) {
			s = s2 + 1;
			*tid = strtoul(s, &s, 16);
		} else if (strncmp(s, "search_max", s2 - s) == 0) {
			s = s2 + 1;
			*search_max = strtoul(s, &s, 10);
		}
		s += strcspn(s, "&;");
		if (*s == '\0') return;
		s ++;
	}
}

int main(void) {
	char *buf;
	char *query = getenv("QUERY_STRING");
	if (!query) query = "";
	uint higawariseed, tid;
	int search_max;
	parse_query(query, &higawariseed, &tid, &search_max);
	if (search_max > SEARCH_MAX) search_max = SEARCH_MAX;
	
	buf = search_sid(higawariseed, tid, search_max);
	printf("Content-Type: text/plain\r\n");
	printf("Content-Length: %u\r\n\r\n", strlen(buf));
	printf("%s", buf);
	free(buf);
	return 0;
}


