// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>

#include <boost/program_options.hpp>

// epee
#include "serialization/keyvalue_serialization.h"
#include "math_helper.h"

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/difficulty.h"
#include "cryptonote_core/i_miner_handler.h"
#include "cryptonote_core/MinerConfig.h"

namespace cryptonote {
  class miner {
  public:
    miner(const Currency& currency, i_miner_handler* phandler);
    ~miner();

    bool init(const MinerConfig& config);
    bool set_block_template(const Block& bl, const difficulty_type& diffic);
    bool on_block_chain_update();
    bool start(const AccountPublicAddress& adr, size_t threads_count, const boost::thread::attributes& attrs);
    uint64_t get_speed();
    void send_stop_signal();
    bool stop();
    bool is_mining();
    bool on_idle();
    void on_synchronized();
    //synchronous analog (for fast calls)
    static bool find_nonce_for_given_block(crypto::cn_context &context, Block& bl, const difficulty_type& diffic);
    void pause();
    void resume();
    void do_print_hashrate(bool do_hr);

  private:
    bool worker_thread();
    bool request_block_template();
    void  merge_hr();

    struct miner_config
    {
      uint64_t current_extra_message_index;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(current_extra_message_index)
      END_KV_SERIALIZE_MAP()
    };


    const Currency& m_currency;
    volatile uint32_t m_stop;
    epee::critical_section m_template_lock;
    Block m_template;
    std::atomic<uint32_t> m_template_no;
    std::atomic<uint32_t> m_starter_nonce;
    difficulty_type m_diffic;
    volatile uint32_t m_thread_index;
    volatile uint32_t m_threads_total;
    std::atomic<int32_t> m_pausers_count;
    epee::critical_section m_miners_count_lock;

    std::list<boost::thread> m_threads;
    epee::critical_section m_threads_lock;
    i_miner_handler* m_phandler;
    AccountPublicAddress m_mine_address;
    epee::math_helper::once_a_time_seconds<5> m_update_block_template_interval;
    epee::math_helper::once_a_time_seconds<2> m_update_merge_hr_interval;
    std::vector<blobdata> m_extra_messages;
    miner_config m_config;
    std::string m_config_folder_path;
    std::atomic<uint64_t> m_last_hr_merge_time;
    std::atomic<uint64_t> m_hashes;
    std::atomic<uint64_t> m_current_hash_rate;
    epee::critical_section m_last_hash_rates_lock;
    std::list<uint64_t> m_last_hash_rates;
    bool m_do_print_hashrate;
    bool m_do_mining;
  };
}
