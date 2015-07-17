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

#include "simplewallet.h"

#include <fstream>
#include <future>
#include <thread>
#include <set>
#include <sstream>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "Common/command_line.h"
#include "Common/SignalHandler.h"
#include "Common/PathTools.h"
#include "Common/util.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "node_rpc_proxy/NodeRpcProxy.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "rpc/HttpClient.h"

#include "wallet/wallet_rpc_server.h"
#include "wallet/Wallet.h"
#include "wallet/LegacyKeysImporter.h"
#include "wallet/WalletHelper.h"

#include "version.h"

#include <Logging/LoggerManager.h>

#if defined(WIN32)
#include <crtdbg.h>
#endif

using namespace CryptoNote;
using namespace Logging;
using Common::JsonValue;

namespace po = boost::program_options;

#define EXTENDED_LOGS_FILE "wallet_details.log"
#undef ERROR

namespace {

const command_line::arg_descriptor<std::string> arg_wallet_file = { "wallet-file", "Use wallet <arg>", "" };
const command_line::arg_descriptor<std::string> arg_generate_new_wallet = { "generate-new-wallet", "Generate new wallet and save it to <arg>", "" };
const command_line::arg_descriptor<std::string> arg_daemon_address = { "daemon-address", "Use daemon instance at <host>:<port>", "" };
const command_line::arg_descriptor<std::string> arg_daemon_host = { "daemon-host", "Use daemon instance at host <arg> instead of localhost", "" };
const command_line::arg_descriptor<std::string> arg_password = { "password", "Wallet password", "", true };
const command_line::arg_descriptor<uint16_t> arg_daemon_port = { "daemon-port", "Use daemon instance at port <arg> instead of 8081", 0 };
const command_line::arg_descriptor<uint32_t> arg_log_level = { "set_log", "", INFO, true };
  const command_line::arg_descriptor<bool>      arg_SYNC_FROM_ZERO  = {"SYNC_FROM_ZERO", "Sync from block 0. Use for premine wallet or brainwallet", false};
  const command_line::arg_descriptor<std::string> arg_config_file = {"config-file", "Specify configuration file", "./configs/-.conf"};  
  const command_line::arg_descriptor<std::string> arg_GENESIS_COINBASE_TX_HEX  = {"GENESIS_COINBASE_TX_HEX", "Genesis transaction hex", CryptoNote::parameters::GENESIS_COINBASE_TX_HEX};  
  const command_line::arg_descriptor<uint64_t>    arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX  = {"CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX", "uint64_t", CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX};
  const command_line::arg_descriptor<uint64_t>    arg_MONEY_SUPPLY  = {"MONEY_SUPPLY", "uint64_t", CryptoNote::parameters::MONEY_SUPPLY};
  const command_line::arg_descriptor<unsigned int>    arg_EMISSION_SPEED_FACTOR  = {"EMISSION_SPEED_FACTOR", "unsigned int", CryptoNote::parameters::EMISSION_SPEED_FACTOR};
  const command_line::arg_descriptor<uint64_t>    arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE  = {"CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE", "uint64_t", CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE};
  const command_line::arg_descriptor<uint64_t>    arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT  = {"CRYPTONOTE_DISPLAY_DECIMAL_POINT", "size_t", CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT};
  const command_line::arg_descriptor<uint64_t>    arg_MINIMUM_FEE  = {"MINIMUM_FEE", "uint64_t", CryptoNote::parameters::MINIMUM_FEE};
  const command_line::arg_descriptor<uint64_t>    arg_DEFAULT_DUST_THRESHOLD  = {"DEFAULT_DUST_THRESHOLD", "uint64_t", CryptoNote::parameters::DEFAULT_DUST_THRESHOLD};
  const command_line::arg_descriptor<uint64_t>    arg_DIFFICULTY_TARGET  = {"DIFFICULTY_TARGET", "uint64_t", CryptoNote::parameters::DIFFICULTY_TARGET};
  const command_line::arg_descriptor<size_t>      arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW  = {"CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW", "size_t", CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW};
  const command_line::arg_descriptor<uint64_t>    arg_MAX_BLOCK_SIZE_INITIAL  = {"MAX_BLOCK_SIZE_INITIAL", "uint64_t", CryptoNote::parameters::MAX_BLOCK_SIZE_INITIAL};
  const command_line::arg_descriptor<uint64_t>    arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY  = {"EXPECTED_NUMBER_OF_BLOCKS_PER_DAY", "uint64_t"};
  const command_line::arg_descriptor<uint64_t>    arg_UPGRADE_HEIGHT  = {"UPGRADE_HEIGHT", "uint64_t", 0};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_CUT  = {"DIFFICULTY_CUT", "uint64_t", CryptoNote::parameters::DIFFICULTY_CUT};
  const command_line::arg_descriptor<size_t>      arg_DIFFICULTY_LAG  = {"DIFFICULTY_LAG", "uint64_t", CryptoNote::parameters::DIFFICULTY_LAG};
  const command_line::arg_descriptor<std::string> arg_rpc_bind_port = {"rpc-bind-port", "", std::to_string(RPC_DEFAULT_PORT)};
const command_line::arg_descriptor<bool> arg_testnet = { "testnet", "Used to deploy test nets. The daemon must be launched with --testnet flag", false };
const command_line::arg_descriptor< std::vector<std::string> > arg_command = { "command", "" };


bool parseUrlAddress(const std::string& url, std::string& address, uint16_t& port) {

  auto pos = url.find("://");
  size_t addrStart = 0;

  if (pos == std::string::npos) {
    pos = 0;
  } else {
    addrStart = pos + 3;
  }

  auto addrEnd = url.find(':', addrStart);

  if (addrEnd != std::string::npos) {
    auto portEnd = url.find('/', addrEnd);
    port = Common::fromString<uint16_t>(url.substr(
      addrEnd + 1, portEnd == std::string::npos ? std::string::npos : portEnd - addrEnd - 1));
  } else {
    addrEnd = url.find('/');
    port = 80;
  }

  address = url.substr(addrStart, addrEnd - addrStart);
  return true;
}


inline std::string interpret_rpc_response(bool ok, const std::string& status) {
  std::string err;
  if (ok) {
    if (status == CORE_RPC_STATUS_BUSY) {
      err = "daemon is busy. Please try later";
    } else if (status != CORE_RPC_STATUS_OK) {
      err = status;
    }
  } else {
    err = "possible lost connection to daemon";
  }
  return err;
}

template <typename IterT, typename ValueT = typename IterT::value_type>
class ArgumentReader {
public:

