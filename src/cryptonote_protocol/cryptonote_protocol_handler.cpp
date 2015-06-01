// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "cryptonote_protocol_handler.h"

#include <future>
#include <boost/scope_exit.hpp>
#include <System/Dispatcher.h>

#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/verification_context.h"
#include "p2p/LevinProtocol.h"

using namespace Logging;

namespace CryptoNote {

namespace {

template<class t_parametr>
bool post_notify(i_p2p_endpoint& p2p, typename t_parametr::request& arg, cryptonote_connection_context& context) {
  return p2p.invoke_notify_to_peer(t_parametr::ID, LevinProtocol::encode(arg), context);
}

template<class t_parametr>
void relay_post_notify(i_p2p_endpoint& p2p, typename t_parametr::request& arg, const net_connection_id* excludeConnection = nullptr) {
  p2p.relay_notify_to_all(t_parametr::ID, LevinProtocol::encode(arg), excludeConnection);
}

}

cryptonote_protocol_handler::cryptonote_protocol_handler(const Currency& currency, System::Dispatcher& dispatcher, ICore& rcore, i_p2p_endpoint* p_net_layout, Logging::ILogger& log) :
  m_dispatcher(dispatcher),
  m_currency(currency),
  m_core(rcore),
  m_p2p(p_net_layout),
  m_synchronized(false),
  m_stop(false),
  m_observedHeight(0),
  m_peersCount(0),
  logger(log, "protocol") {
  
  if (!m_p2p) {
    m_p2p = &m_p2p_stub;
  }
}

size_t cryptonote_protocol_handler::getPeerCount() const {
  return m_peersCount;
}

void cryptonote_protocol_handler::set_p2p_endpoint(i_p2p_endpoint* p2p) {
  if (p2p)
    m_p2p = p2p;
  else
    m_p2p = &m_p2p_stub;
}

void cryptonote_protocol_handler::onConnectionOpened(cryptonote_connection_context& context) {
}

void cryptonote_protocol_handler::onConnectionClosed(cryptonote_connection_context& context) {
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);
    uint64_t prevHeight = m_observedHeight;
    recalculateMaxObservedHeight(context);
    if (prevHeight != m_observedHeight) {
      updated = true;
    }
  }

  if (updated) {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptonoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }

  if (context.m_state != cryptonote_connection_context::state_befor_handshake) {
    m_peersCount--;
    m_observerManager.notify(&ICryptonoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }
}

void cryptonote_protocol_handler::stop() {
  m_stop = true;
}

