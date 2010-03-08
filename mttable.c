#include "common.c"

static int compare_row(const ROW *row1, const ROW *row2) {
	if (row1->rand0 > row2->rand0) {
		return 1;
	} else if (row1->rand0 == row2->rand0) {
		return 0;
	} else {
		return -1;
	}
}

static int qsort_callback_row(const void *p1, const void *p2) {
	return compare_row((const ROW *)p1, (const ROW *)p2);
}

static void write_result(void) {
	FILE *fp = fopen(path_result, "wb");
	uint i, j;
	for (i = 0; i < 256; i ++) {
		for (j = 0; j < 65536 * 24; j ++) {
			uint seed = i << 24 | j;
			ROW row;
			row.seed = seed;
			row.rand0 = get_first_mt_result(seed);
			fwrite(&row, SIZEOF_ROW, 1, fp);
		}
	};
	fclose(fp);
}

#define BUF_LEN 100000

static void prepare_sort(void) {
	FILE *rf = fopen(path_result, "rb");
	FILE *wf = fopen(path_sorted, "wb");
	while (1) {
		char buf[BUF_LEN * SIZEOF_ROW];
		size_t n = fread(buf, SIZEOF_ROW, BUF_LEN, rf);
		qsort(buf, n, SIZEOF_ROW, qsort_callback_row);
		fwrite(buf, SIZEOF_ROW, n, wf);
		if (n < BUF_LEN) break;
	}
	fclose(rf);
	fclose(wf);
}

typedef struct {
	FILE *fp;
	ROW last, current;
	int available;
} READER;

static void read_next(READER *reader) {
	size_t n;
	reader->last = reader->current;
	n = fread(&reader->current, SIZEOF_ROW, 1, reader->fp);
	reader->available = (n == 1);
}

static void init_reader(READER *reader, FILE *fp) {
	reader->fp = fp;
	memset(&reader->last, 0xff, sizeof(ROW));
	memset(&reader->current, 0xff, sizeof(ROW));
	reader->available = FALSE;
	read_next(reader);
}

static int end_of_run(READER *reader) {
	return !reader->available || compare_row(&reader->current, &reader->last) < 0;
}

static void copy_to(READER *reader, FILE *fp) {
	fwrite(&reader->current, SIZEOF_ROW, 1, fp);
	read_next(reader);
}

static void copy_run_to(READER *reader, FILE *fp) {
	do {
		copy_to(reader, fp);
	} while (!end_of_run(reader));
}

static void divide(void) {
	FILE *rf = fopen(path_sorted, "rb");
	FILE *wf1 = fopen(path_tmp1, "wb");
	FILE *wf2 = fopen(path_tmp2, "wb");
	READER reader;
	init_reader(&reader, rf);
	for (;;) {
		if (!reader.available) break;
		copy_run_to(&reader, wf1);
		if (!reader.available) break;
		copy_run_to(&reader, wf2);
	}
	fclose(rf);
	fclose(wf1);
	fclose(wf2);
}

static int merge(void) {
	int run_count = 0;
	FILE *rf1 = fopen(path_tmp1, "rb");
	FILE *rf2 = fopen(path_tmp2, "rb");
	FILE *wf = fopen(path_sorted, "wb");
	READER reader1, reader2;
	init_reader(&reader1, rf1);
	init_reader(&reader2, rf2);
	while (reader1.available && reader2.available) {
		for (;;) {
			READER *a, *b;
			if (compare_row(&reader1.current, &reader2.current) < 0) {
				a = &reader1, b = &reader2;
			} else {
				a = &reader2, b = &reader1;
			}
			copy_to(a, wf);
			if (end_of_run(a)) {
				copy_run_to(b, wf);
				break;
			}
		}
		run_count ++;
	}
	while (reader1.available) {
		copy_run_to(&reader1, wf);
		run_count ++;
	}
	while (reader2.available) {
		copy_run_to(&reader2, wf);
		run_count ++;
	}
	fclose(rf1);
	fclose(rf2);
	fclose(wf);
	printf("run_count = %d\n", run_count);
	return run_count;
}

static void merge_sort(void) {
	int run_count;
	do {
		divide();
		run_count = merge();
	} while (run_count != 1);
	remove(path_tmp1);
	remove(path_tmp2);
}

static void remove_rand0(void) {
	FILE *rf = fopen(path_sorted, "rb");
	FILE *wf = fopen(path_table, "wb");
	while (1) {
		ROW buf[BUF_LEN];
		uint seeds[BUF_LEN];
		size_t n = fread(buf, SIZEOF_ROW, BUF_LEN, rf);
		size_t i;
		for (i = 0; i < n; i ++) {
			seeds[i] = buf[i].seed;
		}
		fwrite(seeds, sizeof(uint), n, wf);
		if (n < BUF_LEN) break;
	}
	fclose(rf);
	fclose(wf);
}

static void gen_cache0(FILE *fp, CACHE_ENTRY *cache, uint left, uint right, uint node_pos) {
	uint mid;
	uint seed, val;
	if (node_pos >= cache_size) return;
	mid = (left + right) / 2;
	seed = fetch_table(fp, mid);
	val = get_first_mt_result(seed);
	cache[node_pos].seed = seed;
	cache[node_pos].val = val;
	gen_cache0(fp, cache, left, mid - 1, node_pos * 2 + 1);
	gen_cache0(fp, cache, mid + 1, right, node_pos * 2 + 1 + 1);
}

static void gen_cache(void) {
	FILE *rf, *wf;
	rf = fopen(path_table, "rb");
	CACHE_ENTRY *cache = malloc(sizeof(CACHE_ENTRY) * cache_size);
	gen_cache0(rf, cache, 0, table_length - 1, 0);
	fclose(rf);
	wf = fopen(path_cache, "wb");
	fwrite(cache, sizeof(CACHE_ENTRY), cache_size, wf);
	fclose(wf);
	free(cache);
}

int main(void) {
	printf("phase 1\n");
	write_result();
	printf("phase 2\n");
	prepare_sort();
	printf("phase 3\n");
	merge_sort();
	printf("phase 4\n");
	remove_rand0();
	printf("phase 5\n");
	gen_cache();
	printf("finish\n");
	return 0;
}