  ArgumentReader(IterT begin, IterT end) :
    m_begin(begin), m_end(end), m_cur(begin) {
  }

  bool eof() const {
    return m_cur == m_end;
  }

  ValueT next() {
    if (eof()) {
      throw std::runtime_error("unexpected end of arguments");
    }

    return *m_cur++;
  }

private:

  IterT m_cur;
  IterT m_begin;
  IterT m_end;
};

struct TransferCommand {
  const CryptoNote::Currency& m_currency;
  size_t fake_outs_count;
  std::vector<CryptoNote::Transfer> dsts;
  std::vector<uint8_t> extra;
  uint64_t fee;

  TransferCommand(const CryptoNote::Currency& currency) :
    m_currency(currency), fake_outs_count(0), fee(currency.minimumFee()) {
  }

  bool parseArguments(LoggerRef& logger, const std::vector<std::string> &args) {

    ArgumentReader<std::vector<std::string>::const_iterator> ar(args.begin(), args.end());

    try {

      auto mixin_str = ar.next();

      if (!Common::fromString(mixin_str, fake_outs_count)) {
        logger(ERROR, BRIGHT_RED) << "mixin_count should be non-negative integer, got " << mixin_str;
        return false;
      }

      while (!ar.eof()) {

        auto arg = ar.next();

        if (arg.size() && arg[0] == '-') {

          const auto& value = ar.next();

          if (arg == "-p") {
            if (!createTxExtraWithPaymentId(value, extra)) {
              logger(ERROR, BRIGHT_RED) << "payment ID has invalid format: \"" << value << "\", expected 64-character string";
              return false;
            }
          } else if (arg == "-f") {
            bool ok = m_currency.parseAmount(value, fee);
            if (!ok) {
              logger(ERROR, BRIGHT_RED) << "Fee value is invalid: " << value;
              return false;
            }

            if (fee < m_currency.minimumFee()) {
              logger(ERROR, BRIGHT_RED) << "Fee value is less than minimum: " << m_currency.minimumFee();
              return false;
            }
          }
        } else {
          Transfer destination;
          CryptoNote::tx_destination_entry de;

          if (!m_currency.parseAccountAddressString(arg, de.addr)) {
            crypto::hash paymentId;
            if (CryptoNote::parsePaymentId(arg, paymentId)) {
              logger(ERROR, BRIGHT_RED) << "Invalid payment ID usage. Please, use -p <payment_id>. See help for details.";
            } else {
              logger(ERROR, BRIGHT_RED) << "Wrong address: " << arg;
            }

            return false;
          }

          auto value = ar.next();
          bool ok = m_currency.parseAmount(value, de.amount);
          if (!ok || 0 == de.amount) {
            logger(ERROR, BRIGHT_RED) << "amount is wrong: " << arg << ' ' << value <<
              ", expected number from 0 to " << m_currency.formatAmount(std::numeric_limits<uint64_t>::max());
            return false;
          }
          destination.address = arg;
          destination.amount = de.amount;

          dsts.push_back(destination);
        }
      }

      if (dsts.empty()) {
        logger(ERROR, BRIGHT_RED) << "At least one destination address is required";
        return false;
      }
    } catch (const std::exception& e) {
      logger(ERROR, BRIGHT_RED) << e.what();
      return false;
    }

    return true;
  }
};

JsonValue buildLoggerConfiguration(Level level, const std::string& logfile) {
  JsonValue loggerConfiguration(JsonValue::OBJECT);
  loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

  JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

  JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  consoleLogger.insert("type", "console");
  consoleLogger.insert("level", static_cast<int64_t>(TRACE));
  consoleLogger.insert("pattern", "");

  JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  fileLogger.insert("type", "file");
  fileLogger.insert("filename", logfile);
  fileLogger.insert("level", static_cast<int64_t>(TRACE));

  return loggerConfiguration;
}

std::error_code initAndLoadWallet(IWallet& wallet, std::istream& walletFile, const std::string& password) {
  WalletHelper::InitWalletResultObserver initObserver;
  std::future<std::error_code> f_initError = initObserver.initResult.get_future();

  WalletHelper::IWalletRemoveObserverGuard removeGuard(wallet, initObserver);
  wallet.initAndLoad(walletFile, password);
  auto initError = f_initError.get();

  return initError;
}

std::string tryToOpenWalletOrLoadKeysOrThrow(LoggerRef& logger, std::unique_ptr<IWallet>& wallet, const std::string& walletFile, const std::string& password) {
  std::string keys_file, walletFileName;
  WalletHelper::prepareFileNames(walletFile, keys_file, walletFileName);

  boost::system::error_code ignore;
  bool keysExists = boost::filesystem::exists(keys_file, ignore);
  bool walletExists = boost::filesystem::exists(walletFileName, ignore);
  if (!walletExists && !keysExists && boost::filesystem::exists(walletFile, ignore)) {
    auto replaceEc = tools::replace_file(walletFile, walletFileName);
    if (replaceEc) {
      throw std::runtime_error("failed to rename file '" + walletFile + "' to '" + walletFileName + "'");
    }

    walletExists = true;
  }

  if (walletExists) {
    logger(INFO) << "Loading wallet...";
    std::ifstream walletFile;
    walletFile.open(walletFileName, std::ios_base::binary | std::ios_base::in);
    if (walletFile.fail()) {
      throw std::runtime_error("error opening wallet file '" + walletFileName + "'");
    }

    auto initError = initAndLoadWallet(*wallet, walletFile, password);

    walletFile.close();
    if (initError) { //bad password, or legacy format
      if (keysExists) {
        std::stringstream ss;
        CryptoNote::importLegacyKeys(keys_file, password, ss);
        boost::filesystem::rename(keys_file, keys_file + ".back");
        boost::filesystem::rename(walletFileName, walletFileName + ".back");

        initError = initAndLoadWallet(*wallet, ss, password);
        if (initError) {
          throw std::runtime_error("failed to load wallet: " + initError.message());
        }

        logger(INFO) << "Storing wallet...";

        try {
          CryptoNote::WalletHelper::storeWallet(*wallet, walletFileName);
        } catch (std::exception& e) {
          logger(ERROR, BRIGHT_RED) << "Failed to store wallet: " << e.what();
          throw std::runtime_error("error saving wallet file '" + walletFileName + "'");
        }

        logger(INFO, BRIGHT_GREEN) << "Stored ok";
        return walletFileName;
      } else { // no keys, wallet error loading
        throw std::runtime_error("can't load wallet file '" + walletFileName + "', check password");
      }
    } else { //new wallet ok 
      return walletFileName;
    }
  } else if (keysExists) { //wallet not exists but keys presented
    std::stringstream ss;
    CryptoNote::importLegacyKeys(keys_file, password, ss);
    boost::filesystem::rename(keys_file, keys_file + ".back");

    WalletHelper::InitWalletResultObserver initObserver;
    std::future<std::error_code> f_initError = initObserver.initResult.get_future();

    WalletHelper::IWalletRemoveObserverGuard removeGuard(*wallet, initObserver);

    wallet->initAndLoad(ss, password);
    auto initError = f_initError.get();

    removeGuard.removeObserver();

    if (initError) {
      throw std::runtime_error("failed to load wallet: " + initError.message());
    }

    logger(INFO) << "Storing wallet...";

    try {
      CryptoNote::WalletHelper::storeWallet(*wallet, walletFileName);
    } catch(std::exception& e) {
      logger(ERROR, BRIGHT_RED) << "Failed to store wallet: " << e.what();
      throw std::runtime_error("error saving wallet file '" + walletFileName + "'");
    }

    logger(INFO, BRIGHT_GREEN) << "Stored ok";
    return walletFileName;
  } else { //no wallet no keys
    throw std::runtime_error("wallet file '" + walletFileName + "' is not found");
  }
}


}


