// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdexcept>
#include <algorithm>
#include <string>

#include "IWallet.h"

namespace cryptonote {
class ISerializer;
}

namespace CryptoNote {

struct UnconfirmedTransferDetails;
struct TransactionInfo;
struct Transfer;

void serialize(UnconfirmedTransferDetails& utd, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(TransactionInfo& txi, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(Transfer& tr, const std::string& name, cryptonote::ISerializer& serializer);

}

