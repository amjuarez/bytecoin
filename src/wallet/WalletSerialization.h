// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
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
struct DepositInfo;
struct Deposit;

void serialize(UnconfirmedTransferDetails& utd, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(TransactionInfo& txi, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(Transfer& tr, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(DepositInfo& depositInfo, const std::string& name, cryptonote::ISerializer& serializer);
void serialize(Deposit& deposit, const std::string& name, cryptonote::ISerializer& serializer);

}

