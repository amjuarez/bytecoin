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

#pragma once

#include <list>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include "p2p_protocol_types.h"
#include "cryptonote_config.h"

namespace CryptoNote {

/************************************************************************/
/*                                                                      */
/************************************************************************/
class peerlist_manager
{
public:
  bool init(bool allow_local_ip);
  size_t get_white_peers_count(){ return m_peers_white.size(); }
  size_t get_gray_peers_count(){ return m_peers_gray.size(); }
  bool merge_peerlist(const std::list<peerlist_entry>& outer_bs);
  bool get_peerlist_head(std::list<peerlist_entry>& bs_head, uint32_t depth = CryptoNote::P2P_DEFAULT_PEERS_IN_HANDSHAKE);
  bool get_peerlist_full(std::list<peerlist_entry>& pl_gray, std::list<peerlist_entry>& pl_white);
  bool get_white_peer_by_index(peerlist_entry& p, size_t i);
  bool get_gray_peer_by_index(peerlist_entry& p, size_t i);
  bool append_with_peer_white(const peerlist_entry& pr);
  bool append_with_peer_gray(const peerlist_entry& pr);
  bool set_peer_just_seen(peerid_type peer, uint32_t ip, uint32_t port);
  bool set_peer_just_seen(peerid_type peer, const net_address& addr);
  bool set_peer_unreachable(const peerlist_entry& pr);
  bool is_ip_allowed(uint32_t ip);
  void trim_white_peerlist();
  void trim_gray_peerlist();

private:

  struct by_time{};
  struct by_id{};
  struct by_addr{};

  typedef boost::multi_index_container<
    peerlist_entry,
    boost::multi_index::indexed_by<
    // access by peerlist_entry::net_adress
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_addr>, boost::multi_index::member<peerlist_entry, net_address, &peerlist_entry::adr> >,
    // sort by peerlist_entry::last_seen<
    boost::multi_index::ordered_non_unique<boost::multi_index::tag<by_time>, boost::multi_index::member<peerlist_entry, time_t, &peerlist_entry::last_seen> >
    >
  > peers_indexed;

public:

  template <class Archive, class t_version_type>
  void serialize(Archive &a, const t_version_type ver)
  {
    if (ver < 4)
      return;

    a & m_peers_white;
    a & m_peers_gray;
  }

private:

  friend class boost::serialization::access;
  std::string m_config_folder;
  bool m_allow_local_ip;
  peers_indexed m_peers_gray;
  peers_indexed m_peers_white;
};

}

BOOST_CLASS_VERSION(CryptoNote::peerlist_manager, 4)
