// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 

#pragma once

#ifndef _STRING_TOOLS_H_
#define _STRING_TOOLS_H_

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "warnings.h"

#ifndef OUT
	#define OUT
#endif

#ifdef WINDOWS_PLATFORM
#pragma comment (lib, "Rpcrt4.lib")
#endif

// Don't include lexical_cast.hpp, to reduce compilation time
//#include <boost/lexical_cast.hpp>

namespace boost {
  namespace uuids {
    struct uuid;
  }

  template <typename Target, typename Source>
  inline Target lexical_cast(const Source &arg);
}

namespace epee
{
namespace string_tools
{
  std::wstring get_str_from_guid(const boost::uuids::uuid& rid);
  std::string get_str_from_guid_a(const boost::uuids::uuid& rid);
  bool get_guid_from_string( boost::uuids::uuid& inetifer, std::wstring str_id);
  bool get_guid_from_string(OUT boost::uuids::uuid& inetifer, const std::string& str_id);
  //----------------------------------------------------------------------------
  template<class CharT>
  std::basic_string<CharT> buff_to_hex(const std::basic_string<CharT>& s)
  {
    std::basic_stringstream<CharT> hexStream;
    hexStream << std::hex << std::noshowbase << std::setw(2);

    for(typename std::basic_string<CharT>::const_iterator it = s.begin(); it != s.end(); it++)
    {
      hexStream << "0x"<< static_cast<unsigned int>(static_cast<unsigned char>(*it)) << " ";
    }
    return hexStream.str();
  }
  //----------------------------------------------------------------------------
  template<class CharT>
  std::basic_string<CharT> buff_to_hex_nodelimer(const std::basic_string<CharT>& s)
  {
    std::basic_stringstream<CharT> hexStream;
    hexStream << std::hex << std::noshowbase;

    for(typename std::basic_string<CharT>::const_iterator it = s.begin(); it != s.end(); it++)
    {
      hexStream << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(static_cast<unsigned char>(*it));
    }
    return hexStream.str();
  }
  //----------------------------------------------------------------------------
  template<class CharT>
  bool parse_hexstr_to_binbuff(const std::basic_string<CharT>& s, std::basic_string<CharT>& res)
  {
    res.clear();
    try
    {
      long v = 0;
      for(size_t i = 0; i < (s.size() + 1) / 2; i++)
      {
        CharT byte_str[3];
        size_t copied = s.copy(byte_str, 2, 2 * i);
        byte_str[copied] = CharT(0);
        CharT* endptr;
        v = strtoul(byte_str, &endptr, 16);
        if (v < 0 || 0xFF < v || endptr != byte_str + copied)
        {
          return false;
        }
        res.push_back(static_cast<unsigned char>(v));
      }

      return true;
    }catch(...)
    {
      return false;
    }
  }
  //----------------------------------------------------------------------------
  template<class t_pod_type>
  bool parse_tpod_from_hex_string(const std::string& str_hash, t_pod_type& t_pod)
  {
    std::string buf;
    bool res = epee::string_tools::parse_hexstr_to_binbuff(str_hash, buf);
    if (!res || buf.size() != sizeof(t_pod_type))
    {
      return false;
    }
    else
    {
      buf.copy(reinterpret_cast<char *>(&t_pod), sizeof(t_pod_type));
      return true;
    }
  }
  //----------------------------------------------------------------------------
PUSH_WARNINGS
DISABLE_GCC_WARNING(maybe-uninitialized)
  template<class XType>
  inline bool get_xtype_from_string(OUT XType& val, const std::string& str_id)
  {
    if (std::is_integral<XType>::value && !std::numeric_limits<XType>::is_signed && !std::is_same<XType, bool>::value)
    {
      for (char c : str_id)
      {
        if (!std::isdigit(c))
          return false;
      }
    }

    try
    {
      val = boost::lexical_cast<XType>(str_id);
      return true;
    }
    catch(std::exception& /*e*/)
    {
      //const char* pmsg = e.what();
      return false;
    }
    catch(...)
    {
      return false;
    }

    return true;
  }
POP_WARNINGS
	//---------------------------------------------------
	template<typename int_t>
	bool get_xnum_from_hex_string(const std::string str, int_t& res )
	{
		try
		{
			std::stringstream ss;
			ss << std::hex << str;
			ss >> res;
			return true;	
		}
		catch(...)
		{
			return false;
		}
	}
	//----------------------------------------------------------------------------
	template<class XType>
	inline bool xtype_to_string(const  XType& val, std::string& str)
	{
		try
		{
			str = boost::lexical_cast<std::string>(val);
		}
		catch(...)
		{
			return false;
		}

		return true;
	}
    
	typedef std::map<std::string, std::string> command_line_params_a;
	typedef std::map<std::wstring, std::wstring> command_line_params_w;

