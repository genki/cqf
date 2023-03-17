/*
 * ============================================================================
 *
 *        Authors:  Prashant Pandey <ppandey@cs.stonybrook.edu>
 *                  Rob Johnson <robj@vmware.com>   
 *
 * ============================================================================
 */

#define _GNU_SOURCE
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
//#include <openssl/rand.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sodium.h>

#include "include/zipf.h"
#include "include/gqf_wrapper.h"

typedef struct prng_state {
    unsigned int seed;
} prng_state_t;

void RAND_bytes(unsigned char *buf, int num)
{
  randombytes_buf(buf, num);
}

int initstate_r(unsigned int seed, char *statebuf, size_t statelen, prng_state_t *state) {
    if (statebuf == NULL || statelen < sizeof(prng_state_t)) {
        return -1;
    }
    state->seed = seed;
    return 0;
}

int random_r(prng_state_t *state, int32_t *result) {
    if (state == NULL || result == NULL) {
        return -1;
    }
    *result = rand_r(&state->seed);
    return 0;
}

#ifndef  USE_MYRANDOM
#define RFUN random
#define RSEED srandom
#else
#define RFUN myrandom
#define RSEED mysrandom

static unsigned int m_z = 1;
static unsigned int m_w = 1;
static void mysrandom (unsigned int seed) {
	m_z = seed;
	m_w = (seed<<16) + (seed >> 16);
}

static long myrandom()
{
	m_z = 36969 * (m_z & 65535) + (m_z >> 16);
	m_w = 18000 * (m_w & 65535) + (m_w >> 16);
	return ((m_z << 16) + m_w) % 0x7FFFFFFF;
}
#endif

static float tdiff (struct timeval *start, struct timeval *end) {
	return (end->tv_sec-start->tv_sec) +1e-6*(end->tv_usec - start->tv_usec);
}

/*
uint64_t aes_hash2(uint64_t x)
{
	const uint64_t round_keys[32] =
	{ // These were generated by hashing some randomly chosen files on my laptop
		0x795e15dc8136095f, 0x562371660e56b023,
		0x086bb301d2fb5e87, 0x1fe74f801c68d829,
		0x38a19379fd013357, 0x4a7ef2fca0f840f5,
		0x7d2a08bc58553aef, 0x092cfe1997ab8b53,
		0xd18a0c07dac143d4, 0x64e345ef125a576c,
		0x82807902d8211a1f, 0x6985dc4ddcdaf85d,
		0x2214ff750cf750af, 0xb574b4138eb8a37e,
		0x83e11205e8050dd5, 0x2d62b24118df61eb,
		0x8a16453f8f6b6fa1, 0x260c9e8491474d4f,
		0x06eb44d6042ca8ae, 0x43efbd457306b135,
		0xbfcb7ac89f346686, 0xd00362f30651d0d0,
		0x016d3080768968d5, 0x74b4c2e46ef801de,
		0xf623864a4396fe74, 0x9fc26ea69dad6067,
		0xd0eb2f4e08564d99, 0x408b357725ae0297,
		0xd19efb8e82d22151, 0x58c5ead61b7ecc15,
		0x14e904bc8de1c705, 0x1ef79cd4f487912d
	};
	__uint128_t *rks = (__uint128_t *)round_keys;
	uint64_t output;

	asm("movq       %[input],       %%xmm15;"
			"pxor       %[round_keys0], %%xmm15;"
			"aesenc     %[round_keys1], %%xmm15;"
			"aesenc     %[round_keys2], %%xmm15;"
			"aesenc     %[round_keys3], %%xmm15;"
			"aesenc     %[round_keys4], %%xmm15;"
			"aesenc     %[round_keys5], %%xmm15;"
			"aesenc     %[round_keys6], %%xmm15;"
			"aesenc     %[round_keys7], %%xmm15;"
			"aesenc     %[round_keys8], %%xmm15;"
			"aesenc     %[round_keys9], %%xmm15;"
			"aesenclast %[round_keysa], %%xmm15;"
			"vmovq      %%xmm15,        %[output]"
			: [output] "=irm" (output)
			: [input] "irm" (x),
			[round_keys0] "m" (rks[0]),
			[round_keys1] "m" (rks[1]),
			[round_keys2] "m" (rks[2]),
			[round_keys3] "m" (rks[3]),
			[round_keys4] "m" (rks[4]),
			[round_keys5] "m" (rks[5]),
			[round_keys6] "m" (rks[6]),
			[round_keys7] "m" (rks[7]),
			[round_keys8] "m" (rks[8]),
			[round_keys9] "m" (rks[9]),
			[round_keysa] "m" (rks[10])
				 : "xmm15"
					 );

	return output;
}*/