bool cryptonote_protocol_handler::start_sync(cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "Starting synchronization";

  if (context.m_state == cryptonote_connection_context::state_synchronizing) {
    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    m_core.get_short_chain_history(r.block_ids);
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return true;
}

bool cryptonote_protocol_handler::get_stat_info(core_stat_info& stat_inf) {
  return m_core.get_stat_info(stat_inf);
}

void cryptonote_protocol_handler::log_connections() {
  std::stringstream ss;

  ss << std::setw(25) << std::left << "Remote Host"
    << std::setw(20) << "Peer id"
    << std::setw(25) << "Recv/Sent (inactive,sec)"
    << std::setw(25) << "State"
    << std::setw(20) << "Livetime(seconds)" << ENDL;

  m_p2p->for_each_connection([&](const cryptonote_connection_context& cntxt, peerid_type peer_id) {
    ss << std::setw(25) << std::left << std::string(cntxt.m_is_income ? "[INC]" : "[OUT]") +
      Common::ipAddressToString(cntxt.m_remote_ip) + ":" + std::to_string(cntxt.m_remote_port)
      << std::setw(20) << std::hex << peer_id
      // << std::setw(25) << std::to_string(cntxt.m_recv_cnt) + "(" + std::to_string(time(NULL) - cntxt.m_last_recv) + ")" + "/" + std::to_string(cntxt.m_send_cnt) + "(" + std::to_string(time(NULL) - cntxt.m_last_send) + ")"
      << std::setw(25) << get_protocol_state_string(cntxt.m_state)
      << std::setw(20) << std::to_string(time(NULL) - cntxt.m_started) << ENDL;
  });
  logger(INFO) << "Connections: " << ENDL << ss.str();
}

uint64_t cryptonote_protocol_handler::get_current_blockchain_height() {
  uint64_t height;
  crypto::hash blockId;
  m_core.get_blockchain_top(height, blockId);
  return height;
}

bool cryptonote_protocol_handler::process_payload_sync_data(const CORE_SYNC_DATA& hshd, cryptonote_connection_context& context, bool is_inital) {
  if (context.m_state == cryptonote_connection_context::state_befor_handshake && !is_inital)
    return true;

  if (context.m_state == cryptonote_connection_context::state_synchronizing) {
  } else if (m_core.have_block(hshd.top_id)) {
    context.m_state = cryptonote_connection_context::state_normal;
    if (is_inital)
      on_connection_synchronized();
  } else {
    int64_t diff = static_cast<int64_t>(hshd.current_height) - static_cast<int64_t>(get_current_blockchain_height());

    logger(diff >= 0 ? (is_inital ? Logging::INFO : Logging::DEBUGGING) : Logging::TRACE, Logging::BRIGHT_YELLOW) << context <<
      "Sync data returned unknown top block: " << get_current_blockchain_height() << " -> " << hshd.current_height
      << " [" << std::abs(diff) << " blocks (" << std::abs(diff) / (24 * 60 * 60 / m_currency.difficultyTarget()) << " days) "
      << (diff >= 0 ? std::string("behind") : std::string("ahead")) << "] " << std::endl << "SYNCHRONIZATION started";

    logger(Logging::DEBUGGING) << "Remote top block height: " << hshd.current_height << ", id: " << hshd.top_id;
    //let the socket to send response to handshake, but request callback, to let send request data after response
    logger(Logging::TRACE) << context << "requesting synchronization";
    context.m_state = cryptonote_connection_context::state_sync_required;
  }

  updateObservedHeight(hshd.current_height, context);
  context.m_remote_blockchain_height = hshd.current_height;

  if (is_inital) {
    m_peersCount++;
    m_observerManager.notify(&ICryptonoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }

  return true;
}

bool cryptonote_protocol_handler::get_payload_sync_data(CORE_SYNC_DATA& hshd) {
  m_core.get_blockchain_top(hshd.current_height, hshd.top_id);
  hshd.current_height += 1;
  return true;
}


template <typename Command, typename Handler>
int notifyAdaptor(const std::string& reqBuf, cryptonote_connection_context& ctx, Handler handler) {

  typedef typename Command::request Request;
  int command = Command::ID;

  Request req = boost::value_initialized<Request>();
  if (!LevinProtocol::decode(reqBuf, req)) {
    throw std::runtime_error("Failed to load_from_binary in command " + std::to_string(command));
  }

  return handler(command, req, ctx);
}

#define HANDLE_NOTIFY(CMD, Handler) case CMD::ID: { ret = notifyAdaptor<CMD>(in, ctx, boost::bind(Handler, this, _1, _2, _3)); break; }

int cryptonote_protocol_handler::handleCommand(bool is_notify, int command, const std::string& in, std::string& out, cryptonote_connection_context& ctx, bool& handled) {
  int ret = 0;
  handled = true;

  switch (command) {
    HANDLE_NOTIFY(NOTIFY_NEW_BLOCK, &cryptonote_protocol_handler::handle_notify_new_block)
    HANDLE_NOTIFY(NOTIFY_NEW_TRANSACTIONS, &cryptonote_protocol_handler::handle_notify_new_transactions)
    HANDLE_NOTIFY(NOTIFY_REQUEST_GET_OBJECTS, &cryptonote_protocol_handler::handle_request_get_objects)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_GET_OBJECTS, &cryptonote_protocol_handler::handle_response_get_objects)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHAIN, &cryptonote_protocol_handler::handle_request_chain)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHAIN_ENTRY, &cryptonote_protocol_handler::handle_response_chain_entry)

  default:
    handled = false;
  }

  return ret;
}

