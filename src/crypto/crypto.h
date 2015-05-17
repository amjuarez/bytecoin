// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstddef>
#include <limits>
#include <mutex>
#include <vector>

#include "common/pod-class.h"
#include "generic-ops.h"
#include "hash.h"

namespace crypto {

  using std::size_t;

  extern "C" {
#include "random.h"
  }

  extern std::mutex random_lock;

#pragma pack(push, 1)
  POD_CLASS ec_point {
    char data[32];
  };

  POD_CLASS ec_scalar {
    char data[32];
  };

  POD_CLASS public_key: ec_point {
    friend class crypto_ops;
  };

  POD_CLASS secret_key: ec_scalar {
    friend class crypto_ops;
  };

  POD_CLASS key_derivation: ec_point {
    friend class crypto_ops;
  };

  POD_CLASS key_image: ec_point {
    friend class crypto_ops;
  };

  POD_CLASS signature {
    ec_scalar c, r;
    friend class crypto_ops;
  };
#pragma pack(pop)

  static_assert(sizeof(ec_point) == 32 && sizeof(ec_scalar) == 32 &&
    sizeof(public_key) == 32 && sizeof(secret_key) == 32 &&
    sizeof(key_derivation) == 32 && sizeof(key_image) == 32 &&
    sizeof(signature) == 64, "Invalid structure size");

  class crypto_ops {
    crypto_ops();
    crypto_ops(const crypto_ops &);
    void operator=(const crypto_ops &);
    ~crypto_ops();

    static void generate_keys(public_key &, secret_key &);
    friend void generate_keys(public_key &, secret_key &);
    static bool check_key(const public_key &);
    friend bool check_key(const public_key &);
    static bool secret_key_to_public_key(const secret_key &, public_key &);
    friend bool secret_key_to_public_key(const secret_key &, public_key &);
    static bool generate_key_derivation(const public_key &, const secret_key &, key_derivation &);
    friend bool generate_key_derivation(const public_key &, const secret_key &, key_derivation &);
    static bool derive_public_key(const key_derivation &, std::size_t, const public_key &, public_key &);
    friend bool derive_public_key(const key_derivation &, std::size_t, const public_key &, public_key &);
    static void derive_secret_key(const key_derivation &, std::size_t, const secret_key &, secret_key &);
    friend void derive_secret_key(const key_derivation &, std::size_t, const secret_key &, secret_key &);
    static bool underive_public_key(const key_derivation &, std::size_t, const public_key &, public_key &);
    friend bool underive_public_key(const key_derivation &, std::size_t, const public_key &, public_key &);
    static void generate_signature(const hash &, const public_key &, const secret_key &, signature &);
    friend void generate_signature(const hash &, const public_key &, const secret_key &, signature &);
    static bool check_signature(const hash &, const public_key &, const signature &);
    friend bool check_signature(const hash &, const public_key &, const signature &);
    static void generate_key_image(const public_key &, const secret_key &, key_image &);
    friend void generate_key_image(const public_key &, const secret_key &, key_image &);
    static void generate_ring_signature(const hash &, const key_image &,
      const public_key *const *, std::size_t, const secret_key &, std::size_t, signature *);
    friend void generate_ring_signature(const hash &, const key_image &,
      const public_key *const *, std::size_t, const secret_key &, std::size_t, signature *);
    static bool check_ring_signature(const hash &, const key_image &,
      const public_key *const *, std::size_t, const signature *);
    friend bool check_ring_signature(const hash &, const key_image &,
      const public_key *const *, std::size_t, const signature *);
  };

  /* Generate a value filled with random bytes.
   */
  template<typename T>
  typename std::enable_if<std::is_pod<T>::value, T>::type rand() {
    typename std::remove_cv<T>::type res;
    std::lock_guard<std::mutex> lock(random_lock);
    generate_random_bytes(sizeof(T), &res);
    return res;
  }

  /* Random number engine based on crypto::rand()
   */
  template <typename T>
  class random_engine {
  public:
    typedef T result_type;

#ifdef __clang__
    constexpr static T min() {
      return (std::numeric_limits<T>::min)();
    }

    constexpr static T max() {
      return (std::numeric_limits<T>::max)();
    }
#else
    static T(min)() {
      return (std::numeric_limits<T>::min)();
    }

    static T(max)() {
      return (std::numeric_limits<T>::max)();
    }
#endif
    typename std::enable_if<std::is_unsigned<T>::value, T>::type operator()() {
      return rand<T>();
    }
  };

