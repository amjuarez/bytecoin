// Copyright (c) 2016, Monero Research Labs
// Copyright (c) 2016-2017 XDN-project developers
//
// Author: Shen Noether <shen.noether@gmx.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once
#ifndef RCT_TYPES_H
#define RCT_TYPES_H

#include <cstddef>
#include <vector>
#include <ostream>
#include <cinttypes>

extern "C" {
#include "crypto/crypto-ops.h"
#include "crypto/random.h"
#include "crypto/keccak.h"
}

#include "crypto/crypto.h"
#include "crypto/generic-ops.h"

//atomic units of moneros
#define ATOMS 64

//for printing large ints

using namespace std;
using namespace Crypto;

//Namespace specifically for ring ct code
namespace rct {
//basic ops containers
typedef unsigned char * Bytes;

// Can contain a secret or public key
//  similar to SecretKey / public_key of crypto-ops,
//  but uses unsigned chars,
//  also includes an operator for accessing the i'th byte.
struct key {
  unsigned char & operator[](int i) {
    return bytes[i];
  }
  unsigned char operator[](int i) const {
    return bytes[i];
  }
  bool operator==(const key &k) const { return !memcmp(bytes, k.bytes, sizeof(bytes)); }
  unsigned char bytes[32];
};

typedef vector<key> keyV; //vector of keys
typedef vector<keyV> keyM; //matrix of keys (indexed by column first)

//containers For CT operations
//if it's  representing a private ctkey then "dest" contains the secret key of the address
// while "mask" contains a where C = aG + bH is CT pedersen commitment and b is the amount
// (store b, the amount, separately
//if it's representing a public ctkey, then "dest" = P the address, mask = C the commitment
struct ctkey {
  key dest;
  key mask; //C here if public
};
typedef vector<ctkey> ctkeyV;
typedef vector<ctkeyV> ctkeyM;

//data for passing the amount to the receiver secretly
// If the pedersen commitment to an amount is C = aG + bH,
// "mask" contains a 32 byte key a
// "amount" contains a hex representation (in 32 bytes) of a 64 bit number
// "senderPk" is not the senders actual public key, but a one-time public key generated for
// the purpose of the ECDH exchange
struct ecdhTuple {
  key mask;
  key amount;
  key senderPk;
};

//containers for representing amounts
typedef uint64_t xmr_amount;
typedef unsigned int bits[ATOMS];
typedef key key64[64];

struct boroSig {
  key64 s0;
  key64 s1;
  key ee;
};

//Container for precomp
struct geDsmp {
  ge_dsmp k;
};

//just contains the necessary keys to represent MLSAG sigs
//c.f. http://eprint.iacr.org/2015/1098
struct mgSig {
  keyM ss;
  key cc;
  keyV II;
};

//contains the data for an Borromean sig
// also contains the "Ci" values such that
// \sum Ci = C
// and the signature proves that each Ci is either
// a Pedersen commitment to 0 or to 2^i
//thus proving that C is in the range of [0, 2^64]
struct rangeSig {
  boroSig asig;
  key64 Ci;
};

//A container to hold all signatures necessary for RingCT
// rangeSigs holds all the rangeproof data of a transaction
// MG holds the MLSAG signature of a transaction
// mixRing holds all the public keypairs (P, C) for a transaction
// ecdhInfo holds an encoded mask / amount to be passed to each receiver
// outPk contains public keypairs which are destinations (P, C),
//  P = address, C = commitment to amount
enum {
  RCTTypeNull = 0,
  RCTTypeFull = 1,
  RCTTypeSimple = 2,
};

struct rctSigBase {
  uint8_t type;
  key message;
  ctkeyM mixRing; //the set of all pubkeys / copy
  //pairs that you mix with
  keyV pseudoOuts; //C - for simple rct
  vector<ecdhTuple> ecdhInfo;
  ctkeyV outPk;
  xmr_amount txnFee; // contains b
};

struct rctSigPrunable {
  vector<rangeSig> rangeSigs;
  vector<mgSig> MGs; // simple rct has N, full has 1
};

struct rctSig: public rctSigBase {
  rctSigPrunable p;
};

static inline const rct::key pk2rct(const Crypto::PublicKey &pk) { return (const rct::key&)pk; }
static inline const rct::key sk2rct(const Crypto::SecretKey &sk) { return (const rct::key&)sk; }
static inline const rct::key ki2rct(const Crypto::KeyImage &ki) { return (const rct::key&)ki; }
static inline const rct::key hash2rct(const Crypto::Hash &h) { return (const rct::key&)h; }
static inline const Crypto::PublicKey rct2pk(const rct::key &k) { return (const Crypto::PublicKey&)k; }
static inline const Crypto::SecretKey rct2sk(const rct::key &k) { return (const Crypto::SecretKey&)k; }
static inline const Crypto::KeyImage rct2ki(const rct::key &k) { return (const Crypto::KeyImage&)k; }
static inline const Crypto::Hash rct2hash(const rct::key &k) { return (const Crypto::Hash&)k; }
static inline bool operator==(const rct::key &k0, const Crypto::PublicKey &k1) { return memcmp(&k0, &k1, 32) == 0; }
static inline bool operator!=(const rct::key &k0, const Crypto::PublicKey &k1) { return memcmp(&k0, &k1, 32) != 0; }
}

namespace cryptonote {
static inline bool operator==(const Crypto::PublicKey &k0, const rct::key &k1) { return memcmp(&k0, &k1, 32) == 0; }
static inline bool operator!=(const Crypto::PublicKey &k0, const rct::key &k1) { return memcmp(&k0, &k1, 32) != 0; }
static inline bool operator==(const Crypto::SecretKey &k0, const rct::key &k1) { return memcmp(&k0, &k1, 32) == 0; }
static inline bool operator!=(const Crypto::SecretKey &k0, const rct::key &k1) { return memcmp(&k0, &k1, 32) != 0; }
}

template<typename T> std::ostream &print256(std::ostream &o, const T &v);
inline std::ostream &operator <<(std::ostream &o, const rct::key &v) { return print256(o, v); }

#endif  /* RCTTYPES_H */
