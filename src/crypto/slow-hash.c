// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "aesb.h"
#include "common/int-util.h"
#include "hash-ops.h"
#include "oaes_lib.h"

static void (*const extra_hashes[4])(const void *, size_t, char *) = {
  hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
};

#define MEMORY         (1 << 21) /* 2 MiB */
#define ITER           (1 << 20)
#define AES_BLOCK_SIZE  16
#define AES_KEY_SIZE    32 /*16*/
#define INIT_SIZE_BLK   8
#define INIT_SIZE_BYTE (INIT_SIZE_BLK * AES_BLOCK_SIZE)

static size_t e2i(const uint8_t* a) { return (*((uint64_t*)a) / AES_BLOCK_SIZE) & (MEMORY / AES_BLOCK_SIZE - 1); }

static void mul(const uint8_t* a, const uint8_t* b, uint8_t* res) {
  uint64_t a0, b0;
  uint64_t hi, lo;

  a0 = SWAP64LE(((uint64_t*)a)[0]);
  b0 = SWAP64LE(((uint64_t*)b)[0]);
  lo = mul128(a0, b0, &hi);
  ((uint64_t*)res)[0] = SWAP64LE(hi);
  ((uint64_t*)res)[1] = SWAP64LE(lo);
}

static void sum_half_blocks(uint8_t* a, const uint8_t* b) {
  uint64_t a0, a1, b0, b1;

  a0 = SWAP64LE(((uint64_t*)a)[0]);
  a1 = SWAP64LE(((uint64_t*)a)[1]);
  b0 = SWAP64LE(((uint64_t*)b)[0]);
  b1 = SWAP64LE(((uint64_t*)b)[1]);
  a0 += b0;
  a1 += b1;
  ((uint64_t*)a)[0] = SWAP64LE(a0);
  ((uint64_t*)a)[1] = SWAP64LE(a1);
}

static void copy_block(uint8_t* dst, const uint8_t* src) {
  ((uint64_t*)dst)[0] = ((uint64_t*)src)[0];
  ((uint64_t*)dst)[1] = ((uint64_t*)src)[1];
}

static void xor_blocks(uint8_t* a, const uint8_t* b) {
  ((uint64_t*)a)[0] ^= ((uint64_t*)b)[0];
  ((uint64_t*)a)[1] ^= ((uint64_t*)b)[1];
}

static void xor_blocks_dst(const uint8_t* a, const uint8_t* b, uint8_t* dst) {
  ((uint64_t*)dst)[0] = ((uint64_t*)a)[0] ^ ((uint64_t*)b)[0];
  ((uint64_t*)dst)[1] = ((uint64_t*)a)[1] ^ ((uint64_t*)b)[1];
}

static void mul_sum_xor_dst(const uint8_t* a, uint8_t* c, uint8_t* dst) {
  uint8_t product[AES_BLOCK_SIZE];
  mul(a, dst, product);
  sum_half_blocks(product, c);

  xor_blocks_dst(dst, product, c);
  copy_block(dst, product);
}

#pragma pack(push, 1)
union cn_slow_hash_state {
  union hash_state hs;
  struct {
    uint8_t k[64];
    uint8_t init[INIT_SIZE_BYTE];
  };
};
#pragma pack(pop)

void cn_slow_hash(const void *data, size_t length, char *hash) {
  uint8_t long_state[MEMORY];
  union cn_slow_hash_state state;
  uint8_t text[INIT_SIZE_BYTE];
  uint8_t a[AES_BLOCK_SIZE];
  uint8_t b[AES_BLOCK_SIZE];
  uint8_t c[AES_BLOCK_SIZE];
  size_t i, j;
  uint8_t aes_key[AES_KEY_SIZE];
  oaes_ctx* aes_ctx;

  hash_process(&state.hs, data, length);
  memcpy(text, state.init, INIT_SIZE_BYTE);
  memcpy(aes_key, state.hs.b, AES_KEY_SIZE);
  aes_ctx = (oaes_ctx*)oaes_alloc();

  oaes_key_import_data(aes_ctx, aes_key, AES_KEY_SIZE);
  for (i = 0; i < MEMORY / INIT_SIZE_BYTE; i++) {
    for (j = 0; j < INIT_SIZE_BLK; j++)
      aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], aes_ctx->key->exp_data);

    memcpy(&long_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
  }

  for (i = 0; i < 16; i++) {
    a[i] = state.k[     i] ^ state.k[32 + i];
    b[i] = state.k[16 + i] ^ state.k[48 + i];
  }

  for (i = 0; i < ITER / 2; i++) {
    /* Dependency chain: address -> read value ------+
     * written value <-+ hard function (AES or MUL) <+
     * next address  <-+
     */
    /* Iteration 1 */
    j = e2i(a);
    aesb_single_round(&long_state[j * AES_BLOCK_SIZE], c, a);
    xor_blocks_dst(c, b, &long_state[j * AES_BLOCK_SIZE]);
    assert(j == e2i(a));
    /* Iteration 2 */
    mul_sum_xor_dst(c, a, &long_state[e2i(c) * AES_BLOCK_SIZE]);
    copy_block(b, c);
  }

  memcpy(text, state.init, INIT_SIZE_BYTE);
  oaes_key_import_data(aes_ctx, &state.hs.b[32], AES_KEY_SIZE);
  for (i = 0; i < MEMORY / INIT_SIZE_BYTE; i++) {
    for (j = 0; j < INIT_SIZE_BLK; j++) {
      xor_blocks(&text[j * AES_BLOCK_SIZE], &long_state[i * INIT_SIZE_BYTE + j * AES_BLOCK_SIZE]);
      aesb_pseudo_round(&text[j * AES_BLOCK_SIZE], &text[j * AES_BLOCK_SIZE], aes_ctx->key->exp_data);
    }
  }
  memcpy(state.init, text, INIT_SIZE_BYTE);
  hash_permutation(&state.hs);
  extra_hashes[state.hs.b[0] & 3](&state, 200, hash);
  oaes_free((OAES_CTX **)&aes_ctx);
}