  /* Generate a new key pair
   */
  inline void generate_keys(public_key &pub, secret_key &sec) {
    crypto_ops::generate_keys(pub, sec);
  }

  /* Check a public key. Returns true if it is valid, false otherwise.
   */
  inline bool check_key(const public_key &key) {
    return crypto_ops::check_key(key);
  }

  /* Checks a private key and computes the corresponding public key.
   */
  inline bool secret_key_to_public_key(const secret_key &sec, public_key &pub) {
    return crypto_ops::secret_key_to_public_key(sec, pub);
  }

  /* To generate an ephemeral key used to send money to:
   * * The sender generates a new key pair, which becomes the transaction key. The public transaction key is included in "extra" field.
   * * Both the sender and the receiver generate key derivation from the transaction key and the receivers' "view" key.
   * * The sender uses key derivation, the output index, and the receivers' "spend" key to derive an ephemeral public key.
   * * The receiver can either derive the public key (to check that the transaction is addressed to him) or the private key (to spend the money).
   */
  inline bool generate_key_derivation(const public_key &key1, const secret_key &key2, key_derivation &derivation) {
    return crypto_ops::generate_key_derivation(key1, key2, derivation);
  }
  inline bool derive_public_key(const key_derivation &derivation, std::size_t output_index,
    const public_key &base, public_key &derived_key) {
    return crypto_ops::derive_public_key(derivation, output_index, base, derived_key);
  }
  inline void derive_secret_key(const key_derivation &derivation, std::size_t output_index,
    const secret_key &base, secret_key &derived_key) {
    crypto_ops::derive_secret_key(derivation, output_index, base, derived_key);
  }

  /* Inverse function of derive_public_key. It can be used by the receiver to find which "spend" key was used to generate a transaction. This may be useful if the receiver used multiple addresses which only differ in "spend" key.
   */
  inline bool underive_public_key(const key_derivation &derivation, std::size_t output_index,
    const public_key &derived_key, public_key &base) {
    return crypto_ops::underive_public_key(derivation, output_index, derived_key, base);
  }

  /* Generation and checking of a standard signature.
   */
  inline void generate_signature(const hash &prefix_hash, const public_key &pub, const secret_key &sec, signature &sig) {
    crypto_ops::generate_signature(prefix_hash, pub, sec, sig);
  }
  inline bool check_signature(const hash &prefix_hash, const public_key &pub, const signature &sig) {
    return crypto_ops::check_signature(prefix_hash, pub, sig);
  }

  /* To send money to a key:
   * * The sender generates an ephemeral key and includes it in transaction output.
   * * To spend the money, the receiver generates a key image from it.
   * * Then he selects a bunch of outputs, including the one he spends, and uses them to generate a ring signature.
   * To check the signature, it is necessary to collect all the keys that were used to generate it. To detect double spends, it is necessary to check that each key image is used at most once.
   */
  inline void generate_key_image(const public_key &pub, const secret_key &sec, key_image &image) {
    crypto_ops::generate_key_image(pub, sec, image);
  }
  inline void generate_ring_signature(const hash &prefix_hash, const key_image &image,
    const public_key *const *pubs, std::size_t pubs_count,
    const secret_key &sec, std::size_t sec_index,
    signature *sig) {
    crypto_ops::generate_ring_signature(prefix_hash, image, pubs, pubs_count, sec, sec_index, sig);
  }
  inline bool check_ring_signature(const hash &prefix_hash, const key_image &image,
    const public_key *const *pubs, std::size_t pubs_count,
    const signature *sig) {
    return crypto_ops::check_ring_signature(prefix_hash, image, pubs, pubs_count, sig);
  }

  /* Variants with vector<const public_key *> parameters.
   */
  inline void generate_ring_signature(const hash &prefix_hash, const key_image &image,
    const std::vector<const public_key *> &pubs,
    const secret_key &sec, std::size_t sec_index,
    signature *sig) {
    generate_ring_signature(prefix_hash, image, pubs.data(), pubs.size(), sec, sec_index, sig);
  }
  inline bool check_ring_signature(const hash &prefix_hash, const key_image &image,
    const std::vector<const public_key *> &pubs,
    const signature *sig) {
    return check_ring_signature(prefix_hash, image, pubs.data(), pubs.size(), sig);
  }
}

CRYPTO_MAKE_HASHABLE(public_key)
CRYPTO_MAKE_HASHABLE(key_image)
CRYPTO_MAKE_COMPARABLE(signature)