static __uint128_t* zipf_gen(long N, long gencount, double s) {
	int i;
	uint32_t *counts;
	__uint128_t *elems;
	struct timeval a,b,c;
	printf("Generating %ld elements in universe of %ld items with characteristic exponent %f\n",
				 gencount, N, s);
	gettimeofday(&a, NULL);
	ZIPFIAN z = create_zipfian(1, N, RFUN);
	counts = (uint32_t *)calloc(N, sizeof(counts));
	elems = (__uint128_t *)calloc(gencount, sizeof(elems));

	gettimeofday(&b, NULL);
	printf("Setup time    = %0.6fs\n", tdiff(&a, &b));
	for (i=0; i<gencount; i++) {
		long g = zipfian_gen(z);
		assert(0<=g && g<N);
		counts[g]++;
		elems[i] = g;
	}
	gettimeofday(&c, NULL);
	double rtime = tdiff(&b, &c);
	printf("Generate time = %0.6fs (%f per second)\n", rtime, gencount/rtime);
	if (0) {
		for (i=0; i<N; i++) {
			printf("%4.1f (%4.1f)\n", counts[0]/(double)counts[i], i/(counts[0]/(double)counts[i]));
			//	printf("%d ", counts[i]);
		}
		printf("\n");
	}
	destroy_zipfian(z);
	return elems;
}

typedef void * (*rand_init)(uint64_t maxoutputs, __uint128_t maxvalue, void *params);
typedef int (*gen_rand)(void *state, uint64_t noutputs, __uint128_t *outputs);
typedef void * (*duplicate_rand)(void *state);

typedef int (*init_op)(uint64_t nvals, uint64_t hash);
typedef int (*insert_op)(__uint128_t val, uint64_t count);
typedef int (*lookup_op)(__uint128_t val);
typedef __uint128_t (*get_range_op)();
typedef int (*destroy_op)();
typedef int (*iterator_op)(uint64_t pos);
typedef int (*iterator_get_op)(uint64_t *key, uint64_t *value, uint64_t *count);
typedef int (*iterator_next_op)();
typedef int (*iterator_end_op)();


typedef struct rand_generator {
	rand_init init;
	gen_rand gen;
	duplicate_rand dup;
} rand_generator;

typedef struct filter {
	init_op init;
	insert_op insert;
	lookup_op lookup;
	get_range_op range;
	destroy_op destroy;
	iterator_op iterator;
	iterator_get_op get;
	iterator_next_op next;
	iterator_end_op end;
} filter;

typedef struct uniform_pregen_state {
	uint64_t maxoutputs;
	uint64_t nextoutput;
	__uint128_t *outputs;
} uniform_pregen_state;

typedef struct uniform_online_state {
	uint64_t maxoutputs;
	uint64_t maxvalue;
	unsigned int seed;
	char *buf;
	int STATELEN;
	struct prng_state *rand_state;
} uniform_online_state;

typedef struct zipf_params {
	double exp;
	long universe;
	long sample;
} zipf_params;

typedef struct zipfian_pregen_state {
	zipf_params *params;
	uint64_t maxoutputs;
	uint64_t nextoutput;
	__uint128_t *outputs;
} zipfian_pregen_state;