	template<class t_string>
	bool parse_commandline(std::map<t_string, t_string>& res, int argc, char** argv)
	{
    t_string key;
    for(int i = 1; i < argc; i++)
    {
      if(!argv[i])
        break;
      t_string s = argv[i];
      std::string::size_type p = s.find('=');
      if(std::string::npos == p)
      {
        res[s] = "";
      }else
      {
        std::string ss;
        t_string nm = s.substr(0, p);
        t_string vl = s.substr(p+1, s.size());
        res[nm] = vl;
      }
    }
    return true;
	}

	template<class t_string, typename t_type>
	bool get_xparam_from_command_line(const std::map<t_string, t_string>& res, const t_string & key, t_type& val)
	{
		typename std::map<t_string, t_string>::const_iterator it = res.find(key);
		if(it == res.end())
			return false;

		if(it->second.size())
		{
			return get_xtype_from_string(val, it->second);
		}

		return true;
	}

  template<class t_string, typename t_type>
  t_type get_xparam_from_command_line(const std::map<t_string, t_string>& res, const t_string & key, const t_type& default_value)
  {
      typename std::map<t_string, t_string>::const_iterator it = res.find(key);
      if(it == res.end())
          return default_value;

      if(it->second.size())
      {
          t_type s;
          get_xtype_from_string(s, it->second);
          return s;
      }

      return default_value;
  }

  template<class t_string>
  bool have_in_command_line(const std::map<t_string, t_string>& res, const std::basic_string<typename t_string::value_type>& key)
  {
    typename std::map<t_string, t_string>::const_iterator it = res.find(key);
    if(it == res.end())
      return false;

    return true;
  }

  //----------------------------------------------------------------------------
  std::string get_ip_string_from_int32(uint32_t ip);
  bool get_ip_int32_from_string(uint32_t& ip, const std::string& ip_str);
  bool parse_peer_from_string(uint32_t& ip, uint32_t& port, const std::string& addres);
  //----------------------------------------------------------------------------
  template<typename t>
  inline std::string get_t_as_hex_nwidth(const t& v, std::streamsize w = 8)
  {
    std::stringstream ss;
    ss << std::setfill ('0') << std::setw (w) << std::hex << std::noshowbase;
    ss << v;
    return ss.str();
  }
  //----------------------------------------------------------------------------
  std::string num_to_string_fast(int64_t val);
  bool string_to_num_fast(const std::string& buff, int64_t& val);
  bool string_to_num_fast(const std::string& buff, int& val);
#ifdef WINDOWS_PLATFORM
  std::string system_time_to_string(const SYSTEMTIME& st);
#endif
  bool compare_no_case(const std::string& str1, const std::string& str2);
  bool compare_no_case(const std::wstring& str1, const std::wstring& str2);
  bool is_match_prefix(const std::wstring& str1, const std::wstring& prefix);
  bool is_match_prefix(const std::string& str1, const std::string& prefix);
  std::string& get_current_module_name();
  std::string& get_current_module_folder();
#ifdef _WIN32
  std::string get_current_module_path();
#endif
  bool set_module_name_and_folder(const std::string& path_to_process_);
  bool trim_left(std::string& str);
  bool trim_right(std::string& str);
  std::string& trim(std::string& str);
  std::string trim(const std::string& str_);
  //----------------------------------------------------------------------------
  template<class t_pod_type>
  std::string pod_to_hex(const t_pod_type& s)
  {
    std::string buff;
    buff.assign(reinterpret_cast<const char*>(&s), sizeof(s));
    return buff_to_hex_nodelimer(buff);
  }
  //----------------------------------------------------------------------------
  template<class t_pod_type>
  bool hex_to_pod(const std::string& hex_str, t_pod_type& s)
  {
    std::string hex_str_tr = trim(hex_str);
    if(sizeof(s)*2 != hex_str.size())
      return false;
    std::string bin_buff;
    if(!parse_hexstr_to_binbuff(hex_str_tr, bin_buff))
      return false;
    if(bin_buff.size()!=sizeof(s))
      return false;

    s = *(t_pod_type*)bin_buff.data();
    return true;
  }
  //----------------------------------------------------------------------------
  std::string get_extension(const std::string& str);
  std::string get_filename_from_path(const std::string& str);
  std::string cut_off_extension(const std::string& str);
#ifdef _WININET_
  std::string get_string_from_systemtime(const SYSTEMTIME& sys_time);
  SYSTEMTIME get_systemtime_from_string(const std::string& buff);
#endif

#ifdef WINDOWS_PLATFORM
  const DWORD INFO_BUFFER_SIZE = 10000;

  const wchar_t* get_pc_name();
  const wchar_t* get_user_name();
#endif

#ifdef _LM_
  const wchar_t* get_domain_name();
#endif
#ifdef WINDOWS_PLATFORM
  std::string load_resource_string_a(int id, const char* pmodule_name = NULL);
  std::wstring load_resource_string_w(int id, const char* pmodule_name = NULL);
#endif
}
}
#endif //_STRING_TOOLS_H_
