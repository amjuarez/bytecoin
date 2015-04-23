// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <set>

// epee
#include "include_base_utils.h"
#include "misc_language.h"

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "serialization/binary_utils.h"
#include "cryptonote_config.h"

using namespace epee;

namespace cryptonote
{
  //---------------------------------------------------------------
  void get_transaction_prefix_hash(const TransactionPrefix& tx, crypto::hash& h)
  {
    std::ostringstream s;
    binary_archive<true> a(s);
    ::serialization::serialize(a, const_cast<TransactionPrefix&>(tx));
    crypto::cn_fast_hash(s.str().data(), s.str().size(), h);
  }
  //---------------------------------------------------------------
  crypto::hash get_transaction_prefix_hash(const TransactionPrefix& tx)
  {
    crypto::hash h = null_hash;
    get_transaction_prefix_hash(tx, h);
    return h;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, Transaction& tx)
  {
    std::stringstream ss;
    ss << tx_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, tx);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse transaction from blob");
    return true;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, Transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash)
  {
    std::stringstream ss;
    ss << tx_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, tx);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse transaction from blob");
    //TODO: validate tx

    crypto::cn_fast_hash(tx_blob.data(), tx_blob.size(), tx_hash);
    get_transaction_prefix_hash(tx, tx_prefix_hash);
    return true;
  }
  //---------------------------------------------------------------
  bool generate_key_image_helper(const account_keys& ack, const crypto::public_key& tx_public_key, size_t real_output_index, KeyPair& in_ephemeral, crypto::key_image& ki)
  {
    crypto::key_derivation recv_derivation = AUTO_VAL_INIT(recv_derivation);
    bool r = crypto::generate_key_derivation(tx_public_key, ack.m_view_secret_key, recv_derivation);
    CHECK_AND_ASSERT_MES(r, false, "key image helper: failed to generate_key_derivation(" << tx_public_key << ", " << ack.m_view_secret_key << ")");

    r = crypto::derive_public_key(recv_derivation, real_output_index, ack.m_account_address.m_spendPublicKey, in_ephemeral.pub);
    CHECK_AND_ASSERT_MES(r, false, "key image helper: failed to derive_public_key(" << recv_derivation << ", " << real_output_index <<  ", " << ack.m_account_address.m_spendPublicKey << ")");

    crypto::derive_secret_key(recv_derivation, real_output_index, ack.m_spend_secret_key, in_ephemeral.sec);

    crypto::generate_key_image(in_ephemeral.pub, in_ephemeral.sec, ki);
    return true;
  }
  //---------------------------------------------------------------
  uint64_t power_integral(uint64_t a, uint64_t b)
  {
    if(b == 0)
      return 1;
    uint64_t total = a;
    for(uint64_t i = 1; i != b; i++)
      total *= a;
    return total;
  }
  //---------------------------------------------------------------
  bool get_tx_fee(const Transaction& tx, uint64_t & fee)
  {
    uint64_t amount_in = 0;
    uint64_t amount_out = 0;

    for (const auto& in : tx.vin) {
      if (in.type() == typeid(TransactionInputToKey)) {
        amount_in += boost::get<TransactionInputToKey>(in).amount;
      } else if (in.type() == typeid(TransactionInputMultisignature)) {
        amount_in += boost::get<TransactionInputMultisignature>(in).amount;
      }
    }

    for (const auto& o : tx.vout) {
      amount_out += o.amount;
    }

    CHECK_AND_ASSERT_MES(amount_in >= amount_out, false, "transaction spend (" <<amount_in << ") more than it has (" << amount_out << ")");
    fee = amount_in - amount_out;
    return true;
  }
  //---------------------------------------------------------------
  uint64_t get_tx_fee(const Transaction& tx)
  {
    uint64_t r = 0;
    if(!get_tx_fee(tx, r))
      return 0;
    return r;
  }
  //---------------------------------------------------------------
  bool parse_tx_extra(const std::vector<uint8_t>& tx_extra, std::vector<tx_extra_field>& tx_extra_fields)
  {
    tx_extra_fields.clear();

    if(tx_extra.empty())
      return true;

    std::string extra_str(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size());
    std::istringstream iss(extra_str);
    binary_archive<false> ar(iss);

    bool eof = false;
    while (!eof) {
      tx_extra_field field;
      bool r = ::do_serialize(ar, field);
      if (!r) {
        LOG_PRINT_L4("failed to deserialize extra field. extra = " <<
          string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));
        return false;
      }
      tx_extra_fields.push_back(field);

      std::ios_base::iostate state = iss.rdstate();
      eof = (EOF == iss.peek());
      iss.clear(state);
    }
    
    if (!::serialization::check_stream_state(ar)) {
      LOG_PRINT_L4("failed to deserialize extra field. extra = " <<
        string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));
      return false;
    }

    return true;
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const std::vector<uint8_t>& tx_extra)
  {
    std::vector<tx_extra_field> tx_extra_fields;
    parse_tx_extra(tx_extra, tx_extra_fields);

    tx_extra_pub_key pub_key_field;
    if(!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field))
      return null_pkey;

    return pub_key_field.pub_key;
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const Transaction& tx)
  {
    return get_tx_pub_key_from_extra(tx.extra);
  }
  //---------------------------------------------------------------
  bool add_tx_pub_key_to_extra(Transaction& tx, const crypto::public_key& tx_pub_key)
  {
    tx.extra.resize(tx.extra.size() + 1 + sizeof(crypto::public_key));
    tx.extra[tx.extra.size() - 1 - sizeof(crypto::public_key)] = TX_EXTRA_TAG_PUBKEY;
    *reinterpret_cast<crypto::public_key*>(&tx.extra[tx.extra.size() - sizeof(crypto::public_key)]) = tx_pub_key;
    return true;
  }
  //---------------------------------------------------------------
  bool add_extra_nonce_to_tx_extra(std::vector<uint8_t>& tx_extra, const blobdata& extra_nonce)
  {
    CHECK_AND_ASSERT_MES(extra_nonce.size() <= TX_EXTRA_NONCE_MAX_COUNT, false, "extra nonce could be 255 bytes max");
    size_t start_pos = tx_extra.size();
    tx_extra.resize(tx_extra.size() + 2 + extra_nonce.size());
    //write tag
    tx_extra[start_pos] = TX_EXTRA_NONCE;
    //write len
    ++start_pos;
    tx_extra[start_pos] = static_cast<uint8_t>(extra_nonce.size());
    //write data
    ++start_pos;
    memcpy(&tx_extra[start_pos], extra_nonce.data(), extra_nonce.size());
    return true;
  }
  //---------------------------------------------------------------
  void set_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const crypto::hash& payment_id)
  {
    extra_nonce.clear();
    extra_nonce.push_back(TX_EXTRA_NONCE_PAYMENT_ID);
    const uint8_t* payment_id_ptr = reinterpret_cast<const uint8_t*>(&payment_id);
    std::copy(payment_id_ptr, payment_id_ptr + sizeof(payment_id), std::back_inserter(extra_nonce));
  }
  //---------------------------------------------------------------
  bool get_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, crypto::hash& payment_id)
  {
    if(sizeof(crypto::hash) + 1 != extra_nonce.size())
      return false;
    if(TX_EXTRA_NONCE_PAYMENT_ID != extra_nonce[0])
      return false;
    payment_id = *reinterpret_cast<const crypto::hash*>(extra_nonce.data() + 1);
    return true;
  }

  bool parsePaymentId(const std::string& paymentIdString, crypto::hash& paymentId) {
    cryptonote::blobdata binData;
    if (!epee::string_tools::parse_hexstr_to_binbuff(paymentIdString, binData)) {
      return false;
    }

    if (sizeof(crypto::hash) != binData.size()) {
      return false;
    }

    paymentId = *reinterpret_cast<const crypto::hash*>(binData.data());
    return true;
  }


  bool createTxExtraWithPaymentId(const std::string& paymentIdString, std::vector<uint8_t>& extra) {
    crypto::hash paymentIdBin;

    if (!parsePaymentId(paymentIdString, paymentIdBin)) {
      return false;
    }

    std::string extraNonce;
    cryptonote::set_payment_id_to_tx_extra_nonce(extraNonce, paymentIdBin);

    if (!cryptonote::add_extra_nonce_to_tx_extra(extra, extraNonce)) {
      return false;
    }

    return true;
  }

  bool getPaymentIdFromTxExtra(const std::vector<uint8_t>& extra, crypto::hash& paymentId) {
    std::vector<tx_extra_field> tx_extra_fields;
    if(!parse_tx_extra(extra, tx_extra_fields)) {
      return false;
    }

    tx_extra_nonce extra_nonce;
    if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce)) {
      if (!get_payment_id_from_tx_extra_nonce(extra_nonce.nonce, paymentId)) {
        return false;
      }
    } else {
      return false;
    }

    return true;
  }

  bool construct_tx(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, std::vector<uint8_t> extra, Transaction& tx, uint64_t unlock_time)
  {
    tx.vin.clear();
    tx.vout.clear();
    tx.signatures.clear();

    tx.version = CURRENT_TRANSACTION_VERSION;
    tx.unlockTime = unlock_time;

    tx.extra = extra;
    KeyPair txkey = KeyPair::generate();
    add_tx_pub_key_to_extra(tx, txkey.pub);

    struct input_generation_context_data
    {
      KeyPair in_ephemeral;
    };
    std::vector<input_generation_context_data> in_contexts;


    uint64_t summary_inputs_money = 0;
    //fill inputs
    for (const tx_source_entry& src_entr : sources)
    {
      if(src_entr.real_output >= src_entr.outputs.size())
      {
        LOG_ERROR("real_output index (" << src_entr.real_output << ")bigger than output_keys.size()=" << src_entr.outputs.size());
        return false;
      }
      summary_inputs_money += src_entr.amount;

      //key_derivation recv_derivation;
      in_contexts.push_back(input_generation_context_data());
      KeyPair& in_ephemeral = in_contexts.back().in_ephemeral;
      crypto::key_image img;
      if(!generate_key_image_helper(sender_account_keys, src_entr.real_out_tx_key, src_entr.real_output_in_tx_index, in_ephemeral, img))
        return false;

      //check that derivated key is equal with real output key
      if( !(in_ephemeral.pub == src_entr.outputs[src_entr.real_output].second) )
      {
        LOG_ERROR("derived public key missmatch with output public key! "<< ENDL << "derived_key:"
          << string_tools::pod_to_hex(in_ephemeral.pub) << ENDL << "real output_public_key:"
          << string_tools::pod_to_hex(src_entr.outputs[src_entr.real_output].second) );
        return false;
      }

      //put key image into tx input
      TransactionInputToKey input_to_key;
      input_to_key.amount = src_entr.amount;
      input_to_key.keyImage = img;

      //fill outputs array and use relative offsets
      for (const tx_source_entry::output_entry& out_entry : src_entr.outputs) {
        input_to_key.keyOffsets.push_back(out_entry.first);
      }

      input_to_key.keyOffsets = absolute_output_offsets_to_relative(input_to_key.keyOffsets);
      tx.vin.push_back(input_to_key);
    }

    // "Shuffle" outs
    std::vector<tx_destination_entry> shuffled_dsts(destinations);
    std::sort(shuffled_dsts.begin(), shuffled_dsts.end(), [](const tx_destination_entry& de1, const tx_destination_entry& de2) { return de1.amount < de2.amount; } );

    uint64_t summary_outs_money = 0;
    //fill outputs
    size_t output_index = 0;
    for (const tx_destination_entry& dst_entr : shuffled_dsts) {
      CHECK_AND_ASSERT_MES(dst_entr.amount > 0, false, "Destination with wrong amount: " << dst_entr.amount);
      crypto::key_derivation derivation;
      crypto::public_key out_eph_public_key;
      bool r = crypto::generate_key_derivation(dst_entr.addr.m_viewPublicKey, txkey.sec, derivation);
      CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to generate_key_derivation(" << dst_entr.addr.m_viewPublicKey << ", " << txkey.sec << ")");

      r = crypto::derive_public_key(derivation, output_index, dst_entr.addr.m_spendPublicKey, out_eph_public_key);
      CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to derive_public_key(" << derivation << ", " << output_index << ", "<< dst_entr.addr.m_spendPublicKey << ")");

      TransactionOutput out;
      out.amount = dst_entr.amount;
      TransactionOutputToKey tk;
      tk.key = out_eph_public_key;
      out.target = tk;
      tx.vout.push_back(out);
      output_index++;
      summary_outs_money += dst_entr.amount;
    }

    //check money
    if(summary_outs_money > summary_inputs_money )
    {
      LOG_ERROR("Transaction inputs money ("<< summary_inputs_money << ") less than outputs money (" << summary_outs_money << ")");
      return false;
    }


    //generate ring signatures
    crypto::hash tx_prefix_hash;
    get_transaction_prefix_hash(tx, tx_prefix_hash);

    std::stringstream ss_ring_s;
    size_t i = 0;
    for (const tx_source_entry& src_entr : sources) {
      ss_ring_s << "pub_keys:" << ENDL;
      std::vector<const crypto::public_key*> keys_ptrs;
      for (const tx_source_entry::output_entry& o : src_entr.outputs) {
        keys_ptrs.push_back(&o.second);
        ss_ring_s << o.second << ENDL;
      }

      tx.signatures.push_back(std::vector<crypto::signature>());
      std::vector<crypto::signature>& sigs = tx.signatures.back();
      sigs.resize(src_entr.outputs.size());
      crypto::generate_ring_signature(tx_prefix_hash, boost::get<TransactionInputToKey>(tx.vin[i]).keyImage, keys_ptrs,
        in_contexts[i].in_ephemeral.sec, src_entr.real_output, sigs.data());
      ss_ring_s << "signatures:" << ENDL;
      std::for_each(sigs.begin(), sigs.end(), [&](const crypto::signature& s){ss_ring_s << s << ENDL;});
      ss_ring_s << "prefix_hash:" << tx_prefix_hash << ENDL << "in_ephemeral_key: " << in_contexts[i].in_ephemeral.sec <<
        ENDL << "real_output: " << src_entr.real_output;
      i++;
    }

    LOG_PRINT2("construct_tx.log", "transaction_created: " << get_transaction_hash(tx) << ENDL << obj_to_json_str(tx) << ENDL << ss_ring_s.str() , LOG_LEVEL_3);

    return true;
  }
  //---------------------------------------------------------------
  bool get_inputs_money_amount(const Transaction& tx, uint64_t& money)
  {
    money = 0;

    for (const auto& in : tx.vin) {
      uint64_t amount = 0;

      if (in.type() == typeid(TransactionInputToKey)) {
        amount = boost::get<TransactionInputToKey>(in).amount;
      } else if (in.type() == typeid(TransactionInputMultisignature)) {
        amount = boost::get<TransactionInputMultisignature>(in).amount;
      }

      money += amount;
    }
    return true;
  }
  //---------------------------------------------------------------
  uint64_t get_block_height(const Block& b)
  {
    CHECK_AND_ASSERT_MES(b.minerTx.vin.size() == 1, 0, "wrong miner tx in block: " << get_block_hash(b) << ", b.minerTx.vin.size() != 1");
    CHECKED_GET_SPECIFIC_VARIANT(b.minerTx.vin[0], const TransactionInputGenerate, coinbase_in, 0);
    return coinbase_in.height;
  }
  //---------------------------------------------------------------
  bool check_inputs_types_supported(const Transaction& tx) {
    for (const auto& in : tx.vin) {
      if (in.type() != typeid(TransactionInputToKey) && in.type() != typeid(TransactionInputMultisignature)) {
        LOG_PRINT_L1("Transaction << " << get_transaction_hash(tx) << " contains inputs with invalid type.");
        return false;
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool check_outs_valid(const Transaction& tx) {
    for (const TransactionOutput& out : tx.vout) {
      //assert(out.target.type() == typeid(TransactionOutputToKey) || out.target.type() == typeid(TransactionOutputMultisignature));
      if (out.target.type() == typeid(TransactionOutputToKey)) {
        CHECK_AND_NO_ASSERT_MES(0 < out.amount, false, "zero amount ouput in transaction id=" << get_transaction_hash(tx));

        if (!check_key(boost::get<TransactionOutputToKey>(out.target).key)) {
          return false;
        }
      } else if (out.target.type() == typeid(TransactionOutputMultisignature)) {
        const TransactionOutputMultisignature& multisignatureOutput = ::boost::get<TransactionOutputMultisignature>(out.target);
        if (multisignatureOutput.requiredSignatures > multisignatureOutput.keys.size()) {
          LOG_PRINT_L1("Transaction << " << get_transaction_hash(tx) << " contains multisignature output with invalid required signature count.");
          return false;
        }

        for (const crypto::public_key& key : multisignatureOutput.keys) {
          if (!check_key(key)) {
            LOG_PRINT_L1("Transaction << " << get_transaction_hash(tx) << " contains multisignature output with invalid public keys.");
            return false;
          }
        }
      } else {
        LOG_PRINT_L1("Transaction << " << get_transaction_hash(tx) << " contains outputs with invalid type.");
        return false;
      }
    }

    return true;
  }

  //-----------------------------------------------------------------------------------------------
  bool checkMultisignatureInputsDiff(const Transaction& tx) {
    std::set<std::pair<uint64_t, uint32_t>> inputsUsage;
    for (const auto& inv : tx.vin) {
      if (inv.type() == typeid(TransactionInputMultisignature)) {
        const TransactionInputMultisignature& in = ::boost::get<TransactionInputMultisignature>(inv);
        if (!inputsUsage.insert(std::make_pair(in.amount, static_cast<uint32_t>(in.outputIndex))).second) {
          return false;
        }
      }
    }
    return true;
  }

  //-----------------------------------------------------------------------------------------------
  bool check_money_overflow(const Transaction& tx)
  {
    return check_inputs_overflow(tx) && check_outs_overflow(tx);
  }
  //---------------------------------------------------------------
  bool check_inputs_overflow(const Transaction& tx)
  {
    uint64_t money = 0;

    for (const auto& in : tx.vin) {
      uint64_t amount = 0;

      if (in.type() == typeid(TransactionInputToKey)) {
        amount = boost::get<TransactionInputToKey>(in).amount;
      } else if (in.type() == typeid(TransactionInputMultisignature)) {
        amount = boost::get<TransactionInputMultisignature>(in).amount;
      }

      if (money > amount + money)
        return false;

      money += amount;
    }
    return true;
  }
  //---------------------------------------------------------------
  bool check_outs_overflow(const Transaction& tx)
  {
    uint64_t money = 0;
    for (const auto& o : tx.vout) {
      if(money > o.amount + money)
        return false;
      money += o.amount;
    }
    return true;
  }
  //---------------------------------------------------------------
  uint64_t get_outs_money_amount(const Transaction& tx)
  {
    uint64_t outputs_amount = 0;
    for (const auto& o : tx.vout) {
      outputs_amount += o.amount;
    }
    return outputs_amount;
  }
  //---------------------------------------------------------------
  std::string short_hash_str(const crypto::hash& h)
  {
    std::string res = string_tools::pod_to_hex(h);
    CHECK_AND_ASSERT_MES(res.size() == 64, res, "wrong hash256 with string_tools::pod_to_hex conversion");
    auto erased_pos = res.erase(8, 48);
    res.insert(8, "....");
    return res;
  }

  //---------------------------------------------------------------
  bool is_out_to_acc(const account_keys& acc, const TransactionOutputToKey& out_key, const crypto::key_derivation& derivation, size_t keyIndex)
  {
    crypto::public_key pk;
    derive_public_key(derivation, keyIndex, acc.m_account_address.m_spendPublicKey, pk);
    return pk == out_key.key;
  }

  //---------------------------------------------------------------
  bool is_out_to_acc(const account_keys& acc, const TransactionOutputToKey& out_key, const crypto::public_key& tx_pub_key, size_t keyIndex)
  {
    crypto::key_derivation derivation;
    generate_key_derivation(tx_pub_key, acc.m_view_secret_key, derivation);
    return is_out_to_acc(acc, out_key, derivation, keyIndex);
  }

  //---------------------------------------------------------------
  bool lookup_acc_outs(const account_keys& acc, const Transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered)
  {
    crypto::public_key tx_pub_key = get_tx_pub_key_from_extra(tx);
    if(null_pkey == tx_pub_key)
      return false;
    return lookup_acc_outs(acc, tx, tx_pub_key, outs, money_transfered);
  }
  //---------------------------------------------------------------
  bool lookup_acc_outs(const account_keys& acc, const Transaction& tx, const crypto::public_key& tx_pub_key, std::vector<size_t>& outs, uint64_t& money_transfered)
  {
    money_transfered = 0;
    size_t keyIndex = 0;
    size_t outputIndex = 0;

    crypto::key_derivation derivation;
    generate_key_derivation(tx_pub_key, acc.m_view_secret_key, derivation);

    for (const TransactionOutput& o : tx.vout) {
      assert(o.target.type() == typeid(TransactionOutputToKey) || o.target.type() == typeid(TransactionOutputMultisignature));
      if (o.target.type() == typeid(TransactionOutputToKey)) {
        if (is_out_to_acc(acc, boost::get<TransactionOutputToKey>(o.target), derivation, keyIndex)) {
          outs.push_back(outputIndex);
          money_transfered += o.amount;
        }

        ++keyIndex;
      } else if (o.target.type() == typeid(TransactionOutputMultisignature)) {
        keyIndex += boost::get<TransactionOutputMultisignature>(o.target).keys.size();
      }

      ++outputIndex;
    }
    return true;
  }
  //---------------------------------------------------------------
  void get_blob_hash(const blobdata& blob, crypto::hash& res)
  {
    cn_fast_hash(blob.data(), blob.size(), res);
  }
  //---------------------------------------------------------------
  crypto::hash get_blob_hash(const blobdata& blob)
  {
    crypto::hash h = null_hash;
    get_blob_hash(blob, h);
    return h;
  }
  //---------------------------------------------------------------
  crypto::hash get_transaction_hash(const Transaction& t)
  {
    crypto::hash h = null_hash;
    size_t blob_size = 0;
    get_object_hash(t, h, blob_size);
    return h;
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const Transaction& t, crypto::hash& res)
  {
    size_t blob_size = 0;
    return get_object_hash(t, res, blob_size);
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const Transaction& t, crypto::hash& res, size_t& blob_size)
  {
    return get_object_hash(t, res, blob_size);
  }
  //---------------------------------------------------------------
  bool get_block_hashing_blob(const Block& b, blobdata& blob) {
    if (!t_serializable_object_to_blob(static_cast<const BlockHeader&>(b), blob)) {
      return false;
    }
    crypto::hash tree_root_hash = get_tx_tree_hash(b);
    blob.append(reinterpret_cast<const char*>(&tree_root_hash), sizeof(tree_root_hash));
    blob.append(tools::get_varint_data(b.txHashes.size() + 1));

    return true;
  }
  //---------------------------------------------------------------
  bool get_block_hash(const Block& b, crypto::hash& res) {
    blobdata blob;
    if (!get_block_hashing_blob(b, blob)) {
      return false;
    }

    return get_object_hash(blob, res);
  }
  //---------------------------------------------------------------
  crypto::hash get_block_hash(const Block& b) {
    crypto::hash p = null_hash;
    get_block_hash(b, p);
    return p;
  }
  //---------------------------------------------------------------
  bool get_aux_block_header_hash(const Block& b, crypto::hash& res) {
    blobdata blob;
    if (!get_block_hashing_blob(b, blob)) {
      return false;
    }

    return get_object_hash(blob, res);
  }
  //---------------------------------------------------------------
  bool get_block_longhash(crypto::cn_context &context, const Block& b, crypto::hash& res) {
    blobdata bd;
    if (!get_block_hashing_blob(b, bd)) {
      return false;
    }

    crypto::cn_slow_hash(context, bd.data(), bd.size(), res);
    return true;
  }
  //---------------------------------------------------------------
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off)
  {
    std::vector<uint64_t> res = off;
    for(size_t i = 1; i < res.size(); i++)
      res[i] += res[i-1];
    return res;
  }
  //---------------------------------------------------------------
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off)
  {
    std::vector<uint64_t> res = off;
    if(!off.size())
      return res;
    std::sort(res.begin(), res.end());//just to be sure, actually it is already should be sorted
    for(size_t i = res.size()-1; i != 0; i--)
      res[i] -= res[i-1];

    return res;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, Block& b)
  {
    std::stringstream ss;
    ss << b_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, b);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse block from blob");
    return true;
  }
  //---------------------------------------------------------------
  blobdata block_to_blob(const Block& b)
  {
    return t_serializable_object_to_blob(b);
  }
  //---------------------------------------------------------------
  bool block_to_blob(const Block& b, blobdata& b_blob)
  {
    return t_serializable_object_to_blob(b, b_blob);
  }
  //---------------------------------------------------------------
  blobdata tx_to_blob(const Transaction& tx)
  {
    return t_serializable_object_to_blob(tx);
  }
  //---------------------------------------------------------------
  bool tx_to_blob(const Transaction& tx, blobdata& b_blob)
  {
    return t_serializable_object_to_blob(tx, b_blob);
  }
  //---------------------------------------------------------------
  void get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes, crypto::hash& h)
  {
    tree_hash(tx_hashes.data(), tx_hashes.size(), h);
  }
  //---------------------------------------------------------------
  crypto::hash get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes)
  {
    crypto::hash h = null_hash;
    get_tx_tree_hash(tx_hashes, h);
    return h;
  }
  //---------------------------------------------------------------
  crypto::hash get_tx_tree_hash(const Block& b)
  {
    std::vector<crypto::hash> txs_ids;
    crypto::hash h = null_hash;
    size_t bl_sz = 0;
    get_transaction_hash(b.minerTx, h, bl_sz);
    txs_ids.push_back(h);
    for (auto& th : b.txHashes) {
      txs_ids.push_back(th);
    }
    return get_tx_tree_hash(txs_ids);
  }
  //---------------------------------------------------------------
}