typedef struct app_params {
	char *ip_file;
	int num;
} app_params;

typedef struct app_pregen_state {
	app_params *params;
	uint64_t maxoutputs;
	uint64_t nextoutput;
	__uint128_t *outputs;
} app_pregen_state;

__uint128_t *app_file_read(char *ip_file, int num)
{
	int i = 0, ch;
	__uint128_t *vals = (__uint128_t*)malloc(sizeof(__uint128_t)*num);
	FILE * input_values;
	
	input_values = fopen(ip_file,"r");
	if (input_values == NULL) {
		fprintf(stderr, "Error! Could not open file.\n");
	}

	ch = fscanf(input_values, "%ld", (uint64_t *)&vals[i++]);
	while (ch != EOF) {
		ch = fscanf(input_values, "%ld", (uint64_t *)&vals[i]);
		i++;
	}

	fclose(input_values);
	return vals;
}

void *app_pregen_init(uint64_t maxoutputs, __uint128_t maxvalue, void *params)
{
	uint32_t i;
	app_pregen_state *state = (app_pregen_state *)malloc(sizeof(app_pregen_state));
	assert(state != NULL);

	state->maxoutputs = maxoutputs;
	state->nextoutput = 0;
	state->params = (app_params *)params;
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		RSEED(tv.tv_sec + tv.tv_usec);
	}
	state->outputs = app_file_read(state->params->ip_file, state->params->num);
	assert(state->outputs != NULL);
	for (i = 0; i < state->params->num; i++)
		state->outputs[i] = (1 * state->outputs[i]) % maxvalue;

	return (void *)state;
}

int app_pregen_gen_rand(void *_state, uint64_t noutputs, __uint128_t *outputs)
{
	app_pregen_state *state = (app_pregen_state *)_state;
	assert(state->nextoutput + noutputs <= state->maxoutputs);
	memcpy(outputs, state->outputs+state->nextoutput, noutputs * sizeof(*state->outputs));
	state->nextoutput += noutputs;
	return noutputs;
}

void *app_pregen_duplicate(void *state)
{
	app_pregen_state *newstate = (app_pregen_state *)malloc(sizeof(*newstate));
	assert(newstate);
	memcpy(newstate, state, sizeof(*newstate));
	return newstate;
}

void *zipfian_pregen_init(uint64_t maxoutputs, __uint128_t maxvalue, void *params)
{
	uint32_t i;
	zipfian_pregen_state *state = (zipfian_pregen_state *)malloc(sizeof(zipfian_pregen_state));
	assert(state != NULL);

	state->maxoutputs = maxoutputs;
	state->nextoutput = 0;
	state->params = (zipf_params*)params;
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		RSEED(tv.tv_sec + tv.tv_usec);
	}
	state->outputs = zipf_gen(state->params->universe, state->params->sample, state->params->exp);
	assert(state->outputs != NULL);
	for (i = 0; i < state->maxoutputs; i++)
		state->outputs[i] = (1 * state->outputs[i]) % maxvalue;

	return (void *)state;
}

int zipfian_pregen_gen_rand(void *_state, uint64_t noutputs, __uint128_t *outputs)
{
	zipfian_pregen_state *state = (zipfian_pregen_state *)_state;
	assert(state->nextoutput + noutputs <= state->maxoutputs);
	memcpy(outputs, state->outputs+state->nextoutput, noutputs * sizeof(*state->outputs));
	state->nextoutput += noutputs;
	return noutputs;
}

void *zipfian_pregen_duplicate(void *state)
{
	zipfian_pregen_state *newstate = (zipfian_pregen_state *)malloc(sizeof(*newstate));
	assert(newstate);
	memcpy(newstate, state, sizeof(*newstate));
	return newstate;
}

