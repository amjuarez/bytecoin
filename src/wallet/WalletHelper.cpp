#include "WalletHelper.h"
#include <boost/filesystem.hpp>

#include "string_tools.h"
#include "cryptonote_protocol/blobdatatype.h"

using namespace cryptonote;
using namespace epee;


void WalletHelper::prepareFileNames(const std::string& file_path, std::string& keys_file, std::string& wallet_file) {
  keys_file = file_path;
  wallet_file = file_path;
  boost::system::error_code e;
  if (string_tools::get_extension(keys_file) == "keys") {//provided keys file name
    wallet_file = string_tools::cut_off_extension(wallet_file);
  } else {//provided wallet file name
    keys_file += ".keys";
  }
}