#undef HANDLE_NOTIFY

int cryptonote_protocol_handler::handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request& arg, cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_BLOCK (hop " << arg.hop << ")";

  updateObservedHeight(arg.current_blockchain_height, context);

  context.m_remote_blockchain_height = arg.current_blockchain_height;

  if (context.m_state != cryptonote_connection_context::state_normal) {
    return 1;
  }

  for (auto tx_blob_it = arg.b.txs.begin(); tx_blob_it != arg.b.txs.end(); tx_blob_it++) {
    CryptoNote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
    m_core.handle_incoming_tx(*tx_blob_it, tvc, true);
    if (tvc.m_verifivation_failed) {
      logger(Logging::INFO) << context << "Block verification failed: transaction verification failed, dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }
  }

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  m_core.handle_incoming_block_blob(arg.b.block, bvc, true, false);
  if (bvc.m_verifivation_failed) {
    logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection";
    context.m_state = cryptonote_connection_context::state_shutdown;
    return 1;
  }
  if (bvc.m_added_to_main_chain) {
    ++arg.hop;
    //TODO: Add here announce protocol usage
    relay_post_notify<NOTIFY_NEW_BLOCK>(*m_p2p, arg, &context.m_connection_id);
    // relay_block(arg, context);
  } else if (bvc.m_marked_as_orphaned) {
    context.m_state = cryptonote_connection_context::state_synchronizing;
    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    m_core.get_short_chain_history(r.block_ids);
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return 1;
}

int cryptonote_protocol_handler::handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request& arg, cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_TRANSACTIONS";
  if (context.m_state != cryptonote_connection_context::state_normal)
    return 1;

  for (auto tx_blob_it = arg.txs.begin(); tx_blob_it != arg.txs.end();) {
    CryptoNote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
    m_core.handle_incoming_tx(*tx_blob_it, tvc, false);
    if (tvc.m_verifivation_failed) {
      logger(Logging::INFO) << context << "Tx verification failed, dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }
    if (tvc.m_should_be_relayed)
      ++tx_blob_it;
    else
      arg.txs.erase(tx_blob_it++);
  }

  if (arg.txs.size()) {
    //TODO: add announce usage here
    relay_post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, &context.m_connection_id);
  }

  return true;
}

int cryptonote_protocol_handler::handle_request_get_objects(int command, NOTIFY_REQUEST_GET_OBJECTS::request& arg, cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_GET_OBJECTS";
  NOTIFY_RESPONSE_GET_OBJECTS::request rsp;
  if (!m_core.handle_get_objects(arg, rsp)) {
    logger(Logging::ERROR) << context << "failed to handle request NOTIFY_REQUEST_GET_OBJECTS, dropping connection";
    context.m_state = cryptonote_connection_context::state_shutdown;
  }
  logger(Logging::TRACE) << context << "-->>NOTIFY_RESPONSE_GET_OBJECTS: blocks.size()=" << rsp.blocks.size() << ", txs.size()=" << rsp.txs.size()
    << ", rsp.m_current_blockchain_height=" << rsp.current_blockchain_height << ", missed_ids.size()=" << rsp.missed_ids.size();
  post_notify<NOTIFY_RESPONSE_GET_OBJECTS>(*m_p2p, rsp, context);
  return 1;
}