void *uniform_pregen_init(uint64_t maxoutputs, __uint128_t maxvalue, void *params)
{
	uint32_t i;
	uniform_pregen_state *state = (uniform_pregen_state *)malloc(sizeof(uniform_pregen_state));
	assert(state != NULL);

	state->nextoutput = 0;

	state->maxoutputs = maxoutputs;
	state->outputs = (__uint128_t *)malloc(state->maxoutputs * sizeof(state->outputs[0]));
	assert(state->outputs != NULL);
	RAND_bytes((unsigned char *)state->outputs, sizeof(*state->outputs) * state->maxoutputs);
	for (i = 0; i < state->maxoutputs; i++)
		state->outputs[i] = (1 * state->outputs[i]) % maxvalue;

	return (void *)state;
}

int uniform_pregen_gen_rand(void *_state, uint64_t noutputs, __uint128_t *outputs)
{
	uniform_pregen_state *state = (uniform_pregen_state *)_state;
	assert(state->nextoutput + noutputs <= state->maxoutputs);
	memcpy(outputs, state->outputs+state->nextoutput, noutputs * sizeof(*state->outputs));
	state->nextoutput += noutputs;
	return noutputs;
}

void *uniform_pregen_duplicate(void *state)
{
	uniform_pregen_state *newstate = (uniform_pregen_state *)malloc(sizeof(*newstate));
	assert(newstate);
	memcpy(newstate, state, sizeof(*newstate));
	return newstate;
}

void *uniform_online_init(uint64_t maxoutputs, __uint128_t maxvalue, void *params)
{
	uniform_online_state *state = (uniform_online_state *)malloc(sizeof(uniform_online_state));
	assert(state != NULL);

	state->maxoutputs = maxoutputs;
	state->maxvalue = maxvalue;
	state->seed = time(NULL);
	state->STATELEN = 256;
	state->buf = (char *)calloc(256, sizeof(char));
	state->rand_state = (struct prng_state *)calloc(1, sizeof(struct prng_state));

	initstate_r(state->seed, state->buf, state->STATELEN, state->rand_state);
	return (void *)state;
}

int uniform_online_gen_rand(void *_state, uint64_t noutputs, __uint128_t *outputs)
{
	uint32_t i, j;
	uniform_online_state *state = (uniform_online_state *)_state;
	assert(state->rand_state != NULL);
	memset(outputs, 0, noutputs*sizeof(__uint128_t));
	for (i = 0; i < noutputs; i++) {
		int32_t result;
		for (j = 0; j < 4; j++) {
			random_r(state->rand_state, &result);
			outputs[i] = (outputs[i] * RAND_MAX) + result;
		}
		outputs[i] = (1 * outputs[i]) % state->maxvalue;
	}
	return noutputs;
}

void *uniform_online_duplicate(void *_state)
{
	uniform_online_state *newstate = (uniform_online_state *)malloc(sizeof(uniform_online_state));
	assert(newstate != NULL);
	uniform_online_state *oldstate = (uniform_online_state *)_state;

	newstate->maxvalue = oldstate->maxvalue;
	newstate->seed = oldstate->seed;
	newstate->STATELEN = oldstate->STATELEN;
	
	newstate->buf = (char *)calloc(256, sizeof(char));
	memcpy(newstate->buf, oldstate->buf, newstate->STATELEN);
	newstate->rand_state = (struct prng_state *)calloc(1, sizeof(struct prng_state));

	initstate_r(newstate->seed, newstate->buf, newstate->STATELEN, newstate->rand_state);
	return newstate;
}

rand_generator uniform_pregen = {
	uniform_pregen_init,
	uniform_pregen_gen_rand,
	uniform_pregen_duplicate
};

rand_generator uniform_online = {
	uniform_online_init,
	uniform_online_gen_rand,
	uniform_online_duplicate
};

rand_generator zipfian_pregen = {
	zipfian_pregen_init,
	zipfian_pregen_gen_rand,
	zipfian_pregen_duplicate
};

rand_generator app_pregen = {
	app_pregen_init,
	app_pregen_gen_rand,
	app_pregen_duplicate
};

