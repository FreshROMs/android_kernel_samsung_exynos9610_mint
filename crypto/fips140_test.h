#ifndef _CRYPTO_FIPS140_TEST_H
#define _CRYPTO_FIPS140_TEST_H

#include <linux/types.h>

#define FIPS140_MAX_LEN_IV		48
#define FIPS140_MAX_LEN_KEY	132
#define FIPS140_MAX_LEN_DIGEST	64
#define FIPS140_MAX_LEN_PCTEXT	1024
#define FIPS140_MAX_LEN_ENTROPY	48
#define FIPS140_MAX_LEN_STR	128

#define FIPS140_TEST_ENCRYPT 1
#define FIPS140_TEST_DECRYPT 0

#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
void reset_in_fips_err(void);
void set_fips_functest_KAT_mode(const int num);
void set_fips_functest_conditional_mode(const int num);
char *get_fips_functest_mode(void);
#define SKC_FUNCTEST_KAT_CASE_NUM 26
#define SKC_FUNCTEST_CONDITIONAL_CASE_NUM 2
#define SKC_FUNCTEST_NO_TEST "NO_TEST"
#endif // CONFIG_CRYPTO_FIPS_FUNC_TEST

typedef struct {
	const char key[FIPS140_MAX_LEN_KEY];
	const char iv[FIPS140_MAX_LEN_IV];
	const char ptext[FIPS140_MAX_LEN_PCTEXT];
	const char ctext[FIPS140_MAX_LEN_PCTEXT];
	unsigned char klen;
	unsigned char iv_len;
	unsigned short len;
} cipher_testvec_t;

typedef struct {
	const char key[FIPS140_MAX_LEN_KEY];
	const char ptext[FIPS140_MAX_LEN_PCTEXT];
	const char digest[FIPS140_MAX_LEN_DIGEST];
	unsigned short plen;
	unsigned short klen;
} hash_testvec_t;

typedef struct {
	const char key[FIPS140_MAX_LEN_KEY];
	const char iv[FIPS140_MAX_LEN_IV];
	const char input[FIPS140_MAX_LEN_PCTEXT];
	const char assoc[FIPS140_MAX_LEN_PCTEXT];
	const char result[FIPS140_MAX_LEN_PCTEXT];
	unsigned int iv_len;
	unsigned char klen;
	unsigned short ilen;
	unsigned short alen;
	unsigned short rlen;
} aead_testvec_t;

typedef struct {
	const unsigned char entropy[FIPS140_MAX_LEN_ENTROPY];
	size_t entropylen;
	const unsigned char entpra[FIPS140_MAX_LEN_ENTROPY];
	const unsigned char entprb[FIPS140_MAX_LEN_ENTROPY];
	size_t entprlen;
	const unsigned char addtla[FIPS140_MAX_LEN_IV];
	const unsigned char addtlb[FIPS140_MAX_LEN_IV];
	size_t addtllen;
	const unsigned char pers[FIPS140_MAX_LEN_STR];
	size_t perslen;
	const unsigned char expected[FIPS140_MAX_LEN_STR];
	size_t expectedlen;
} drbg_testvec_t;

typedef struct {
	uint8_t mode;
	size_t rlen;
	size_t KiLength;
	size_t LabelLength;
	size_t ContextLength;
	uint32_t L;
	const uint8_t Ki[FIPS140_MAX_LEN_KEY];
	const uint8_t Ko[FIPS140_MAX_LEN_KEY];
	const uint8_t Label[FIPS140_MAX_LEN_STR];
	const uint8_t Context[FIPS140_MAX_LEN_STR];
} kbkdf_testvec_t;

typedef struct {
	const cipher_testvec_t* vecs;;
	unsigned int tv_count;
} cipher_test_suite_t;

typedef struct {
	const hash_testvec_t* vecs;;
	unsigned int tv_count;
} hash_test_suite_t;

typedef struct {
	const aead_testvec_t* vecs;;
	unsigned int tv_count;
} aead_test_suite_t;

typedef struct {
	const drbg_testvec_t* vecs;
	unsigned int tv_count;
} drbg_test_suite_t;

typedef struct {
	const kbkdf_testvec_t* vecs;
	unsigned int tv_count;
} kbkdf_test_suite_t;

extern int alg_test_fips140(const char *driver, const char *alg);
int fips140_kat(void);

#endif	/* _CRYPTO_FIPS140_TEST_H */
