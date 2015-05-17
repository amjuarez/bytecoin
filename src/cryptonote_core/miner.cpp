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

#include <sstream>
#include <numeric>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <boost/limits.hpp>
#include <boost/utility/value_init.hpp>

#include "misc_language.h"
#include "include_base_utils.h"
#include "cryptonote_basic_impl.h"
#include "cryptonote_format_utils.h"
#include "file_io_utils.h"
#include "common/command_line.h"
#include "crypto/hash.h"
#include "crypto/random.h"
#include "string_coding.h"
#include "storages/portable_storage_template_helper.h"

using namespace epee;

#include "miner.h"
#include <thread>
#include <future>

namespace cryptonote
{

  miner::miner(const Currency& currency, i_miner_handler* phandler):
    m_currency(currency),
    m_stop(1),
    m_template(boost::value_initialized<Block>()),
    m_template_no(0),
    m_diffic(0),
    m_thread_index(0),
    m_phandler(phandler),
    m_pausers_count(0),
    m_threads_total(0),
    m_starter_nonce(0),
    m_last_hr_merge_time(0),
    m_hashes(0),
    m_do_print_hashrate(false),
    m_do_mining(false),
    m_current_hash_rate(0)
  {
  }
  //-----------------------------------------------------------------------------------------------------
  miner::~miner() {
    stop();
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::set_block_template(const Block& bl, const difficulty_type& di) {
    CRITICAL_REGION_LOCAL(m_template_lock);
    m_template = bl;

    if (BLOCK_MAJOR_VERSION_2 == m_template.majorVersion) {
      cryptonote::tx_extra_merge_mining_tag mm_tag;
      mm_tag.depth = 0;
      if (!cryptonote::get_aux_block_header_hash(m_template, mm_tag.merkle_root)) {
        return false;
      }

      m_template.parentBlock.minerTx.extra.clear();
      if (!cryptonote::append_mm_tag_to_extra(m_template.parentBlock.minerTx.extra, mm_tag)) {
        return false;
      }
    }

    m_diffic = di;
    ++m_template_no;
    m_starter_nonce = crypto::rand<uint32_t>();
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_block_chain_update() {
    if (!is_mining()) {
      return true;
    }

    return request_block_template();
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::request_block_template() {
    Block bl = AUTO_VAL_INIT(bl);
    difficulty_type di = AUTO_VAL_INIT(di);
    uint64_t height;
    cryptonote::blobdata extra_nonce;
    if(m_extra_messages.size() && m_config.current_extra_message_index < m_extra_messages.size())
    {
      extra_nonce = m_extra_messages[m_config.current_extra_message_index];
    }

    if(!m_phandler->get_block_template(bl, m_mine_address, di, height, extra_nonce))
    {
      LOG_ERROR("Failed to get_block_template(), stopping mining");
      return false;
    }
    set_block_template(bl, di);
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_idle()
  {
    m_update_block_template_interval.do_call([&](){
      if(is_mining())request_block_template();
      return true;
    });

    m_update_merge_hr_interval.do_call([&](){
      merge_hr();
      return true;
    });

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::do_print_hashrate(bool do_hr)
  {
    m_do_print_hashrate = do_hr;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::merge_hr()
  {
    if(m_last_hr_merge_time && is_mining())
    {
      m_current_hash_rate = m_hashes * 1000 / ((misc_utils::get_tick_count() - m_last_hr_merge_time + 1));
      CRITICAL_REGION_LOCAL(m_last_hash_rates_lock);
      m_last_hash_rates.push_back(m_current_hash_rate);
      if(m_last_hash_rates.size() > 19)
        m_last_hash_rates.pop_front();
      if(m_do_print_hashrate)
      {
        uint64_t total_hr = std::accumulate(m_last_hash_rates.begin(), m_last_hash_rates.end(), 0);
        float hr = static_cast<float>(total_hr)/static_cast<float>(m_last_hash_rates.size());
        std::cout << "hashrate: " << std::setprecision(4) << std::fixed << hr << ENDL;
      }
    }
    m_last_hr_merge_time = misc_utils::get_tick_count();
    m_hashes = 0;
  }

  bool miner::init(const MinerConfig& config) {
    if (!config.extraMessages.empty()) {
      std::string buff;
      bool r = file_io_utils::load_file_to_string(config.extraMessages, buff);
      CHECK_AND_ASSERT_MES(r, false, "Failed to load file with extra messages: " << config.extraMessages);
      std::vector<std::string> extra_vec;
      boost::split(extra_vec, buff, boost::is_any_of("\n"), boost::token_compress_on );
      m_extra_messages.resize(extra_vec.size());
      for(size_t i = 0; i != extra_vec.size(); i++) {
        string_tools::trim(extra_vec[i]);
        if(!extra_vec[i].size())
          continue;
        std::string buff = string_encoding::base64_decode(extra_vec[i]);
        if(buff != "0")
          m_extra_messages[i] = buff;
      }
      m_config_folder_path = boost::filesystem::path(config.extraMessages).parent_path().string();
      m_config = AUTO_VAL_INIT(m_config);
      epee::serialization::load_t_from_json_file(m_config, m_config_folder_path + "/" + cryptonote::parameters::MINER_CONFIG_FILE_NAME);
      LOG_PRINT_L0("Loaded " << m_extra_messages.size() << " extra messages, current index " << m_config.current_extra_message_index);
    }

    if(!config.startMining.empty()) {
      if (!m_currency.parseAccountAddressString(config.startMining, m_mine_address)) {
        LOG_ERROR("Target account address " << config.startMining << " has wrong format, starting daemon canceled");
        return false;
      }
      m_threads_total = 1;
      m_do_mining = true;
      if(config.miningThreads > 0) {
        m_threads_total = config.miningThreads;
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::is_mining()
  {
    return !m_stop;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::start(const AccountPublicAddress& adr, size_t threads_count, const boost::thread::attributes& attrs)
  {
    m_mine_address = adr;
    m_threads_total = static_cast<uint32_t>(threads_count);
    m_starter_nonce = crypto::rand<uint32_t>();
    CRITICAL_REGION_LOCAL(m_threads_lock);
    if(is_mining())
    {
      LOG_ERROR("Starting miner but it's already started");
      return false;
    }

    if(!m_threads.empty())
    {
      LOG_ERROR("Unable to start miner because there are active mining threads");
      return false;
    }

    if(!m_template_no)
      request_block_template();//lets update block template

    boost::interprocess::ipcdetail::atomic_write32(&m_stop, 0);
    boost::interprocess::ipcdetail::atomic_write32(&m_thread_index, 0);

    for(size_t i = 0; i != threads_count; i++)
    {
      m_threads.push_back(boost::thread(attrs, boost::bind(&miner::worker_thread, this)));
    }

    LOG_PRINT_L0("Mining has started with " << threads_count << " threads, good luck!" )
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  uint64_t miner::get_speed()
  {
    if(is_mining())
      return m_current_hash_rate;
    else
      return 0;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::send_stop_signal()
  {
    boost::interprocess::ipcdetail::atomic_write32(&m_stop, 1);
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::stop()
  {
    send_stop_signal();
    CRITICAL_REGION_LOCAL(m_threads_lock);

    BOOST_FOREACH(boost::thread& th, m_threads)
      th.join();

    m_threads.clear();
    LOG_PRINT_L0("Mining has been stopped, " << m_threads.size() << " finished" );
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::find_nonce_for_given_block(crypto::cn_context &context, Block& bl, const difficulty_type& diffic) {

    unsigned nthreads = std::thread::hardware_concurrency();

    if (nthreads > 0 && diffic > 5) {
      std::vector<std::future<void>> threads(nthreads);
      std::atomic<uint32_t> foundNonce;
      std::atomic<bool> found(false);
      uint32_t startNonce = crypto::rand<uint32_t>();

      for (unsigned i = 0; i < nthreads; ++i) {
        threads[i] = std::async(std::launch::async, [&, i]() {
          crypto::cn_context localctx;
          crypto::hash h;

          Block lb(bl); // copy to local block

          for (uint32_t nonce = startNonce + i; !found; nonce += nthreads) {
            lb.nonce = nonce;

            if (!get_block_longhash(localctx, lb, h)) {
              return;
            }

            if (check_hash(h, diffic)) {
              foundNonce = nonce;
              found = true;
              return;
            }
          }
        });
      }

      for (auto& t : threads) {
        t.wait();
      }

      if (found) {
        bl.nonce = foundNonce.load();
      }

      return found;
    } else {
      for (; bl.nonce != std::numeric_limits<uint32_t>::max(); bl.nonce++) {
        crypto::hash h;
        if (!get_block_longhash(context, bl, h)) {
          return false;
        }

        if (check_hash(h, diffic)) {
          return true;
        }
      }
    }

    return false;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::on_synchronized()
  {
    if(m_do_mining)
    {
      boost::thread::attributes attrs;
      attrs.set_stack_size(THREAD_STACK_SIZE);

      start(m_mine_address, m_threads_total, attrs);
    }
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::pause()
  {
    CRITICAL_REGION_LOCAL(m_miners_count_lock);
    ++m_pausers_count;
    if(m_pausers_count == 1 && is_mining())
      LOG_PRINT_L2("MINING PAUSED");
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::resume()
  {
    CRITICAL_REGION_LOCAL(m_miners_count_lock);
    --m_pausers_count;
    if(m_pausers_count < 0)
    {
      m_pausers_count = 0;
      LOG_PRINT_RED_L0("Unexpected miner::resume() called");
    }
    if(!m_pausers_count && is_mining())
      LOG_PRINT_L2("MINING RESUMED");
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::worker_thread()
  {
    uint32_t th_local_index = boost::interprocess::ipcdetail::atomic_inc32(&m_thread_index);
    LOG_PRINT_L0("Miner thread was started ["<< th_local_index << "]");
    log_space::log_singletone::set_thread_log_prefix(std::string("[miner ") + std::to_string(th_local_index) + "]");
    uint32_t nonce = m_starter_nonce + th_local_index;
    difficulty_type local_diff = 0;
    uint32_t local_template_ver = 0;
    crypto::cn_context context;
    Block b;
    while(!m_stop)
    {
      if(m_pausers_count)//anti split workaround
      {
        misc_utils::sleep_no_w(100);
        continue;
      }

      if(local_template_ver != m_template_no)
      {

        CRITICAL_REGION_BEGIN(m_template_lock);
        b = m_template;
        local_diff = m_diffic;
        CRITICAL_REGION_END();
        local_template_ver = m_template_no;
        nonce = m_starter_nonce + th_local_index;
      }

      if(!local_template_ver)//no any set_block_template call
      {
        LOG_PRINT_L2("Block template not set yet");
        epee::misc_utils::sleep_no_w(1000);
        continue;
      }

      b.nonce = nonce;
      crypto::hash h;
      if (!m_stop && !get_block_longhash(context, b, h)) {
        LOG_ERROR("Failed to get block long hash");
        m_stop = true;
      }

      if (!m_stop && check_hash(h, local_diff))
      {
        //we lucky!
        ++m_config.current_extra_message_index;
        LOG_PRINT_GREEN("Found block for difficulty: " << local_diff, LOG_LEVEL_0);
        if(!m_phandler->handle_block_found(b))
        {
          --m_config.current_extra_message_index;
        }else
        {
          //success update, lets update config
          epee::serialization::store_t_to_json_file(m_config, m_config_folder_path + "/" + cryptonote::parameters::MINER_CONFIG_FILE_NAME);
        }
      }

      nonce+=m_threads_total;
      ++m_hashes;
    }
    LOG_PRINT_L0("Miner thread stopped ["<< th_local_index << "]");
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
}