filter gqf = {
	gqf_init,
	gqf_insert,
	gqf_lookup,
	gqf_range,
	gqf_destroy,
	gqf_iterator,
	gqf_get,
	gqf_next,
	gqf_end
};

void filter_multi_merge(filter qf_arr[], int nqf, filter qfr)
{
	int i;
	int flag = 0;
	int smallest_i = 0;
	uint64_t smallest_key = UINT64_MAX;
	for (i=0; i<nqf; i++) {
		qf_arr[i].iterator(0);
	}

	while (!flag) {
		uint64_t keys[nqf];
		uint64_t values[nqf];
		uint64_t counts[nqf];
		for (i=0; i<nqf; i++)
			qf_arr[i].get(&keys[i], &values[i], &counts[i]);
		
		do {
			smallest_key = UINT64_MAX;
			for (i=0; i<nqf; i++) {
				if (keys[i] < smallest_key) {
					smallest_key = keys[i]; smallest_i = i;
				}
			}
			qfr.insert(keys[smallest_i], counts[smallest_i]);
			qf_arr[smallest_i].next();
			qf_arr[smallest_i].get(&keys[smallest_i], &values[smallest_i], &counts[smallest_i]);
		} while(!qf_arr[smallest_i].end());

		/* remove the qf that is exhausted from the array */
		if (smallest_i < nqf-1)
			memmove(&qf_arr[smallest_i], &qf_arr[smallest_i+1], (nqf-smallest_i-1)*sizeof(qf_arr[0]));
		nqf--;
		if (nqf == 1)
			flag = 1;
	}
	if (!qf_arr[0].end()) {
		do {
			uint64_t key, value, count;
			qf_arr[0].get(&key, &value, &count);
			qfr.insert(key, count);
		} while(!qf_arr[0].next());
	}

	return;
}


