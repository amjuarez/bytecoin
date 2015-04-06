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

#include <limits>

#include <boost/utility/value_init.hpp>

#include "include_base_utils.h"

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/difficulty.h"
#include "cryptonote_protocol/blobdatatype.h"


namespace cryptonote
{
  //---------------------------------------------------------------
  void get_transaction_prefix_hash(const TransactionPrefix& tx, crypto::hash& h);
  crypto::hash get_transaction_prefix_hash(const TransactionPrefix& tx);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, Transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, Transaction& tx);

  struct tx_source_entry
  {
    typedef std::pair<uint64_t, crypto::public_key> output_entry;

    std::vector<output_entry> outputs;  //index + key
    size_t real_output;                 //index in outputs vector of real output_entry
    crypto::public_key real_out_tx_key; //incoming real tx public key
    size_t real_output_in_tx_index;     //index in transaction outputs vector
    uint64_t amount;                    //money
  };

  struct tx_destination_entry
  {
    uint64_t amount;                    //money
    AccountPublicAddress addr;          //destination address

    tx_destination_entry() : amount(0), addr(boost::value_initialized<AccountPublicAddress>()) { }
    tx_destination_entry(uint64_t a, const AccountPublicAddress &ad) : amount(a), addr(ad) { }
  };

  //---------------------------------------------------------------
  bool construct_tx(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, std::vector<uint8_t> extra, Transaction& tx, uint64_t unlock_time);

  template<typename T>
  bool find_tx_extra_field_by_type(const std::vector<tx_extra_field>& tx_extra_fields, T& field)
  {
    auto it = std::find_if(tx_extra_fields.begin(), tx_extra_fields.end(), [](const tx_extra_field& f) { return typeid(T) == f.type(); });
    if(tx_extra_fields.end() == it)
      return false;

    field = boost::get<T>(*it);
    return true;
  }

  bool parse_tx_extra(const std::vector<uint8_t>& tx_extra, std::vector<tx_extra_field>& tx_extra_fields);
  crypto::public_key get_tx_pub_key_from_extra(const std::vector<uint8_t>& tx_extra);
  crypto::public_key get_tx_pub_key_from_extra(const Transaction& tx);
  bool add_tx_pub_key_to_extra(Transaction& tx, const crypto::public_key& tx_pub_key);
  bool add_extra_nonce_to_tx_extra(std::vector<uint8_t>& tx_extra, const blobdata& extra_nonce);
  void set_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const crypto::hash& payment_id);
  bool get_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, crypto::hash& payment_id);
  bool append_mm_tag_to_extra(std::vector<uint8_t>& tx_extra, const tx_extra_merge_mining_tag& mm_tag);
  bool get_mm_tag_from_extra(const std::vector<uint8_t>& tx_extra, tx_extra_merge_mining_tag& mm_tag);
  bool is_out_to_acc(const account_keys& acc, const TransactionOutputToKey& out_key, const crypto::public_key& tx_pub_key, size_t keyIndex);
  bool is_out_to_acc(const account_keys& acc, const TransactionOutputToKey& out_key, const crypto::key_derivation& derivation, size_t keyIndex);
  bool lookup_acc_outs(const account_keys& acc, const Transaction& tx, const crypto::public_key& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool lookup_acc_outs(const account_keys& acc, const Transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool get_tx_fee(const Transaction& tx, uint64_t & fee);
  uint64_t get_tx_fee(const Transaction& tx);
  bool generate_key_image_helper(const account_keys& ack, const crypto::public_key& tx_public_key, size_t real_output_index, KeyPair& in_ephemeral, crypto::key_image& ki);
  void get_blob_hash(const blobdata& blob, crypto::hash& res);
  crypto::hash get_blob_hash(const blobdata& blob);
  std::string short_hash_str(const crypto::hash& h);
  bool createTxExtraWithPaymentId(const std::string& paymentIdString, std::vector<uint8_t>& extra);
  //returns false if payment id is not found or parse error
  bool getPaymentIdFromTxExtra(const std::vector<uint8_t>& extra, crypto::hash& paymentId);
  bool parsePaymentId(const std::string& paymentIdString, crypto::hash& paymentId);

  crypto::hash get_transaction_hash(const Transaction& t);
  bool get_transaction_hash(const Transaction& t, crypto::hash& res);
  bool get_transaction_hash(const Transaction& t, crypto::hash& res, size_t& blob_size);
  bool get_block_hashing_blob(const Block& b, blobdata& blob);
  bool get_parent_block_hashing_blob(const Block& b, blobdata& blob);
  bool get_aux_block_header_hash(const Block& b, crypto::hash& res);
  bool get_block_hash(const Block& b, crypto::hash& res);
  crypto::hash get_block_hash(const Block& b);
  bool get_block_longhash(crypto::cn_context &context, const Block& b, crypto::hash& res);
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, Block& b);
  bool get_inputs_money_amount(const Transaction& tx, uint64_t& money);
  uint64_t get_outs_money_amount(const Transaction& tx);
  bool check_inputs_types_supported(const Transaction& tx);
  bool check_outs_valid(const Transaction& tx);
  bool checkMultisignatureInputsDiff(const Transaction& tx);

  bool check_money_overflow(const Transaction& tx);
  bool check_outs_overflow(const Transaction& tx);
  bool check_inputs_overflow(const Transaction& tx);
  uint64_t get_block_height(const Block& b);
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off);
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off);
  //---------------------------------------------------------------
  template<class t_object>
  bool t_serializable_object_to_blob(const t_object& to, blobdata& b_blob)
  {
    std::stringstream ss;
    binary_archive<true> ba(ss);
    bool r = ::serialization::serialize(ba, const_cast<t_object&>(to));
    b_blob = ss.str();
    return r;
  }
  //---------------------------------------------------------------
  template<class t_object>
  blobdata t_serializable_object_to_blob(const t_object& to)
  {
    blobdata b;
    t_serializable_object_to_blob(to, b);
    return b;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res)
  {
    get_blob_hash(t_serializable_object_to_blob(o), res);
    return true;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_blobsize(const t_object& o, size_t& size) {
    blobdata blob;
    if (!t_serializable_object_to_blob(o, blob)) {
      size = (std::numeric_limits<size_t>::max)();
      return false;
    }
    size = blob.size();
    return true;
  }
  //---------------------------------------------------------------
  template<class t_object>
  size_t get_object_blobsize(const t_object& o)
  {
    size_t size;
    get_object_blobsize(o, size);
    return size;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res, size_t& blob_size)
  {
    blobdata bl = t_serializable_object_to_blob(o);
    blob_size = bl.size();
    get_blob_hash(bl, res);
    return true;
  }
  //---------------------------------------------------------------
  template <typename T>
  std::string obj_to_json_str(const T& obj) {
    std::stringstream ss;
    json_archive<true> ar(ss, true);
    bool r = ::serialization::serialize(ar, *const_cast<T*>(&obj));
    CHECK_AND_ASSERT_MES(r, "", "obj_to_json_str failed: serialization::serialize returned false");
    return ss.str();
  }
  //---------------------------------------------------------------
  // 62387455827 -> 455827 + 7000000 + 80000000 + 300000000 + 2000000000 + 60000000000, where 455827 <= dust_threshold
  template<typename chunk_handler_t, typename dust_handler_t>
  void decompose_amount_into_digits(uint64_t amount, uint64_t dust_threshold, const chunk_handler_t& chunk_handler, const dust_handler_t& dust_handler)
  {
    if (0 == amount)
    {
      return;
    }

    bool is_dust_handled = false;
    uint64_t dust = 0;
    uint64_t order = 1;
    while (0 != amount)
    {
      uint64_t chunk = (amount % 10) * order;
      amount /= 10;
      order *= 10;

      if (dust + chunk <= dust_threshold)
      {
        dust += chunk;
      }
      else
      {
        if (!is_dust_handled && 0 != dust)
        {
          dust_handler(dust);
          is_dust_handled = true;
        }
        if (0 != chunk)
        {
          chunk_handler(chunk);
        }
      }
    }

    if (!is_dust_handled && 0 != dust)
    {
      dust_handler(dust);
    }
  }
  //---------------------------------------------------------------
  blobdata block_to_blob(const Block& b);
  bool block_to_blob(const Block& b, blobdata& b_blob);
  blobdata tx_to_blob(const Transaction& b);
  bool tx_to_blob(const Transaction& b, blobdata& b_blob);
  void get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes, crypto::hash& h);
  crypto::hash get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes);
  crypto::hash get_tx_tree_hash(const Block& b);

#define CHECKED_GET_SPECIFIC_VARIANT(variant_var, specific_type, variable_name, fail_return_val) \
  CHECK_AND_ASSERT_MES(variant_var.type() == typeid(specific_type), fail_return_val, "wrong variant type: " << variant_var.type().name() << ", expected " << typeid(specific_type).name()); \
  specific_type& variable_name = boost::get<specific_type>(variant_var);

}
