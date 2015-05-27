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

#include "PeerListManager.h"

#include <time.h>
#include <boost/foreach.hpp>
#include <System/Ipv4Address.h>

using namespace CryptoNote;

//--------------------------------------------------------------------------------------------------
bool peerlist_manager::init(bool allow_local_ip)
{
  m_allow_local_ip = allow_local_ip;
  return true;
}

//--------------------------------------------------------------------------------------------------
 void peerlist_manager::trim_white_peerlist()
{
  while (m_peers_gray.size() > CryptoNote::P2P_LOCAL_GRAY_PEERLIST_LIMIT)
  {
    peers_indexed::index<by_time>::type& sorted_index = m_peers_gray.get<by_time>();
    sorted_index.erase(sorted_index.begin());
  }
}
//--------------------------------------------------------------------------------------------------
 void peerlist_manager::trim_gray_peerlist()
{
  while (m_peers_white.size() > CryptoNote::P2P_LOCAL_WHITE_PEERLIST_LIMIT)
  {
    peers_indexed::index<by_time>::type& sorted_index = m_peers_white.get<by_time>();
    sorted_index.erase(sorted_index.begin());
  }
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::merge_peerlist(const std::list<peerlist_entry>& outer_bs)
{ 
  for(const peerlist_entry& be : outer_bs) {
    append_with_peer_gray(be);
  }

  // delete extra elements
  trim_gray_peerlist();
  return true;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::get_white_peer_by_index(peerlist_entry& p, size_t i)
{
  if (i >= m_peers_white.size())
    return false;

  peers_indexed::index<by_time>::type& by_time_index = m_peers_white.get<by_time>();

  auto it = by_time_index.rbegin();
  std::advance(it, i);
  p = *it;

  return true;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::get_gray_peer_by_index(peerlist_entry& p, size_t i)
{ 
  if (i >= m_peers_gray.size())
    return false;

  peers_indexed::index<by_time>::type& by_time_index = m_peers_gray.get<by_time>();
  
  auto it = by_time_index.rbegin();
  std::advance(it, i);
  p = *it;

  return true;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::is_ip_allowed(uint32_t ip)
{
  System::Ipv4Address addr(networkToHost(ip));

  //never allow loopback ip
  if (addr.isLoopback()) {
    return false;
  }

  if (!m_allow_local_ip && addr.isPrivate()) {
    return false;
  }

  return true;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::get_peerlist_head(std::list<peerlist_entry>& bs_head, uint32_t depth)
{
  peers_indexed::index<by_time>::type& by_time_index = m_peers_white.get<by_time>();
  uint32_t cnt = 0;

  BOOST_REVERSE_FOREACH(const peers_indexed::value_type& vl, by_time_index)
  {
    if (!vl.last_seen)
      continue;
    bs_head.push_back(vl);
    if (cnt++ > depth)
      break;
  }
  return true;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::get_peerlist_full(std::list<peerlist_entry>& pl_gray, std::list<peerlist_entry>& pl_white)
{
  peers_indexed::index<by_time>::type& by_time_index_gr = m_peers_gray.get<by_time>();
  peers_indexed::index<by_time>::type& by_time_index_wt = m_peers_white.get<by_time>();

  std::copy(by_time_index_gr.rbegin(), by_time_index_gr.rend(), std::back_inserter(pl_gray));
  std::copy(by_time_index_wt.rbegin(), by_time_index_wt.rend(), std::back_inserter(pl_white));

  return true;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::set_peer_just_seen(peerid_type peer, uint32_t ip, uint32_t port)
{
  net_address addr;
  addr.ip = ip;
  addr.port = port;
  return set_peer_just_seen(peer, addr);
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::set_peer_just_seen(peerid_type peer, const net_address& addr)
{
  try {
    //find in white list
    peerlist_entry ple;
    ple.adr = addr;
    ple.id = peer;
    ple.last_seen = time(NULL);
    return append_with_peer_white(ple);
  } catch (std::exception&) {
  }

  return false;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::append_with_peer_white(const peerlist_entry& ple)
{
  try {
    if (!is_ip_allowed(ple.adr.ip))
      return true;

    //find in white list
    auto by_addr_it_wt = m_peers_white.get<by_addr>().find(ple.adr);
    if (by_addr_it_wt == m_peers_white.get<by_addr>().end()) {
      //put new record into white list
      m_peers_white.insert(ple);
      trim_white_peerlist();
    } else {
      //update record in white list 
      m_peers_white.replace(by_addr_it_wt, ple);
    }
    //remove from gray list, if need
    auto by_addr_it_gr = m_peers_gray.get<by_addr>().find(ple.adr);
    if (by_addr_it_gr != m_peers_gray.get<by_addr>().end()) {
      m_peers_gray.erase(by_addr_it_gr);
    }
    return true;
  } catch (std::exception&) {
  }
  return false;
}
//--------------------------------------------------------------------------------------------------

bool peerlist_manager::append_with_peer_gray(const peerlist_entry& ple)
{
  try {
    if (!is_ip_allowed(ple.adr.ip))
      return true;

    //find in white list
    auto by_addr_it_wt = m_peers_white.get<by_addr>().find(ple.adr);
    if (by_addr_it_wt != m_peers_white.get<by_addr>().end())
      return true;

    //update gray list
    auto by_addr_it_gr = m_peers_gray.get<by_addr>().find(ple.adr);
    if (by_addr_it_gr == m_peers_gray.get<by_addr>().end())
    {
      //put new record into white list
      m_peers_gray.insert(ple);
      trim_gray_peerlist();
    } else
    {
      //update record in white list 
      m_peers_gray.replace(by_addr_it_gr, ple);
    }
    return true;
  } catch (std::exception&) {
  }
  return false;
}
//--------------------------------------------------------------------------------------------------
