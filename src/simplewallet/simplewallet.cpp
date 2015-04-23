// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <thread>
#include <set>
#include <future>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

// epee
#include "include_base_utils.h"
#include "storages/http_abstract_invoke.h"

#include "common/command_line.h"
#include "common/SignalHandler.h"
#include "common/util.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "p2p/net_node.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "simplewallet.h"
#include "wallet/wallet_rpc_server.h"
#include "version.h"
#include "wallet/WalletHelper.h"
#include "wallet/Wallet.h"
#include "wallet/wallet_errors.h"
#include "node_rpc_proxy/NodeRpcProxy.h"
#include "wallet/LegacyKeysImporter.h"

#if defined(WIN32)
#include <crtdbg.h>
#endif

using namespace std;
using namespace epee;
using namespace cryptonote;
using namespace CryptoNote;
using boost::lexical_cast;
namespace po = boost::program_options;

#define EXTENDED_LOGS_FILE "wallet_details.log"


namespace {

const command_line::arg_descriptor<std::string> arg_wallet_file = { "wallet-file", "Use wallet <arg>", "" };
const command_line::arg_descriptor<std::string> arg_generate_new_wallet = { "generate-new-wallet", "Generate new wallet and save it to <arg>", "" };
const command_line::arg_descriptor<std::string> arg_daemon_address = { "daemon-address", "Use daemon instance at <host>:<port>", "" };
const command_line::arg_descriptor<std::string> arg_daemon_host = { "daemon-host", "Use daemon instance at host <arg> instead of localhost", "" };
const command_line::arg_descriptor<std::string> arg_password = { "password", "Wallet password", "", true };
const command_line::arg_descriptor<int> arg_daemon_port = { "daemon-port", "Use daemon instance at port <arg> instead of 8081", 0 };
const command_line::arg_descriptor<uint32_t> arg_log_level = { "set_log", "", 0, true };
const command_line::arg_descriptor<bool> arg_testnet = { "testnet", "Used to deploy test nets. The daemon must be launched with --testnet flag", false };

const command_line::arg_descriptor< std::vector<std::string> > arg_command = { "command", "" };

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

class message_writer {
public:
  message_writer(epee::log_space::console_colors color = epee::log_space::console_color_default, bool bright = false,
    std::string&& prefix = std::string(), int log_level = LOG_LEVEL_2)
    : m_flush(true)
    , m_color(color)
    , m_bright(bright)
    , m_log_level(log_level) {
    m_oss << prefix;
  }

  message_writer(message_writer&& rhs)
    : m_flush(std::move(rhs.m_flush))
#if defined(_MSC_VER)
    , m_oss(std::move(rhs.m_oss))
#else
    // GCC bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54316
    , m_oss(rhs.m_oss.str(), ios_base::out | ios_base::ate)
#endif
    , m_color(std::move(rhs.m_color))
    , m_log_level(std::move(rhs.m_log_level)) {
    rhs.m_flush = false;
  }

  template<typename T>
  std::ostream& operator<<(const T& val) {
    m_oss << val;
    return m_oss;
  }