uint64_t tv2msec(struct timeval tv)
{
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int cmp_uint64_t(const void *a, const void *b)
{
	const uint64_t *ua = (const uint64_t*)a, *ub = (const uint64_t *)b;
	return *ua < *ub ? -1 : *ua == *ub ? 0 : 1;
}

void usage(char *name)
{
	printf("%s [OPTIONS]\n"
				 "Options are:\n"
				 "  -n nslots     [ log_2 of filter capacity.  Default 22 ]\n"
				 "  -r nruns      [ number of runs.  Default 1 ]\n"
				 "  -p npoints    [ number of points on the graph.  Default 20 ]\n"
				 "  -m randmode   [ Data distribution, one of \n"
				 "                    uniform_pregen\n"
				 "                    uniform_online\n"
				 "                    zipfian_pregen\n"
				 "                    custom_pregen\n"
				 "                  Default uniform_pregen ]\n"
				 "  -d datastruct  [ Default gqf. ]\n"
				 "  -a number of filters for merging  [ Default 0 ] [Optional]\n"
				 "  -f outputfile  [ Default gqf. ]\n"
				 "  -i input file for app specific benchmark [Optional]\n"
				 "  -v num of values in the input file [Optional]\n"
				 "  -u universe for zipfian distribution  [ Default nvals ] [Optional]\n"
				 "  -s constant for zipfian distribution  [ Default 1.5 ] [Optional]\n",
				 name);
}

int main(int argc, char **argv)
{
	uint32_t nbits = 22, nruns = 1;
	unsigned int npoints = 20;
	uint64_t nslots = (1ULL << nbits), nvals = 950*nslots/1000;
	double s = 1.5; long universe = nvals;
	int numvals = 0;
	int numfilters = 0;
	char *randmode = "uniform_pregen";
	char *datastruct = "gqf";
	char *outputfile = "gqf";
	char *inputfile = "gqf";
	void *param = NULL;

	filter filter_ds;
	rand_generator *vals_gen;
	void *vals_gen_state;
	void *old_vals_gen_state;
	rand_generator *othervals_gen;
	void *othervals_gen_state;

	unsigned int i, j, exp, run;
	struct timeval tv_insert[100][1];
	struct timeval tv_exit_lookup[100][1];
	struct timeval tv_false_lookup[100][1];
	uint64_t fps = 0;

	const char *dir = "./";
	const char *insert_op = "-insert.txt\0";
	const char *exit_lookup_op = "-exists-lookup.txt\0";
	const char *false_lookup_op = "-false-lookup.txt\0";
	char filename_insert[256];
	char filename_exit_lookup[256];
	char filename_false_lookup[256];

	/* Argument parsing */
	int opt;
	char *term;

	while((opt = getopt(argc, argv, "n:r:p:m:d:a:f:i:v:s")) != -1) {
		switch(opt) {
			case 'n':
				nbits = strtol(optarg, &term, 10);
				if (*term) {
					fprintf(stderr, "Argument to -n must be an integer\n");
					usage(argv[0]);
					exit(1);
				}
				nslots = (1ULL << nbits);
				nvals = 950*nslots/1000;
				universe = nvals;
				break;
			case 'r':
				nruns = strtol(optarg, &term, 10);
				if (*term) {
					fprintf(stderr, "Argument to -r must be an integer\n");
					usage(argv[0]);
					exit(1);
				}
				break;
			case 'p':
				npoints = strtol(optarg, &term, 10);
				if (*term) {
					fprintf(stderr, "Argument to -p must be an integer\n");
					usage(argv[0]);
					exit(1);
				}
				break;
			case 'm':
				randmode = optarg;
				break;
			case 'd':
				datastruct = optarg;
				break;
			case 'f':
				outputfile = optarg;
				break;
			case 'i':
				inputfile = optarg;
				break;
			case 'v':
				numvals = (int)strtol(optarg, &term, 10);
				break;
			case 's':
				s = strtod(optarg, NULL);
				break;
			case 'u':
				universe = strtol(optarg, &term, 10);
				break;
			case 'a':
				numfilters = strtol(optarg, &term, 10);
				if (*term) {
					fprintf(stderr, "Argument to -p must be an integer\n");
					usage(argv[0]);
					exit(1);
				}
				break;
			default:
				fprintf(stderr, "Unknown option\n");
				usage(argv[0]);
				exit(1);
				break;
		}
	}

	if (strcmp(randmode, "uniform_pregen") == 0) {
		vals_gen = &uniform_pregen;
		othervals_gen = &uniform_pregen;
	} else if (strcmp(randmode, "uniform_online") == 0) {
		vals_gen = &uniform_online;
		othervals_gen = &uniform_online;
	} else if (strcmp(randmode, "zipfian_pregen") == 0) {
		vals_gen = &zipfian_pregen;
		othervals_gen = &uniform_pregen;
		param = (zipf_params *)malloc(sizeof(zipf_params));
		assert(param != NULL);
		if (s == 0 || universe == 0) {
			fprintf(stderr, "Unknown randmode.\n");
			usage(argv[0]);
			exit(1);
		}
		((zipf_params *)param)->exp = s;
		((zipf_params *)param)->universe = universe;
		((zipf_params *)param)->sample = nvals;
	} else if (strcmp(randmode, "app_pregen") == 0) {
		vals_gen = &app_pregen;
		othervals_gen = &app_pregen;
		param = (app_params *)malloc(sizeof(app_params));
		((app_params *)param)->ip_file = inputfile;
		((app_params *)param)->num = numvals;
		nvals = numvals;
	} else {
		fprintf(stderr, "Unknown randmode.\n");
		usage(argv[0]);
		exit(1);
	}

	if (strcmp(datastruct, "gqf") == 0) {
		filter_ds = gqf;
//	} else if (strcmp(datastruct, "qf") == 0) {
//		filter_ds = qf;
//	} else if (strcmp(datastruct, "cf") == 0) {
//		filter_ds = cf;
//	} else if (strcmp(datastruct, "bf") == 0) {
//		filter_ds = bf;
	} else {
		fprintf(stderr, "Unknown randmode.\n");
		usage(argv[0]);
		exit(1);
	}

	snprintf(filename_insert, strlen(dir) + strlen(outputfile) + strlen(insert_op) + 1, "%s%s%s", dir, outputfile, insert_op);
	snprintf(filename_exit_lookup, strlen(dir) + strlen(outputfile) + strlen(exit_lookup_op) + 1, "%s%s%s", dir, outputfile, exit_lookup_op);

	snprintf(filename_false_lookup, strlen(dir) + strlen(outputfile) + strlen(false_lookup_op) + 1, "%s%s%s", dir, outputfile, false_lookup_op);

	FILE *fp_insert = fopen(filename_insert, "w");
	FILE *fp_exit_lookup = fopen(filename_exit_lookup, "w");
	FILE *fp_false_lookup = fopen(filename_false_lookup, "w");

	if (fp_insert == NULL || fp_exit_lookup == NULL || fp_false_lookup == NULL) {
		printf("Can't open the data file");
		exit(1);
	}

	if (numfilters > 0) {
		uint64_t num_hash_bits = nbits+ ceil(numfilters/2) + 8;
		filter filters[numfilters];
		filter final_filter;
		rand_generator *generator[numfilters];
		void *generator_state[numfilters];

		// initialize all the filters and generators
		for (int i = 0; i < numfilters; i++) {
			filters[i] = gqf;
			filters[i].init(nbits, num_hash_bits);
			generator[i] = &uniform_online;
			generator_state[i] = generator[i]->init(nvals, filters[i].range(), param);
		}
		final_filter = gqf;
		final_filter.init(nbits+ceil(numfilters/2), num_hash_bits);

		// insert items in the filters
		__uint128_t *vals = (__uint128_t *)malloc((nvals/32)*sizeof(__uint128_t));
		for (int i = 0; i < numfilters; i++) {
			for (int k = 0; k < 32; k++) {
				memset(vals, 0, (nvals/32)*sizeof(__uint128_t));
				assert(generator[i]->gen(generator_state[i], nvals/32, vals) == nvals/32);
				for (uint32_t j = 0; j < nvals/32; j++) {
					filters[i].insert(vals[j], 1);
				}
			}
		}
		free(vals);

		gettimeofday(&tv_insert[0][0], NULL);
		filter_multi_merge(filters, numfilters, final_filter);
		gettimeofday(&tv_insert[1][0], NULL);

		printf("Insert Performance:\n");
		printf(" %f",
					 0.001 * (nvals*numfilters)/(tv2msec(tv_insert[1][0]) - tv2msec(tv_insert[0][0])));
		printf(" Million inserts per second\n");
	} else {

		for (run = 0; run < nruns; run++) {
			fps = 0;
			filter_ds.init(nbits, nbits+8);

			vals_gen_state = vals_gen->init(nvals, filter_ds.range(), param);
			old_vals_gen_state = vals_gen->dup(vals_gen_state);
			sleep(5);
			othervals_gen_state = othervals_gen->init(nvals, filter_ds.range(), param);

			for (exp = 0; exp < 2*npoints; exp += 2) {
				i = (exp/2)*(nvals/npoints);
				j = ((exp/2) + 1)*(nvals/npoints);
				printf("Round: %d\n", exp/2);

				gettimeofday(&tv_insert[exp][run], NULL);
				for (;i < j; i += 1<<16) {
					int nitems = j - i < 1<<16 ? j - i : 1<<16;
					__uint128_t vals[1<<16];
					int m;
					assert(vals_gen->gen(vals_gen_state, nitems, vals) == nitems);

					for (m = 0; m < nitems; m++) {
						filter_ds.insert(vals[m], 1);
					}
				}
				gettimeofday(&tv_insert[exp+1][run], NULL);

				i = (exp/2)*(nvals/npoints);
				gettimeofday(&tv_exit_lookup[exp][run], NULL);
				for (;i < j; i += 1<<16) {
					int nitems = j - i < 1<<16 ? j - i : 1<<16;
					__uint128_t vals[1<<16];
					int m;
					assert(vals_gen->gen(old_vals_gen_state, nitems, vals) == nitems);
					for (m = 0; m < nitems; m++) {
						if (!filter_ds.lookup(vals[m])) {
							fprintf(stderr,
											"Failed lookup for 0x%lx%016lx\n",
											(uint64_t)(vals[m]>>64),
											(uint64_t)(vals[m] & 0xffffffffffffffff));
							abort();
						}
					}
				}
				gettimeofday(&tv_exit_lookup[exp+1][run], NULL);

				i = (exp/2)*(nvals/npoints);
				gettimeofday(&tv_false_lookup[exp][run], NULL);
				for (;i < j; i += 1<<16) {
					int nitems = j - i < 1<<16 ? j - i : 1<<16;
					__uint128_t othervals[1<<16];
					int m;
					assert(othervals_gen->gen(othervals_gen_state, nitems, othervals) == nitems);
					for (m = 0; m < nitems; m++) {
						fps += filter_ds.lookup(othervals[m]);
					}
				}
				gettimeofday(&tv_false_lookup[exp+1][run], NULL);
			}
			filter_ds.destroy();
		}

		printf("Wiring results to file: %s\n", filename_insert);
		fprintf(fp_insert, "x_0");
		for (run = 0; run < nruns; run++) {
			fprintf(fp_insert, "    y_%d", run);
		}
		fprintf(fp_insert, "\n");
		for (exp = 0; exp < 2*npoints; exp += 2) {
			fprintf(fp_insert, "%d", ((exp/2)*(100/npoints)));
			for (run = 0; run < nruns; run++) {
				fprintf(fp_insert, " %f",
								0.001 * (nvals/npoints)/(tv2msec(tv_insert[exp+1][run]) - tv2msec(tv_insert[exp][run])));
			}
			fprintf(fp_insert, "\n");
		}
		printf("Insert Performance written\n");

		printf("Wiring results to file: %s\n", filename_exit_lookup);
		fprintf(fp_exit_lookup, "x_0");
		for (run = 0; run < nruns; run++) {
			fprintf(fp_exit_lookup, "    y_%d", run);
		}
		fprintf(fp_exit_lookup, "\n");
		for (exp = 0; exp < 2*npoints; exp += 2) {
			fprintf(fp_exit_lookup, "%d", ((exp/2)*(100/npoints)));
			for (run = 0; run < nruns; run++) {
				fprintf(fp_exit_lookup, " %f",
								0.001 * (nvals/npoints)/(tv2msec(tv_exit_lookup[exp+1][run]) - tv2msec(tv_exit_lookup[exp][run])));
			}
			fprintf(fp_exit_lookup, "\n");
		}
		printf("Existing Lookup Performance written\n");

		printf("Wiring results to file: %s\n", filename_false_lookup);
		fprintf(fp_false_lookup, "x_0");
		for (run = 0; run < nruns; run++) {
			fprintf(fp_false_lookup, "    y_%d", run);
		}
		fprintf(fp_false_lookup, "\n");
		for (exp = 0; exp < 2*npoints; exp += 2) {
			fprintf(fp_false_lookup, "%d", ((exp/2)*(100/npoints)));
			for (run = 0; run < nruns; run++) {
				fprintf(fp_false_lookup, " %f",
								0.001 * (nvals/npoints)/(tv2msec(tv_false_lookup[exp+1][run]) - tv2msec(tv_false_lookup[exp][run])));
			}
			fprintf(fp_false_lookup, "\n");
		}
		printf("False Lookup Performance written\n");

		printf("FP rate: %f (%lu/%lu)\n", 1.0 * fps / nvals, fps, nvals);
	}
	fclose(fp_insert);
	fclose(fp_exit_lookup);
	fclose(fp_false_lookup);

	return 0;
}