std::string simple_wallet::get_commands_str()
{
  std::stringstream ss;
  ss << "Commands: " << ENDL;
  std::string usage = m_consoleHandler.getUsage();
  boost::replace_all(usage, "\n", "\n  ");
  usage.insert(0, "  ");
  ss << usage << ENDL;
  return ss.str();
}

bool simple_wallet::help(const std::vector<std::string> &args/* = std::vector<std::string>()*/)
{
  success_msg_writer() << get_commands_str();
  return true;
}

bool simple_wallet::exit(const std::vector<std::string> &args) {
  m_consoleHandler.requestStop();
  return true;
}

simple_wallet::simple_wallet(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency, Logging::LoggerManager& log)
  : m_daemon_port(0)
  , m_currency(currency)
  , logManager(log)
  , m_dispatcher(dispatcher)
  , logger(log, "simplewallet")
  , m_refresh_progress_reporter(*this)
  , m_initResultPromise(nullptr)
{
  m_consoleHandler.setHandler("start_mining", boost::bind(&simple_wallet::start_mining, this, _1), "start_mining [<number_of_threads>] - Start mining in daemon");
  m_consoleHandler.setHandler("stop_mining", boost::bind(&simple_wallet::stop_mining, this, _1), "Stop mining in daemon");
  //m_consoleHandler.setHandler("refresh", boost::bind(&simple_wallet::refresh, this, _1), "Resynchronize transactions and balance");
  m_consoleHandler.setHandler("balance", boost::bind(&simple_wallet::show_balance, this, _1), "Show current wallet balance");
  m_consoleHandler.setHandler("incoming_transfers", boost::bind(&simple_wallet::show_incoming_transfers, this, _1), "Show incoming transfers");
  m_consoleHandler.setHandler("list_transfers", boost::bind(&simple_wallet::listTransfers, this, _1), "Show all known transfers");
  m_consoleHandler.setHandler("payments", boost::bind(&simple_wallet::show_payments, this, _1), "payments <payment_id_1> [<payment_id_2> ... <payment_id_N>] - Show payments <payment_id_1>, ... <payment_id_N>");
  m_consoleHandler.setHandler("bc_height", boost::bind(&simple_wallet::show_blockchain_height, this, _1), "Show blockchain height");
  m_consoleHandler.setHandler("transfer", boost::bind(&simple_wallet::transfer, this, _1),
    "transfer <mixin_count> <addr_1> <amount_1> [<addr_2> <amount_2> ... <addr_N> <amount_N>] [-p payment_id] [-f fee]"
    " - Transfer <amount_1>,... <amount_N> to <address_1>,... <address_N>, respectively. "
    "<mixin_count> is the number of transactions yours is indistinguishable from (from 0 to maximum available)");
  m_consoleHandler.setHandler("set_log", boost::bind(&simple_wallet::set_log, this, _1), "set_log <level> - Change current log detalization level, <level> is a number 0-4");
  m_consoleHandler.setHandler("address", boost::bind(&simple_wallet::print_address, this, _1), "Show current wallet public address");
  m_consoleHandler.setHandler("save", boost::bind(&simple_wallet::save, this, _1), "Save wallet synchronized data");
  m_consoleHandler.setHandler("reset", boost::bind(&simple_wallet::reset, this, _1), "Discard cache data and start synchronizing from the start");
  m_consoleHandler.setHandler("help", boost::bind(&simple_wallet::help, this, _1), "Show this help");
  m_consoleHandler.setHandler("exit", boost::bind(&simple_wallet::exit, this, _1), "Close wallet");
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::set_log(const std::vector<std::string> &args)
{
  if (args.size() != 1)
  {
    fail_msg_writer() << "use: set_log <log_level_number_0-4>";
    return true;
  }
  uint16_t l = 0;
  if (!Common::fromString(args[0], l))
  {
    fail_msg_writer() << "wrong number format, use: set_log <log_level_number_0-4>";
    return true;
  }
 
  if (l > Logging::TRACE)
  {
    fail_msg_writer() << "wrong number range, use: set_log <log_level_number_0-4>";
    return true;
  }

  logManager.setMaxLevel(static_cast<Logging::Level>(l));
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::init(const boost::program_options::variables_map& vm)
{
  handle_command_line(vm);

  if (!m_daemon_address.empty() && (!m_daemon_host.empty() || 0 != m_daemon_port)) {
    fail_msg_writer() << "you can't specify daemon host or port several times";
    return false;
  }

  if (m_generate_new.empty() && m_wallet_file_arg.empty()) {
    std::cout << "Nor 'generate-new-wallet' neither 'wallet-file' argument was specified.\nWhat do you want to do?\n[O]pen existing wallet, [G]enerate new wallet file or [E]xit.\n";
    char c;
    do {
      std::string answer;
      std::getline(std::cin, answer);
      c = answer[0];
      if (!(c == 'O' || c == 'G' || c == 'E' || c == 'o' || c == 'g' || c == 'e')) {
        std::cout << "Unknown command: " << c <<std::endl;
      } else {
        break;
      }
    } while (true);

    if (c == 'E' || c == 'e') {
      return false;
    }

    std::cout << "Specify wallet file name (e.g., wallet.bin).\n";
    std::string userInput;
    do {
      std::cout << "Wallet file name: ";
      std::getline(std::cin, userInput);
      boost::algorithm::trim(userInput);
    } while (userInput.empty());

    if (c == 'g' || c == 'G') {
      m_generate_new = userInput;
    } else {
      m_wallet_file_arg = userInput;
    }
  }

  if (!m_generate_new.empty() && !m_wallet_file_arg.empty()) {
    fail_msg_writer() << "you can't specify 'generate-new-wallet' and 'wallet-file' arguments simultaneously";
    return false;
  }

  std::string walletFileName;
  sync_from_zero = command_line::get_arg(vm, arg_SYNC_FROM_ZERO);
  if (!m_generate_new.empty()) {
    std::string ignoredString;
    WalletHelper::prepareFileNames(m_generate_new, ignoredString, walletFileName);
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletFileName, ignore)) {
      fail_msg_writer() << walletFileName << " already exists";
      return false;
    }
  }

  if (m_daemon_host.empty())
    m_daemon_host = "localhost";
  if (!m_daemon_port)
    m_daemon_port = std::stoi(command_line::get_arg(vm, arg_rpc_bind_port));
  
  if (!m_daemon_address.empty()) {
    if (!parseUrlAddress(m_daemon_address, m_daemon_host, m_daemon_port)) {
      fail_msg_writer() << "failed to parse daemon address: " << m_daemon_address;
      return false;
    }
  } else {
    m_daemon_address = std::string("http://") + m_daemon_host + ":" + std::to_string(m_daemon_port);
  }

  tools::password_container pwd_container;
  if (command_line::has_arg(vm, arg_password)) {
    pwd_container.password(command_line::get_arg(vm, arg_password));
  } else if (!pwd_container.read_password()) {
    fail_msg_writer() << "failed to read wallet password";
    return false;
  }

  this->m_node.reset(new NodeRpcProxy(m_daemon_host, m_daemon_port));

  std::promise<std::error_code> errorPromise;
  std::future<std::error_code> f_error = errorPromise.get_future();
  auto callback = [&errorPromise](std::error_code e) {errorPromise.set_value(e); };
  m_node->init(callback);
  auto error = f_error.get();
  if (error) {
    fail_msg_writer() << "failed to init NodeRPCProxy: " << error.message();
    return false;
  }

  sync_from_zero = command_line::get_arg(vm, arg_SYNC_FROM_ZERO);
  if (!m_generate_new.empty())
  {
    bool r = new_wallet(walletFileName, pwd_container.password());
    if (!(r)) { logger(ERROR, BRIGHT_RED) << "account creation failed"; return false; }
  }
  else
  {
    m_wallet.reset(new Wallet(m_currency, *m_node));
    m_wallet->syncAll(sync_from_zero);

    try {
      m_wallet_file = tryToOpenWalletOrLoadKeysOrThrow(logger, m_wallet, m_wallet_file_arg, pwd_container.password());
    } catch (const std::exception& e) {
      fail_msg_writer() << "failed to load wallet: " << e.what();
      return false;
    }

    m_wallet->addObserver(this);
    m_node->addObserver(this);

    logger(INFO, BRIGHT_WHITE) << "Opened wallet: " << m_wallet->getAddress();

    success_msg_writer() <<
      "**********************************************************************\n" <<
      "Use \"help\" command to see the list of available commands.\n" <<
      "**********************************************************************";
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::deinit()
{
  if (!m_wallet.get())
    return true;

  return close_wallet();
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::handle_command_line(const boost::program_options::variables_map& vm)
{
  m_wallet_file_arg = command_line::get_arg(vm, arg_wallet_file);
  m_generate_new = command_line::get_arg(vm, arg_generate_new_wallet);
  m_daemon_address = command_line::get_arg(vm, arg_daemon_address);
  m_daemon_host = command_line::get_arg(vm, arg_daemon_host);
  m_daemon_port = command_line::get_arg(vm, arg_daemon_port);
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::new_wallet(const std::string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;

  m_wallet.reset(new Wallet(m_currency, *m_node.get()));
  m_node->addObserver(this);
  m_wallet->addObserver(this);
  try
  {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();
    m_wallet->syncAll(sync_from_zero);
    m_wallet->initAndGenerate(password);
    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError) {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }

    try {
      CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    } catch (std::exception& e) {
      fail_msg_writer() << "failed to save new wallet: " << e.what();
      throw;
    }

    WalletAccountKeys keys;
    m_wallet->getAccountKeys(keys);

    logger(INFO, BRIGHT_WHITE) <<
      "Generated new wallet: " << m_wallet->getAddress() << std::endl <<
      "view key: " << Common::podToHex(keys.viewSecretKey);
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "failed to generate new wallet: " << e.what();
    return false;
  }

  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Your wallet has been generated.\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "Always use \"exit\" command when closing simplewallet to save\n" <<
    "current session's state. Otherwise, you will possibly need to synchronize \n" <<
    "your wallet again. Your wallet key is NOT under risk anyway.\n" <<
    "**********************************************************************";
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::close_wallet()
{
  try
  {
    CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
    return false;
  }

  m_wallet->removeObserver(this);
  m_wallet->shutdown();

  return true;
}

//----------------------------------------------------------------------------------------------------
bool simple_wallet::save(const std::vector<std::string> &args)
{
  try
  {
    CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    success_msg_writer() << "Wallet data saved";
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
  }

  return true;
}

bool simple_wallet::reset(const std::vector<std::string> &args) {
  m_wallet->reset();
  success_msg_writer(true) << "Reset is complete successfully";
  return true;
}

bool simple_wallet::start_mining(const std::vector<std::string>& args)
{
  COMMAND_RPC_START_MINING::request req;
  req.miner_address = m_wallet->getAddress();

  bool ok = true;
  size_t max_mining_threads_count = (std::max)(std::thread::hardware_concurrency(), static_cast<unsigned>(2));
  if (0 == args.size())
  {
    req.threads_count = 1;
  }
  else if (1 == args.size())
  {
    uint16_t num = 1;
    ok = Common::fromString(args[0], num);
    ok = ok && (1 <= num && num <= max_mining_threads_count);
    req.threads_count = num;
  }
  else
  {
    ok = false;
  }

  if (!ok)
  {
    fail_msg_writer() << "invalid arguments. Please use start_mining [<number_of_threads>], " <<
      "<number_of_threads> should be from 1 to " << max_mining_threads_count;
    return true;
  }


  COMMAND_RPC_START_MINING::response res;

  try {
    HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);

    invokeJsonCommand(httpClient, "/start_mining", req, res);

    std::string err = interpret_rpc_response(true, res.status);
    if (err.empty())
      success_msg_writer() << "Mining started in daemon";
    else
      fail_msg_writer() << "mining has NOT been started: " << err;

  } catch (const std::exception& e) {
    fail_msg_writer() << "Failed to invoke rpc method: " << e.what();
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::stop_mining(const std::vector<std::string>& args)
{
  COMMAND_RPC_STOP_MINING::request req;
  COMMAND_RPC_STOP_MINING::response res;

  try {
    HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);
    invokeJsonCommand(httpClient, "/stop_mining", req, res);
    std::string err = interpret_rpc_response(true, res.status);
    if (err.empty())
      success_msg_writer() << "Mining stopped in daemon";
    else
      fail_msg_writer() << "mining has NOT been stopped: " << err;
  } catch (const std::exception& e) {
    fail_msg_writer() << "Failed to invoke rpc method: " << e.what();
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::initCompleted(std::error_code result) {
  if (m_initResultPromise.get() != nullptr) {
    m_initResultPromise->set_value(result);
  }
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::localBlockchainUpdated(uint64_t height)
{
  m_refresh_progress_reporter.update(height, false);
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::externalTransactionCreated(CryptoNote::TransactionId transactionId) 
{
  TransactionInfo txInfo;
  m_wallet->getTransaction(transactionId, txInfo);

  if (txInfo.totalAmount >= 0) {
    logger(INFO, GREEN) <<
      "Height " << txInfo.blockHeight << ", transaction " << Common::podToHex(txInfo.hash) <<
      ", received " << m_currency.formatAmount(txInfo.totalAmount);
  } else {
    logger(INFO, MAGENTA) <<
      "Height " << txInfo.blockHeight << ", transaction " << Common::podToHex(txInfo.hash) <<
      ", spent " << m_currency.formatAmount(static_cast<uint64_t>(-txInfo.totalAmount));
  }

  m_refresh_progress_reporter.update(txInfo.blockHeight, true);
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_balance(const std::vector<std::string>& args/* = std::vector<std::string>()*/)
{
  success_msg_writer() << "available balance: " << m_currency.formatAmount(m_wallet->actualBalance()) <<
    ", locked amount: " << m_currency.formatAmount(m_wallet->pendingBalance());

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_incoming_transfers(const std::vector<std::string>& args)
{
  bool hasTransfers = false;
  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
    TransactionInfo txInfo;
    m_wallet->getTransaction(trantransactionNumber, txInfo);
    if (txInfo.totalAmount < 0) continue;
    hasTransfers = true;
    logger(INFO) << "        amount       \t                              tx id";
    logger(INFO, GREEN) <<  // spent - magenta
      std::setw(21) << m_currency.formatAmount(txInfo.totalAmount) << '\t' << Common::podToHex(txInfo.hash);
  }

  if (!hasTransfers) success_msg_writer() << "No incoming transfers";
  return true;
}

bool simple_wallet::listTransfers(const std::vector<std::string>& args) {
  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
    TransactionInfo txInfo;
    m_wallet->getTransaction(trantransactionNumber, txInfo);
    if (txInfo.state != TransactionState::Active || txInfo.blockHeight == UNCONFIRMED_TRANSACTION_HEIGHT) {
      continue;
    }

    std::string paymentIdStr = "";    
    std::vector<uint8_t> extraVec;
    extraVec.reserve(txInfo.extra.size());
    std::for_each(txInfo.extra.begin(), txInfo.extra.end(), [&extraVec](const char el) { extraVec.push_back(el); });

    crypto::hash paymentId;
    paymentIdStr = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != null_hash ? Common::podToHex(paymentId) : "");
    
    std::string address = "-";
    if (txInfo.totalAmount < 0) {
      if (txInfo.transferCount > 0)
      {
        Transfer tr;
        m_wallet->getTransfer(txInfo.firstTransferId, tr);
        address = tr.address;
      }
    }

    logger(INFO, txInfo.totalAmount < 0 ? MAGENTA : GREEN)
      << txInfo.timestamp
      << ", " << (txInfo.totalAmount < 0 ? "OUTPUT" : "INPUT")
      << ", " << Common::podToHex(txInfo.hash)
      << ", " << (txInfo.totalAmount < 0 ? "-" : "") << m_currency.formatAmount(std::abs(txInfo.totalAmount))
      << ", " << m_currency.formatAmount(txInfo.fee)
      << ", " << (paymentIdStr.empty() ? std::string("-") : paymentIdStr)
      << ", " << address
      << ", " << txInfo.blockHeight
      << ", " << txInfo.unlockTime; 
  }

  if (transactionsCount == 0) success_msg_writer() << "No transfers";
  return true;
}

bool simple_wallet::show_payments(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    fail_msg_writer() << "expected at least one payment ID";
    return true;
  }

  logger(INFO) << "                            payment                             \t" <<
    "                          transaction                           \t" <<
    "  height\t       amount        ";

  bool payments_found = false;
  for (const std::string& arg: args)
  {
    crypto::hash expectedPaymentId;
    if (CryptoNote::parsePaymentId(arg, expectedPaymentId))
    {
      size_t transactionsCount = m_wallet->getTransactionCount();
      for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
        TransactionInfo txInfo;
        m_wallet->getTransaction(trantransactionNumber, txInfo);
        if (txInfo.totalAmount < 0) continue;
        std::vector<uint8_t> extraVec;
        extraVec.reserve(txInfo.extra.size());
        std::for_each(txInfo.extra.begin(), txInfo.extra.end(), [&extraVec](const char el) { extraVec.push_back(el); });

        crypto::hash paymentId;
        if (CryptoNote::getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId == expectedPaymentId) {
          payments_found = true;
          success_msg_writer(true) <<
            paymentId << "\t\t" <<
            Common::podToHex(txInfo.hash) <<
            std::setw(8) << txInfo.blockHeight << '\t' <<
            std::setw(21) << m_currency.formatAmount(txInfo.totalAmount);// << '\t' <<
        }
      }

      if (!payments_found) {
        success_msg_writer() << "No payments with id " << expectedPaymentId;
        continue;
      }
    } else {
      fail_msg_writer() << "payment ID has invalid format: \"" << arg << "\", expected 64-character string";
    }
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::show_blockchain_height(const std::vector<std::string>& args)
{
  try {
    uint64_t bc_height = m_node->getLastLocalBlockHeight();
    success_msg_writer() << bc_height;
  } catch (std::exception &e) {
    fail_msg_writer() << "failed to get blockchain height: " << e.what();
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::transfer(const std::vector<std::string> &args)
{
  try
  {
    TransferCommand cmd(m_currency);

    if (!cmd.parseArguments(logger, args))
      return false;
    CryptoNote::WalletHelper::SendCompleteResultObserver sent;

    std::string extraString;
    std::copy(cmd.extra.begin(), cmd.extra.end(), std::back_inserter(extraString));

    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    CryptoNote::TransactionId tx = m_wallet->sendTransaction(cmd.dsts, cmd.fee, extraString, cmd.fake_outs_count, 0);
    if (tx == INVALID_TRANSACTION_ID) {
      fail_msg_writer() << "Can't send money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError) {
      fail_msg_writer() << sendError.message();
      return true;
    }

    CryptoNote::TransactionInfo txInfo;
    m_wallet->getTransaction(tx, txInfo);
    success_msg_writer(true) << "Money successfully sent, transaction " << Common::podToHex(txInfo.hash);

    try {
      CryptoNote::WalletHelper::storeWallet(*m_wallet, m_wallet_file);
    } catch (const std::exception& e) {
      fail_msg_writer() << e.what();
      return true;
    }
  }
  catch (const std::system_error& e) 
  {
    fail_msg_writer() << "unexpected error: " << e.what();
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "unexpected error: " << e.what();
  }
  catch (...)
  {
    fail_msg_writer() << "unknown error";
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::run() {
  std::string addr_start = m_wallet->getAddress().substr(0, 6);
  m_consoleHandler.start(false, "[wallet " + addr_start + "]: ", Common::Console::Color::BrightYellow);
  return true;
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::stop() {
  m_consoleHandler.requestStop();
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::print_address(const std::vector<std::string> &args/* = std::vector<std::string>()*/) {
  success_msg_writer() << m_wallet->getAddress();
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::process_command(const std::vector<std::string> &args) {
  return m_consoleHandler.runCommand(args);
}


int main(int argc, char* argv[])
{
#ifdef WIN32
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  po::options_description desc_general("General options");
  command_line::add_arg(desc_general, command_line::arg_help);
  command_line::add_arg(desc_general, command_line::arg_version);

  po::options_description desc_params("Wallet options");
  command_line::add_arg(desc_params, arg_wallet_file);
  command_line::add_arg(desc_params, arg_generate_new_wallet);
  command_line::add_arg(desc_params, arg_password);
  command_line::add_arg(desc_params, arg_daemon_address);
  command_line::add_arg(desc_params, arg_daemon_host);
  command_line::add_arg(desc_params, arg_daemon_port);
  command_line::add_arg(desc_params, arg_command);
  command_line::add_arg(desc_params, arg_log_level);
  command_line::add_arg(desc_params, arg_testnet);
  tools::wallet_rpc_server::init_options(desc_params);
  command_line::add_arg(desc_params, command_line::arg_data_dir, tools::get_default_data_dir());
  command_line::add_arg(desc_params, arg_config_file);
  command_line::add_arg(desc_params, arg_GENESIS_COINBASE_TX_HEX);
  command_line::add_arg(desc_params, arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
  command_line::add_arg(desc_params, arg_MONEY_SUPPLY);
  command_line::add_arg(desc_params, arg_EMISSION_SPEED_FACTOR);
  command_line::add_arg(desc_params, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
  command_line::add_arg(desc_params, arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  command_line::add_arg(desc_params, arg_MINIMUM_FEE);
  command_line::add_arg(desc_params, arg_DEFAULT_DUST_THRESHOLD);
  command_line::add_arg(desc_params, arg_DIFFICULTY_TARGET);
  command_line::add_arg(desc_params, arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
  command_line::add_arg(desc_params, arg_MAX_BLOCK_SIZE_INITIAL);
  command_line::add_arg(desc_params, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);
  command_line::add_arg(desc_params, arg_UPGRADE_HEIGHT);
  command_line::add_arg(desc_params, arg_DIFFICULTY_CUT);
  command_line::add_arg(desc_params, arg_DIFFICULTY_LAG);
  command_line::add_arg(desc_params, arg_rpc_bind_port);
  command_line::add_arg(desc_params, arg_SYNC_FROM_ZERO);

  po::positional_options_description positional_options;
  positional_options.add(arg_command.name, -1);

  po::options_description desc_all;
  desc_all.add(desc_general).add(desc_params);

  Logging::LoggerManager logManager;
  Logging::LoggerRef logger(logManager, "simplewallet");
  System::Dispatcher dispatcher;

  po::variables_map vm;

  bool r = command_line::handle_error_helper(desc_all, [&]() {
    po::store(command_line::parse_command_line(argc, argv, desc_general, true), vm);

    if (command_line::get_arg(vm, command_line::arg_help)) {
      CryptoNote::Currency tmp_currency = CryptoNote::CurrencyBuilder(logManager).currency();
      CryptoNote::simple_wallet tmp_wallet(dispatcher, tmp_currency, logManager);

      std::cout << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG << std::endl;
      std::cout << "Usage: simplewallet [--wallet-file=<file>|--generate-new-wallet=<file>] [--daemon-address=<host>:<port>] [<COMMAND>]";
      std::cout << desc_all << '\n' << tmp_wallet.get_commands_str();
      return false;
    } else if (command_line::get_arg(vm, command_line::arg_version))  {
      std::cout << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG;
      return false;
    }

    auto parser = po::command_line_parser(argc, argv).options(desc_params).positional(positional_options);
    po::store(parser.run(), vm);
    std::string data_dir = command_line::get_arg(vm, command_line::arg_data_dir);
    std::string config = command_line::get_arg(vm, arg_config_file);
      std::cout << config << std::endl;
    boost::filesystem::path data_dir_path(data_dir);
    boost::filesystem::path config_path(config);
      std::cout << "boost filesystem passed" << std::endl;
    if (!config_path.has_parent_path())
    {
      std::cout << "file not found" << std::endl;
      config_path = data_dir_path / config_path;
    }
      std::cout << "boost filesystem passed" << std::endl;
    boost::system::error_code ec;
    if (boost::filesystem::exists(config_path, ec))
    {
      std::cout << "config path exist" << std::endl;
      po::store(po::parse_config_file<char>(config_path.string<std::string>().c_str(), desc_params, true), vm);
    }
    else
    {
      std::cout << "config path does not exist" << std::endl;
    }
    po::notify(vm);
    return true;
  });

  if (!r)
    return 1;

  //set up logging options
  Level logLevel = DEBUGGING;

  if (command_line::has_arg(vm, arg_log_level)) {
    logLevel = static_cast<Level>(command_line::get_arg(vm, arg_log_level));
  }

  logManager.configure(buildLoggerConfiguration(logLevel, Common::ReplaceExtenstion(argv[0], ".log")));

  logger(INFO, BRIGHT_WHITE) << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG;

  CryptoNote::CurrencyBuilder currencyBuilder(logManager);
  currencyBuilder.genesisCoinbaseTxHex(command_line::get_arg(vm, arg_GENESIS_COINBASE_TX_HEX));
  currencyBuilder.publicAddressBase58Prefix(command_line::get_arg(vm, arg_CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX));
  currencyBuilder.moneySupply(command_line::get_arg(vm, arg_MONEY_SUPPLY));
  currencyBuilder.emissionSpeedFactor(command_line::get_arg(vm, arg_EMISSION_SPEED_FACTOR));
  currencyBuilder.blockGrantedFullRewardZone(command_line::get_arg(vm, arg_CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE));
  currencyBuilder.numberOfDecimalPlaces(command_line::get_arg(vm, arg_CRYPTONOTE_DISPLAY_DECIMAL_POINT));
  currencyBuilder.mininumFee(command_line::get_arg(vm, arg_MINIMUM_FEE));
  currencyBuilder.defaultDustThreshold(command_line::get_arg(vm, arg_DEFAULT_DUST_THRESHOLD));
  currencyBuilder.difficultyTarget(command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
  currencyBuilder.minedMoneyUnlockWindow(command_line::get_arg(vm, arg_CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW));
  currencyBuilder.maxBlockSizeInitial(command_line::get_arg(vm, arg_MAX_BLOCK_SIZE_INITIAL));
  if (command_line::has_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) && command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY) != 0)
  {
    currencyBuilder.difficultyWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.upgradeVotingWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
    currencyBuilder.upgradeWindow(command_line::get_arg(vm, arg_EXPECTED_NUMBER_OF_BLOCKS_PER_DAY));
  } else {
    currencyBuilder.difficultyWindow(24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
  }
  currencyBuilder.maxBlockSizeGrowthSpeedDenominator(365 * 24 * 60 * 60 / command_line::get_arg(vm, arg_DIFFICULTY_TARGET));
  currencyBuilder.lockedTxAllowedDeltaSeconds(command_line::get_arg(vm, arg_DIFFICULTY_TARGET) * CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS);  
  if (command_line::has_arg(vm, arg_UPGRADE_HEIGHT) && command_line::get_arg(vm, arg_UPGRADE_HEIGHT) != 0)
  {
    currencyBuilder.upgradeHeight(command_line::get_arg(vm, arg_UPGRADE_HEIGHT));
  }
  currencyBuilder.difficultyLag(command_line::get_arg(vm, arg_DIFFICULTY_CUT));
  currencyBuilder.difficultyCut(command_line::get_arg(vm, arg_DIFFICULTY_LAG));
currencyBuilder.testnet(command_line::get_arg(vm, arg_testnet));
CryptoNote::Currency currency = currencyBuilder.currency();

  if (command_line::has_arg(vm, tools::wallet_rpc_server::arg_rpc_bind_port)) {
    //runs wallet with rpc interface
    if (!command_line::has_arg(vm, arg_wallet_file))
    {
      logger(ERROR, BRIGHT_RED) << "Wallet file not set.";
      return 1;
    }
    if (!command_line::has_arg(vm, arg_daemon_address))
    {
      logger(ERROR, BRIGHT_RED) << "Daemon address not set.";
      return 1;
    }
    if (!command_line::has_arg(vm, arg_password))
    {
      logger(ERROR, BRIGHT_RED) << "Wallet password not set.";
      return 1;
    }

    std::string wallet_file = command_line::get_arg(vm, arg_wallet_file);
    std::string wallet_password = command_line::get_arg(vm, arg_password);
    std::string daemon_address = command_line::get_arg(vm, arg_daemon_address);
    std::string daemon_host = command_line::get_arg(vm, arg_daemon_host);
    int daemon_port = command_line::get_arg(vm, arg_daemon_port);
    if (daemon_host.empty())
      daemon_host = "localhost";
    if (!daemon_port)
      daemon_port = std::stoi(command_line::get_arg(vm, arg_rpc_bind_port));

    if (!daemon_address.empty()) {
      uint16_t port = 0;
      if (!parseUrlAddress(daemon_address, daemon_host, port)) {
        logger(ERROR, BRIGHT_RED) << "failed to parse daemon address: " << daemon_address;
        return 1;
      }

      daemon_port = port;
    }

    std::unique_ptr<INode> node(new NodeRpcProxy(daemon_host, daemon_port));

    std::promise<std::error_code> errorPromise;
    std::future<std::error_code> error = errorPromise.get_future();
    auto callback = [&errorPromise](std::error_code e) {errorPromise.set_value(e); };
    node->init(callback);
    if (error.get()) {
      logger(ERROR, BRIGHT_RED) << ("failed to init NodeRPCProxy");
      return 1;
    }

    std::unique_ptr<IWallet> wallet(new Wallet(currency, *node.get()));

    std::string walletFileName;
    try  {
      walletFileName = ::tryToOpenWalletOrLoadKeysOrThrow(logger, wallet, wallet_file, wallet_password);

      logger(INFO) << "available balance: " << currency.formatAmount(wallet->actualBalance()) <<
      ", locked amount: " << currency.formatAmount(wallet->pendingBalance());

      logger(INFO, BRIGHT_GREEN) << "Loaded ok";
    } catch (const std::exception& e)  {
      logger(ERROR, BRIGHT_RED) << "Wallet initialize failed: " << e.what();
      return 1;
    }

    tools::wallet_rpc_server wrpc(dispatcher, logManager, *wallet, *node, currency, walletFileName);

    if (!wrpc.init(vm)) {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize wallet rpc server";
      return 1;
    }

    tools::SignalHandler::install([&wrpc, &wallet] {
      wrpc.send_stop_signal();
    });

    logger(INFO) << "Starting wallet rpc server";
    wrpc.run();
    logger(INFO) << "Stopped wallet rpc server";
    
    try {
      CryptoNote::WalletHelper::storeWallet(*wallet, walletFileName);

      logger(INFO, BRIGHT_GREEN) << "Stored ok";
    }
    catch (const std::exception& e)
    {
      logger(ERROR, BRIGHT_RED) << "Failed to store wallet: " << e.what();
      return 1;
    }
  } else {
    //runs wallet with console interface
    CryptoNote::simple_wallet wal(dispatcher, currency, logManager);
    
    if (!wal.init(vm)) {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize wallet"; 
      return 1; 
    }

    std::vector<std::string> command = command_line::get_arg(vm, arg_command);
    if (!command.empty())
      wal.process_command(command);

    tools::SignalHandler::install([&wal] {
      wal.stop();
    });
    
    wal.run();

    if (!wal.deinit()) {
      logger(ERROR, BRIGHT_RED) << "Failed to close wallet";
    } else {
      logger(INFO) << "Wallet closed";
    }
  }
  return 1;
  //CATCH_ENTRY_L0("main", 1);
}