  ~message_writer() {
    if (m_flush) {
      m_flush = false;

      LOG_PRINT(m_oss.str(), m_log_level)

        if (epee::log_space::console_color_default == m_color) {
        std::cout << m_oss.str();
        } else {
        epee::log_space::set_console_color(m_color, m_bright);
        std::cout << m_oss.str();
        epee::log_space::reset_console_color();
        }
      std::cout << std::endl;
    }
  }

private:
  message_writer(message_writer& rhs);
  message_writer& operator=(message_writer& rhs);
  message_writer& operator=(message_writer&& rhs);

private:
  bool m_flush;
  std::stringstream m_oss;
  epee::log_space::console_colors m_color;
  bool m_bright;
  int m_log_level;
};

message_writer success_msg_writer(bool color = false) {
  return message_writer(color ? epee::log_space::console_color_green : epee::log_space::console_color_default, false, std::string(), LOG_LEVEL_2);
}

message_writer fail_msg_writer() {
  return message_writer(epee::log_space::console_color_red, true, std::string("Error: "), LOG_LEVEL_0);
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
  const cryptonote::Currency& m_currency;
  size_t fake_outs_count;
  vector<CryptoNote::Transfer> dsts;
  std::vector<uint8_t> extra;
  uint64_t fee;

  TransferCommand(const cryptonote::Currency& currency) :
    m_currency(currency), fake_outs_count(0), fee(currency.minimumFee()) {
  }

  bool parseArguments(const std::vector<std::string> &args) {

    ArgumentReader<std::vector<std::string>::const_iterator> ar(args.begin(), args.end());

    try {

      auto mixin_str = ar.next();

      if (!epee::string_tools::get_xtype_from_string(fake_outs_count, mixin_str)) {
        fail_msg_writer() << "mixin_count should be non-negative integer, got " << mixin_str;
        return false;
      }

      while (!ar.eof()) {

        auto arg = ar.next();

        if (arg.size() && arg[0] == '-') {

          const auto& value = ar.next();

          if (arg == "-p") {
            if (!createTxExtraWithPaymentId(value, extra)) {
              fail_msg_writer() << "payment ID has invalid format: \"" << value << "\", expected 64-character string";
              return false;
            }
          } else if (arg == "-f") {
            bool ok = m_currency.parseAmount(value, fee);
            if (!ok) {
              fail_msg_writer() << "Fee value is invalid: " << value;
              return false;
            }

            if (fee < m_currency.minimumFee()) {
              fail_msg_writer() << "Fee value is less than minimum: " << m_currency.minimumFee();
              return false;
            }
          }
        } else {
          Transfer destination;
          cryptonote::tx_destination_entry de;

          if (!m_currency.parseAccountAddressString(arg, de.addr)) {
            crypto::hash paymentId;
            if (cryptonote::parsePaymentId(arg, paymentId)) {
              fail_msg_writer() << "Invalid payment ID usage. Please, use -p <payment_id>. See help for details.";
            } else {
              fail_msg_writer() << "Wrong address: " << arg;
            }

            return false;
          }

          auto value = ar.next();
          bool ok = m_currency.parseAmount(value, de.amount);
          if (!ok || 0 == de.amount) {
            fail_msg_writer() << "amount is wrong: " << arg << ' ' << value <<
              ", expected number from 0 to " << m_currency.formatAmount(std::numeric_limits<uint64_t>::max());
            return false;
          }
          destination.address = arg;
          destination.amount = de.amount;

          dsts.push_back(destination);
        }
      }

      if (dsts.empty()) {
        fail_msg_writer() << "At least one destination address is required";
        return false;
      }
    } catch (const std::exception& e) {
      fail_msg_writer() << e.what();
      return false;
    }

    return true;
  }
};

std::error_code initAndLoadWallet(IWallet& wallet, std::istream& walletFile, const std::string& password) {
  WalletHelper::InitWalletResultObserver initObserver;
  std::future<std::error_code> f_initError = initObserver.initResult.get_future();

  wallet.addObserver(&initObserver);
  wallet.initAndLoad(walletFile, password);
  auto initError = f_initError.get();
  wallet.removeObserver(&initObserver);

  return initError;
}

std::string tryToOpenWalletOrLoadKeysOrThrow(std::unique_ptr<IWallet>& wallet, const std::string& walletFile, const std::string& password) {
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
    LOG_PRINT_L0("Loading wallet...");
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
        cryptonote::importLegacyKeys(keys_file, password, ss);
        boost::filesystem::rename(keys_file, keys_file + ".back");
        boost::filesystem::rename(walletFileName, walletFileName + ".back");

        initError = initAndLoadWallet(*wallet, ss, password);
        if (initError) {
          throw std::runtime_error("failed to load wallet: " + initError.message());
        }

        LOG_PRINT_L0("Storing wallet...");
        std::ofstream walletFile;
        walletFile.open(walletFileName, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
        if (walletFile.fail()) {
          throw std::runtime_error("error saving wallet file '" + walletFileName + "'");
        }
        WalletHelper::SaveWalletResultObserver saveObserver;
        std::future<std::error_code> f_saveError = saveObserver.saveResult.get_future();
        wallet->addObserver(&saveObserver);
        wallet->save(walletFile, false, false);
        auto saveError = f_saveError.get();
        wallet->removeObserver(&saveObserver);
        if (saveError) {
          fail_msg_writer() << "Failed to store wallet: " << saveError.message();
          throw std::runtime_error("error saving wallet file '" + walletFileName + "'");
        }

        LOG_PRINT_GREEN("Stored ok", LOG_LEVEL_0);
        return walletFileName;
      } else { // no keys, wallet error loading
        throw std::runtime_error("can't load wallet file '" + walletFileName + "', check password");
      }
    } else { //new wallet ok 
      return walletFileName;
    }
  } else if (keysExists) { //wallet not exists but keys presented
    std::stringstream ss;
    cryptonote::importLegacyKeys(keys_file, password, ss);
    boost::filesystem::rename(keys_file, keys_file + ".back");

    WalletHelper::InitWalletResultObserver initObserver;
    std::future<std::error_code> f_initError = initObserver.initResult.get_future();
    wallet->addObserver(&initObserver);
    wallet->initAndLoad(ss, password);
    auto initError = f_initError.get();
    wallet->removeObserver(&initObserver);
    if (initError) {
      throw std::runtime_error("failed to load wallet: " + initError.message());
    }

    LOG_PRINT_L0("Storing wallet...");
    std::ofstream walletFile;
    walletFile.open(walletFileName, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
    if (walletFile.fail()) {
      throw std::runtime_error("error saving wallet file '" + walletFileName  + "'");
    }
    WalletHelper::SaveWalletResultObserver saveObserver;
    std::future<std::error_code> f_saveError = saveObserver.saveResult.get_future();
    wallet->addObserver(&saveObserver);
    wallet->save(walletFile, false, false);
    auto saveError = f_saveError.get();
    wallet->removeObserver(&saveObserver);
    if (saveError) {
      fail_msg_writer() << "Failed to store wallet: " << saveError.message();
      throw std::runtime_error("error saving wallet file '" + walletFileName + "'");
    }

    LOG_PRINT_GREEN("Stored ok", LOG_LEVEL_0);
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
  std::string usage = m_cmd_binder.get_usage();
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

simple_wallet::simple_wallet(const cryptonote::Currency& currency)
  : m_daemon_port(0)
  , m_currency(currency)
  , m_refresh_progress_reporter(*this)
  , m_saveResultPromise(nullptr)
  , m_initResultPromise(nullptr)
{
  m_cmd_binder.set_handler("start_mining", boost::bind(&simple_wallet::start_mining, this, _1), "start_mining [<number_of_threads>] - Start mining in daemon");
  m_cmd_binder.set_handler("stop_mining", boost::bind(&simple_wallet::stop_mining, this, _1), "Stop mining in daemon");
  //m_cmd_binder.set_handler("refresh", boost::bind(&simple_wallet::refresh, this, _1), "Resynchronize transactions and balance");
  m_cmd_binder.set_handler("balance", boost::bind(&simple_wallet::show_balance, this, _1), "Show current wallet balance");
  m_cmd_binder.set_handler("incoming_transfers", boost::bind(&simple_wallet::show_incoming_transfers, this, _1), "Show incoming transfers");
  m_cmd_binder.set_handler("list_transfers", boost::bind(&simple_wallet::listTransfers, this, _1), "Show all known transfers");
  m_cmd_binder.set_handler("payments", boost::bind(&simple_wallet::show_payments, this, _1), "payments <payment_id_1> [<payment_id_2> ... <payment_id_N>] - Show payments <payment_id_1>, ... <payment_id_N>");
  m_cmd_binder.set_handler("bc_height", boost::bind(&simple_wallet::show_blockchain_height, this, _1), "Show blockchain height");
  m_cmd_binder.set_handler("transfer", boost::bind(&simple_wallet::transfer, this, _1),
    "transfer <mixin_count> <addr_1> <amount_1> [<addr_2> <amount_2> ... <addr_N> <amount_N>] [-p payment_id] [-f fee]"
    " - Transfer <amount_1>,... <amount_N> to <address_1>,... <address_N>, respectively. "
    "<mixin_count> is the number of transactions yours is indistinguishable from (from 0 to maximum available)");
  m_cmd_binder.set_handler("set_log", boost::bind(&simple_wallet::set_log, this, _1), "set_log <level> - Change current log detalization level, <level> is a number 0-4");
  m_cmd_binder.set_handler("address", boost::bind(&simple_wallet::print_address, this, _1), "Show current wallet public address");
  m_cmd_binder.set_handler("save", boost::bind(&simple_wallet::save, this, _1), "Save wallet synchronized data");
  m_cmd_binder.set_handler("reset", boost::bind(&simple_wallet::reset, this, _1), "Discard cache data and start synchronizing from the start");
  m_cmd_binder.set_handler("help", boost::bind(&simple_wallet::help, this, _1), "Show this help");
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
  if (!epee::string_tools::get_xtype_from_string(l, args[0]))
  {
    fail_msg_writer() << "wrong number format, use: set_log <log_level_number_0-4>";
    return true;
  }
  if (LOG_LEVEL_4 < l)
  {
    fail_msg_writer() << "wrong number range, use: set_log <log_level_number_0-4>";
    return true;
  }

  log_space::log_singletone::get_set_log_detalisation_level(true, l);
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
      userInput = string_tools::trim(userInput);
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
    m_daemon_port = RPC_DEFAULT_PORT;
  if (m_daemon_address.empty())
    m_daemon_address = std::string("http://") + m_daemon_host + ":" + std::to_string(m_daemon_port);

  tools::password_container pwd_container;
  if (command_line::has_arg(vm, arg_password))
  {
    pwd_container.password(command_line::get_arg(vm, arg_password));
  }
  else
  {
    bool r = pwd_container.read_password();
    if (!r)
    {
      fail_msg_writer() << "failed to read wallet password";
      return false;
    }
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

  if (!m_generate_new.empty())
  {
    bool r = new_wallet(walletFileName, pwd_container.password());
    CHECK_AND_ASSERT_MES(r, false, "account creation failed");
  }
  else
  {
    m_wallet.reset(new Wallet(m_currency, *m_node));

    try {
      m_wallet_file = tryToOpenWalletOrLoadKeysOrThrow(m_wallet, m_wallet_file_arg, pwd_container.password());
    } catch (const std::exception& e) {
      fail_msg_writer() << "failed to load wallet: " << e.what();
      return false;
    }

    m_wallet->addObserver(this);
    m_node->addObserver(this);

    message_writer(epee::log_space::console_color_white, true) << "Opened wallet: " << m_wallet->getAddress();

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

  bool r = close_wallet();
  m_wallet->shutdown();
  return r;
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
bool simple_wallet::new_wallet(const string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;

  m_wallet.reset(new Wallet(m_currency, *m_node.get()));
  m_node->addObserver(this);
  m_wallet->addObserver(this);
  try
  {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();
    m_wallet->initAndGenerate(password);
    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError) {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }
    std::ofstream walletFile;
    walletFile.open(m_wallet_file, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
    if (walletFile.fail())
      return false;
    m_saveResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_saveError = m_saveResultPromise->get_future();
    m_wallet->save(walletFile);
    auto saveError = f_saveError.get();
    m_saveResultPromise.reset(nullptr);
    if (saveError) {
      fail_msg_writer() << "failed to save new wallet: " << saveError.message();
      return false;
    }

    WalletAccountKeys keys;
    m_wallet->getAccountKeys(keys);

    message_writer(epee::log_space::console_color_white, true) <<
      "Generated new wallet: " << m_wallet->getAddress() << std::endl <<
      "view key: " << epee::string_tools::pod_to_hex(keys.viewSecretKey);
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
    std::ofstream walletFile;
    walletFile.open(m_wallet_file, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
    if (walletFile.fail())
      return false;
    m_saveResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_saveError = m_saveResultPromise->get_future();
    m_wallet->save(walletFile);
    auto saveError = f_saveError.get();
    m_saveResultPromise.reset(nullptr);
     if (saveError) {
      fail_msg_writer() << saveError.message();
      return false;
    }
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
    return false;
  }
  m_wallet->removeObserver(this);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::save(const std::vector<std::string> &args)
{
  try
  {
    std::ofstream walletFile;
    walletFile.open(m_wallet_file, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
    if (walletFile.fail())
      return false;
    m_saveResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_saveError = m_saveResultPromise->get_future();
    m_wallet->save(walletFile);
    auto saveError = f_saveError.get();
    m_saveResultPromise.reset(nullptr);
    if (saveError) {
      fail_msg_writer() << saveError.message();
      return false;
    }
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
    ok = string_tools::get_xtype_from_string(num, args[0]);
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
  bool r = net_utils::invoke_http_json_remote_command2(m_daemon_address + "/start_mining", req, res, m_http_client);
  std::string err = interpret_rpc_response(r, res.status);
  if (err.empty())
    success_msg_writer() << "Mining started in daemon";
  else
    fail_msg_writer() << "mining has NOT been started: " << err;
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::stop_mining(const std::vector<std::string>& args)
{
  /* if (!try_connect_to_daemon())
     return true;*/

  COMMAND_RPC_STOP_MINING::request req;
  COMMAND_RPC_STOP_MINING::response res;
  bool r = net_utils::invoke_http_json_remote_command2(m_daemon_address + "/stop_mining", req, res, m_http_client);
  std::string err = interpret_rpc_response(r, res.status);
  if (err.empty())
    success_msg_writer() << "Mining stopped in daemon";
  else
    fail_msg_writer() << "mining has NOT been stopped: " << err;
  return true;
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::initCompleted(std::error_code result) {
  if (m_initResultPromise.get() != nullptr) {
    m_initResultPromise->set_value(result);
  }
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::saveCompleted(std::error_code result) {
  if (m_saveResultPromise.get() != nullptr) {
    m_saveResultPromise->set_value(result);
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
  message_writer(epee::log_space::console_color_green, false) <<
    "Height " << txInfo.blockHeight <<
    ", transaction " << epee::string_tools::pod_to_hex(txInfo.hash) <<
    ", received " << m_currency.formatAmount(static_cast<uint64_t>(txInfo.totalAmount));
  } else {
    message_writer(epee::log_space::console_color_magenta, false) <<
      "Height " << txInfo.blockHeight <<
      ", transaction " << epee::string_tools::pod_to_hex(txInfo.hash) <<
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
    message_writer() << "        amount       \t                              tx id";
    message_writer( epee::log_space::console_color_green, false) <<                     // spent magenta
      std::setw(21) << m_currency.formatAmount(txInfo.totalAmount) << '\t' << epee::string_tools::pod_to_hex(txInfo.hash);
  }

  if (!hasTransfers) success_msg_writer() << "No incoming transfers";
  return true;
}

bool simple_wallet::listTransfers(const std::vector<std::string>& args) {
  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
    TransactionInfo txInfo;
    m_wallet->getTransaction(trantransactionNumber, txInfo);
    if (txInfo.state != TransactionState::Active) {
      continue;
    }

    std::string paymentIdStr = "";    
    std::vector<uint8_t> extraVec;
    extraVec.reserve(txInfo.extra.size());
    std::for_each(txInfo.extra.begin(), txInfo.extra.end(), [&extraVec](const char el) { extraVec.push_back(el); });

    crypto::hash paymentId;
    paymentIdStr = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != null_hash ? epee::string_tools::pod_to_hex(paymentId) : "");
    
    std::string address = "";
    if (txInfo.totalAmount < 0) {
      if (txInfo.transferCount > 0)
      {
        Transfer tr;
        m_wallet->getTransfer(txInfo.firstTransferId, tr);
        address = tr.address;
      }
    }

    message_writer(txInfo.totalAmount < 0 ? epee::log_space::console_color_magenta : epee::log_space::console_color_green, false)
      << txInfo.timestamp
      << ", " << (txInfo.totalAmount < 0 ? "OUTPUT" : "INPUT")
      << ", " << epee::string_tools::pod_to_hex(txInfo.hash)
      << ", " << (txInfo.totalAmount < 0 ? "-" : "") << m_currency.formatAmount(abs(txInfo.totalAmount))
      << ", " << m_currency.formatAmount(txInfo.fee)
      << ", " << paymentIdStr
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

  message_writer() << "                            payment                             \t" <<
    "                          transaction                           \t" <<
    "  height\t       amount        ";

  bool payments_found = false;
  for (const std::string& arg: args)
  {
    crypto::hash expectedPaymentId;
    if (cryptonote::parsePaymentId(arg, expectedPaymentId))
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
        if (cryptonote::getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId == expectedPaymentId) {
          payments_found = true;
          success_msg_writer(true) <<
            paymentId << "\t\t" <<
            epee::string_tools::pod_to_hex(txInfo.hash) <<
            std::setw(8) << txInfo.blockHeight << '\t' <<
            std::setw(21) << m_currency.formatAmount(txInfo.totalAmount);// << '\t' <<
        }
      }

      if (!payments_found)
      {
        success_msg_writer() << "No payments with id " << expectedPaymentId;
        continue;
      }
    }
    else
    {
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

    if (!cmd.parseArguments(args))
      return false;
    cryptonote::WalletHelper::SendCompleteResultObserver sent;
    std::promise<TransactionId> txId;
    sent.expectedTxID = txId.get_future();
    std::future<std::error_code> f_sendError = sent.sendResult.get_future();
    std::string extraString;
    std::copy(cmd.extra.begin(), cmd.extra.end(), std::back_inserter(extraString));

    m_wallet->addObserver(&sent);
    CryptoNote::TransactionId tx = m_wallet->sendTransaction(cmd.dsts, cmd.fee, extraString, cmd.fake_outs_count, 0);
    if (tx == INVALID_TRANSACTION_ID) {
      fail_msg_writer() << "Can't send money";
      return true;
    }
    txId.set_value(tx);
    std::error_code sendError = f_sendError.get();
    m_wallet->removeObserver(&sent);
    if (sendError) {
      fail_msg_writer() << sendError.message();
      return true;
    }

    CryptoNote::TransactionInfo txInfo;
    m_wallet->getTransaction(tx, txInfo);
    success_msg_writer(true) << "Money successfully sent, transaction " << epee::string_tools::pod_to_hex(txInfo.hash);

    try {
      std::ofstream walletFile;
      walletFile.open(m_wallet_file, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
      if (walletFile.fail()) {
        fail_msg_writer() << "cant open " << m_wallet_file << " for save";
        return true;
      }
      m_saveResultPromise.reset(new std::promise<std::error_code>());
      std::future<std::error_code> f_saveError = m_saveResultPromise->get_future();
      m_wallet->save(walletFile);
      auto saveError = f_saveError.get();
      m_saveResultPromise.reset(nullptr);
      if (saveError) {
        fail_msg_writer() << saveError.message();
        return true;
      }
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
bool simple_wallet::run()
{
  std::string addr_start = m_wallet->getAddress().substr(0, 6);
  return m_cmd_binder.run_handling("[wallet " + addr_start + "]: ", "");
}
//----------------------------------------------------------------------------------------------------
void simple_wallet::stop()
{
  m_cmd_binder.stop_handling();
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::print_address(const std::vector<std::string> &args/* = std::vector<std::string>()*/)
{
  success_msg_writer() << m_wallet->getAddress();
  return true;
}
//----------------------------------------------------------------------------------------------------
bool simple_wallet::process_command(const std::vector<std::string> &args)
{
  return m_cmd_binder.process_command_vec(args);
}
//----------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
#ifdef WIN32
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  string_tools::set_module_name_and_folder(argv[0]);

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

  po::positional_options_description positional_options;
  positional_options.add(arg_command.name, -1);

  po::options_description desc_all;
  desc_all.add(desc_general).add(desc_params);
  cryptonote::Currency tmp_currency = cryptonote::CurrencyBuilder().currency();
  cryptonote::simple_wallet tmp_wallet(tmp_currency);
  po::variables_map vm;
  bool r = command_line::handle_error_helper(desc_all, [&]()
  {
    po::store(command_line::parse_command_line(argc, argv, desc_general, true), vm);

    if (command_line::get_arg(vm, command_line::arg_help))
    {
      success_msg_writer() << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG;
      success_msg_writer() << "Usage: simplewallet [--wallet-file=<file>|--generate-new-wallet=<file>] [--daemon-address=<host>:<port>] [<COMMAND>]";
      success_msg_writer() << desc_all << '\n' << tmp_wallet.get_commands_str();
      return false;
    }
    else if (command_line::get_arg(vm, command_line::arg_version))
    {
      success_msg_writer() << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG;
      return false;
    }

    auto parser = po::command_line_parser(argc, argv).options(desc_params).positional(positional_options);
    po::store(parser.run(), vm);
    po::notify(vm);
    return true;
  });
  if (!r)
    return 1;

  //set up logging options
  log_space::get_set_log_detalisation_level(true, LOG_LEVEL_2);
  //log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_0);
  log_space::log_singletone::add_logger(LOGGER_FILE,
    log_space::log_singletone::get_default_log_file().c_str(),
    log_space::log_singletone::get_default_log_folder().c_str(), LOG_LEVEL_4);

  message_writer(epee::log_space::console_color_white, true) << CRYPTONOTE_NAME << " wallet v" << PROJECT_VERSION_LONG;

  if (command_line::has_arg(vm, arg_log_level))
  {
    LOG_PRINT_L0("Setting log level = " << command_line::get_arg(vm, arg_log_level));
    log_space::get_set_log_detalisation_level(true, command_line::get_arg(vm, arg_log_level));
  }

  cryptonote::CurrencyBuilder currencyBuilder;
  currencyBuilder.testnet(command_line::get_arg(vm, arg_testnet));
  cryptonote::Currency currency = currencyBuilder.currency();

  if (command_line::has_arg(vm, tools::wallet_rpc_server::arg_rpc_bind_port))
  {
    log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_2);
    //runs wallet with rpc interface
    if (!command_line::has_arg(vm, arg_wallet_file))
    {
      fail_msg_writer() << "Wallet file not set.";
      return 1;
    }
    if (!command_line::has_arg(vm, arg_daemon_address))
    {
      fail_msg_writer() << "Daemon address not set.";
      return 1;
    }
    if (!command_line::has_arg(vm, arg_password))
    {
      fail_msg_writer() << "Wallet password not set.";
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
      daemon_port = RPC_DEFAULT_PORT;
    if (daemon_address.empty())
      daemon_address = std::string("http://") + daemon_host + ":" + std::to_string(daemon_port);


    std::unique_ptr<INode> node;

    node.reset(new NodeRpcProxy(daemon_host, daemon_port));

    std::promise<std::error_code> errorPromise;
    std::future<std::error_code> error = errorPromise.get_future();
    auto callback = [&errorPromise](std::error_code e) {errorPromise.set_value(e); };
    node->init(callback);
    if (error.get()) {
      fail_msg_writer() << ("failed to init NodeRPCProxy");
      return 1;
    }

    std::unique_ptr<IWallet> wallet;

    wallet.reset(new Wallet(currency, *node.get()));
    std::string walletFileName;
    try
    {
      walletFileName = ::tryToOpenWalletOrLoadKeysOrThrow(wallet, wallet_file, wallet_password);      
      LOG_PRINT_L1("available balance: " << currency.formatAmount(wallet->actualBalance()) <<
        ", locked amount: " << currency.formatAmount(wallet->pendingBalance()));
      LOG_PRINT_GREEN("Loaded ok", LOG_LEVEL_0);
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Wallet initialize failed: " << e.what();
      return 1;
    }

    tools::wallet_rpc_server wrpc(*wallet, *node, currency, walletFileName);
    wrpc.init(vm);
    CHECK_AND_ASSERT_MES(r, 1, "Failed to initialize wallet rpc server");

    tools::SignalHandler::install([&wrpc, &wallet] {
      wrpc.send_stop_signal();
    });
    LOG_PRINT_L0("Starting wallet rpc server");
    wrpc.run();
    LOG_PRINT_L0("Stopped wallet rpc server");
    try
    {
      LOG_PRINT_L0("Storing wallet...");
      std::ofstream walletFile;
      walletFile.open(walletFileName, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
      if (walletFile.fail())
        return false;
      WalletHelper::SaveWalletResultObserver saveObserver;
      std::future<std::error_code> f_saveError = saveObserver.saveResult.get_future();
      wallet->addObserver(&saveObserver);
      wallet->save(walletFile);
      auto saveError = f_saveError.get();
      wallet->removeObserver(&saveObserver);
      if (saveError) {
        fail_msg_writer() << "Failed to store wallet: " << saveError.message();
        return 1;
      }
      LOG_PRINT_GREEN("Stored ok", LOG_LEVEL_0);
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << "Failed to store wallet: " << e.what();
      return 1;
    }
  }
  else
  {
    //runs wallet with console interface
    cryptonote::simple_wallet wal(currency);
    r = wal.init(vm);
    CHECK_AND_ASSERT_MES(r, 1, "Failed to initialize wallet");

    std::vector<std::string> command = command_line::get_arg(vm, arg_command);
    if (!command.empty())
      wal.process_command(command);

    tools::SignalHandler::install([&wal] {
      wal.stop();
    });
    wal.run();

    if (!wal.deinit()) {
      fail_msg_writer() << "Failed to close wallet";
    }    
  }
  return 1;
  //CATCH_ENTRY_L0("main", 1);
}