int cryptonote_protocol_handler::handle_response_get_objects(int command, NOTIFY_RESPONSE_GET_OBJECTS::request& arg, cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "NOTIFY_RESPONSE_GET_OBJECTS";

  if (context.m_last_response_height > arg.current_blockchain_height) {
    logger(Logging::ERROR) << context << "sent wrong NOTIFY_HAVE_OBJECTS: arg.m_current_blockchain_height=" << arg.current_blockchain_height
      << " < m_last_response_height=" << context.m_last_response_height << ", dropping connection";
    context.m_state = cryptonote_connection_context::state_shutdown;
    return 1;
  }

  updateObservedHeight(arg.current_blockchain_height, context);

  context.m_remote_blockchain_height = arg.current_blockchain_height;

  size_t count = 0;
  for (const block_complete_entry& block_entry : arg.blocks) {
    ++count;
    Block b;
    if (!parse_and_validate_block_from_blob(block_entry.block, b)) {
      logger(Logging::ERROR) << context << "sent wrong block: failed to parse and validate block: \r\n"
        << blobToHex(block_entry.block) << "\r\n dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    //to avoid concurrency in core between connections, suspend connections which delivered block later then first one
    if (count == 2) {
      if (m_core.have_block(get_block_hash(b))) {
        context.m_state = cryptonote_connection_context::state_idle;
        context.m_needed_objects.clear();
        context.m_requested_objects.clear();
        logger(Logging::DEBUGGING) << context << "Connection set to idle state.";
        return 1;
      }
    }

    auto req_it = context.m_requested_objects.find(get_block_hash(b));
    if (req_it == context.m_requested_objects.end()) {
      logger(Logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << Common::podToHex(get_blob_hash(block_entry.block))
        << " wasn't requested, dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }
    if (b.txHashes.size() != block_entry.txs.size()) {
      logger(Logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << Common::podToHex(get_blob_hash(block_entry.block))
        << ", txHashes.size()=" << b.txHashes.size() << " mismatch with block_complete_entry.m_txs.size()=" << block_entry.txs.size() << ", dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    }

    context.m_requested_objects.erase(req_it);
  }

  if (context.m_requested_objects.size()) {
    logger(Logging::ERROR, Logging::BRIGHT_RED) << context <<
      "returned not all requested objects (context.m_requested_objects.size()="
      << context.m_requested_objects.size() << "), dropping connection";
    context.m_state = cryptonote_connection_context::state_shutdown;
    return 1;
  }

  {
    m_core.pause_mining();

    BOOST_SCOPE_EXIT_ALL(this) { m_core.update_block_template_and_resume_mining(); };

    auto currentContext = m_dispatcher.getCurrentContext();

    auto resultFuture = std::async(std::launch::async, [&]{
      int result = processObjects(context, arg.blocks);
      m_dispatcher.remoteSpawn([&] {
        m_dispatcher.pushContext(currentContext);
      });

      return result;
    });

    m_dispatcher.dispatch();
    int result = resultFuture.get();
    if (result != 0) {
      return result;
    }
  }

  uint64_t height;
  crypto::hash top;
  m_core.get_blockchain_top(height, top);
  logger(INFO, BRIGHT_GREEN) << "Local blockchain updated, new height = " << height;

  if (!m_stop && context.m_state == cryptonote_connection_context::state_synchronizing) {
    request_missing_objects(context, true);
  }

  return 1;
}

int cryptonote_protocol_handler::processObjects(cryptonote_connection_context& context, const std::list<block_complete_entry>& blocks) {

  for (const block_complete_entry& block_entry : blocks) {
    if (m_stop) {
      break;
    }

    //process transactions
    for (auto& tx_blob : block_entry.txs) {
      tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
      m_core.handle_incoming_tx(tx_blob, tvc, true);
      if (tvc.m_verifivation_failed) {
        logger(Logging::ERROR) << context << "transaction verification failed on NOTIFY_RESPONSE_GET_OBJECTS, \r\ntx_id = "
          << Common::podToHex(get_blob_hash(tx_blob)) << ", dropping connection";
        context.m_state = cryptonote_connection_context::state_shutdown;
        return 1;
      }
    }

    // process block
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_core.handle_incoming_block_blob(block_entry.block, bvc, false, false);

    if (bvc.m_verifivation_failed) {
      logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    } else if (bvc.m_marked_as_orphaned) {
      logger(Logging::INFO) << context << "Block received at sync phase was marked as orphaned, dropping connection";
      context.m_state = cryptonote_connection_context::state_shutdown;
      return 1;
    } else if (bvc.m_already_exists) {
      logger(Logging::DEBUGGING) << context << "Block already exists, switching to idle state";
      context.m_state = cryptonote_connection_context::state_idle;
      return 1;
    }
  }

  return 0;

}


bool cryptonote_protocol_handler::on_idle() {
  return m_core.on_idle();
}

int cryptonote_protocol_handler::handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request& arg, cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << arg.block_ids.size();
  NOTIFY_RESPONSE_CHAIN_ENTRY::request r;
  if (!m_core.find_blockchain_supplement(arg.block_ids, r)) {
    logger(Logging::ERROR) << context << "Failed to handle NOTIFY_REQUEST_CHAIN.";
    return 1;
  }
  logger(Logging::TRACE) << context << "-->>NOTIFY_RESPONSE_CHAIN_ENTRY: m_start_height=" << r.start_height << ", m_total_height=" << r.total_height << ", m_block_ids.size()=" << r.m_block_ids.size();
  post_notify<NOTIFY_RESPONSE_CHAIN_ENTRY>(*m_p2p, r, context);
  return 1;
}

bool cryptonote_protocol_handler::request_missing_objects(cryptonote_connection_context& context, bool check_having_blocks) {
  if (context.m_needed_objects.size()) {
    //we know objects that we need, request this objects
    NOTIFY_REQUEST_GET_OBJECTS::request req;
    size_t count = 0;
    auto it = context.m_needed_objects.begin();

    while (it != context.m_needed_objects.end() && count < BLOCKS_SYNCHRONIZING_DEFAULT_COUNT) {
      if (!(check_having_blocks && m_core.have_block(*it))) {
        req.blocks.push_back(*it);
        ++count;
        context.m_requested_objects.insert(*it);
      }
      context.m_needed_objects.erase(it++);
    }
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_GET_OBJECTS: blocks.size()=" << req.blocks.size() << ", txs.size()=" << req.txs.size();
    post_notify<NOTIFY_REQUEST_GET_OBJECTS>(*m_p2p, req, context);
  } else if (context.m_last_response_height < context.m_remote_blockchain_height - 1) {//we have to fetch more objects ids, request blockchain entry

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    m_core.get_short_chain_history(r.block_ids);
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  } else {
    if (!(context.m_last_response_height ==
      context.m_remote_blockchain_height - 1 &&
      !context.m_needed_objects.size() &&
      !context.m_requested_objects.size())) {
      logger(Logging::ERROR, Logging::BRIGHT_RED)
        << "request_missing_blocks final condition failed!"
        << "\r\nm_last_response_height=" << context.m_last_response_height
        << "\r\nm_remote_blockchain_height=" << context.m_remote_blockchain_height
        << "\r\nm_needed_objects.size()=" << context.m_needed_objects.size()
        << "\r\nm_requested_objects.size()=" << context.m_requested_objects.size() 
        << "\r\non connection [" << context << "]";
      return false;
    }

    context.m_state = cryptonote_connection_context::state_normal;
    logger(Logging::INFO, Logging::BRIGHT_GREEN) << context << "SYNCHRONIZED OK";
    on_connection_synchronized();
  }
  return true;
}

bool cryptonote_protocol_handler::on_connection_synchronized() {
  bool val_expected = false;
  if (m_synchronized.compare_exchange_strong(val_expected, true)) {
    logger(Logging::INFO) << ENDL << "**********************************************************************" << ENDL
      << "You are now synchronized with the network. You may now start simplewallet." << ENDL
      << ENDL
      << "Please note, that the blockchain will be saved only after you quit the daemon with \"exit\" command or if you use \"save\" command." << ENDL
      << "Otherwise, you will possibly need to synchronize the blockchain again." << ENDL
      << ENDL
      << "Use \"help\" command to see the list of available commands." << ENDL
      << "**********************************************************************";
    m_core.on_synchronized();
  }
  return true;
}

int cryptonote_protocol_handler::handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request& arg, cryptonote_connection_context& context) {
  logger(Logging::TRACE) << context << "NOTIFY_RESPONSE_CHAIN_ENTRY: m_block_ids.size()=" << arg.m_block_ids.size()
    << ", m_start_height=" << arg.start_height << ", m_total_height=" << arg.total_height;

  if (!arg.m_block_ids.size()) {
    logger(Logging::ERROR) << context << "sent empty m_block_ids, dropping connection";
    context.m_state = cryptonote_connection_context::state_shutdown;
    return 1;
  }

  if (!m_core.have_block(arg.m_block_ids.front())) {
    logger(Logging::ERROR)
      << context << "sent m_block_ids starting from unknown id: "
      << Common::podToHex(arg.m_block_ids.front())
      << " , dropping connection";
    context.m_state = cryptonote_connection_context::state_shutdown;
    return 1;
  }

  context.m_remote_blockchain_height = arg.total_height;
  context.m_last_response_height = arg.start_height + arg.m_block_ids.size() - 1;

  if (context.m_last_response_height > context.m_remote_blockchain_height) {
    logger(Logging::ERROR)
      << context
      << "sent wrong NOTIFY_RESPONSE_CHAIN_ENTRY, with \r\nm_total_height="
      << arg.total_height << "\r\nm_start_height=" << arg.start_height
      << "\r\nm_block_ids.size()=" << arg.m_block_ids.size();
    context.m_state = cryptonote_connection_context::state_shutdown;
  }

  for (auto& bl_id : arg.m_block_ids) {
    if (!m_core.have_block(bl_id))
      context.m_needed_objects.push_back(bl_id);
  }

  request_missing_objects(context, false);
  return 1;
}

void cryptonote_protocol_handler::relay_block(NOTIFY_NEW_BLOCK::request& arg) {
  auto buf = LevinProtocol::encode(arg);
  m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_BLOCK::ID, buf);
}

void cryptonote_protocol_handler::relay_transactions(NOTIFY_NEW_TRANSACTIONS::request& arg) {
  auto buf = LevinProtocol::encode(arg);
  m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf);
}

void cryptonote_protocol_handler::updateObservedHeight(uint64_t peerHeight, const cryptonote_connection_context& context) {
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);

    uint64_t height = m_observedHeight;
    if (peerHeight > context.m_remote_blockchain_height) {
      m_observedHeight = std::max(m_observedHeight, peerHeight);
      if (m_observedHeight != height) {
        updated = true;
      }
    } else if (context.m_remote_blockchain_height == m_observedHeight) {
      //the client switched to alternative chain and had maximum observed height. need to recalculate max height
      recalculateMaxObservedHeight(context);
      if (m_observedHeight != height) {
        updated = true;
      }
    }
  }

  if (updated) {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptonoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }
}

void cryptonote_protocol_handler::recalculateMaxObservedHeight(const cryptonote_connection_context& context) {
  //should be locked outside
  uint64_t peerHeight = 0;
  m_p2p->for_each_connection([&peerHeight, &context](const cryptonote_connection_context& ctx, peerid_type peerId) {
    if (ctx.m_connection_id != context.m_connection_id) {
      peerHeight = std::max(peerHeight, ctx.m_remote_blockchain_height);
    }
  });

  uint64_t localHeight = 0;
  crypto::hash ignore;
  m_core.get_blockchain_top(localHeight, ignore);
  m_observedHeight = std::max(peerHeight, localHeight);
}

uint64_t cryptonote_protocol_handler::getObservedHeight() const {
  std::lock_guard<std::mutex> lock(m_observedHeightMutex);
  return m_observedHeight;
};

bool cryptonote_protocol_handler::addObserver(ICryptonoteProtocolObserver* observer) {
  return m_observerManager.add(observer);
}

bool cryptonote_protocol_handler::removeObserver(ICryptonoteProtocolObserver* observer) {
  return m_observerManager.remove(observer);
}

};
