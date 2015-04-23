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

#include "string_tools.h"

#include <locale>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <boost/algorithm/string.hpp>


#ifdef WINDOWS_PLATFORM
#pragma comment (lib, "Rpcrt4.lib")
#endif

namespace epee
{
namespace string_tools
{
	std::wstring get_str_from_guid(const boost::uuids::uuid& rid)
	{
		return boost::lexical_cast<std::wstring>(rid);
	}
	//----------------------------------------------------------------------------
	std::string get_str_from_guid_a(const boost::uuids::uuid& rid)
	{
		return boost::lexical_cast<std::string>(rid);
	}
	//----------------------------------------------------------------------------
	bool get_guid_from_string( boost::uuids::uuid& inetifer, std::wstring str_id)
	{
		if(str_id.size() < 36)
			return false;

		if('{' == *str_id.begin())
			str_id.erase(0, 1);

		if('}' == *(--str_id.end()))
			str_id.erase(--str_id.end());

		try
		{
			inetifer = boost::lexical_cast<boost::uuids::uuid>(str_id);
			return true;
		}
		catch(...)
		{
			return false;
		}
	}
	//----------------------------------------------------------------------------
	bool get_guid_from_string(OUT boost::uuids::uuid& inetifer, const std::string& str_id)
	{
		std::string local_str_id = str_id;
		if(local_str_id.size() < 36)
			return false;

		if('{' == *local_str_id.begin())
			local_str_id.erase(0, 1);

		if('}' == *(--local_str_id.end()))
			local_str_id.erase(--local_str_id.end());

		try
		{
			inetifer = boost::lexical_cast<boost::uuids::uuid>(local_str_id);
			return true;
		}
		catch(...)
		{
			return false;
		}
	}
	//----------------------------------------------------------------------------
//#ifdef _WINSOCK2API_
	std::string get_ip_string_from_int32(uint32_t ip)
	{
		in_addr adr;
		adr.s_addr = ip;
		const char* pbuf = inet_ntoa(adr);
		if(pbuf)
			return pbuf;
		else
			return "[failed]";
	}
	//----------------------------------------------------------------------------
	bool get_ip_int32_from_string(uint32_t& ip, const std::string& ip_str)
	{
		ip = inet_addr(ip_str.c_str());
		if(INADDR_NONE == ip)
			return false;

		return true;
	}
  //----------------------------------------------------------------------------
  bool parse_peer_from_string(uint32_t& ip, uint32_t& port, const std::string& addres)
  {
    //parse ip and address
    std::string::size_type p = addres.find(':');
    if(p == std::string::npos)
    {
      return false;
    }
    std::string ip_str = addres.substr(0, p);
    std::string port_str = addres.substr(p+1, addres.size());

    if(!get_ip_int32_from_string(ip, ip_str))
    {
      return false;
    }

    if(!get_xtype_from_string(port, port_str))
    {
      return false;
    }
    return true;
  }

	//----------------------------------------------------------------------------
	std::string num_to_string_fast(int64_t val)
	{
		/*
		char  buff[30] = {0};
		i64toa_s(val, buff, sizeof(buff)-1, 10);
		return buff;*/
		return boost::lexical_cast<std::string>(val);
	}
	//----------------------------------------------------------------------------
	bool string_to_num_fast(const std::string& buff, int64_t& val)
	{
		//return get_xtype_from_string(val, buff);
#if (defined _MSC_VER)
    val = _atoi64(buff.c_str());
#else
    val = atoll(buff.c_str());
#endif
		/*
		 * val = atoi64(buff.c_str());
     */
		if(buff != "0" && val == 0)
			return false;
		return true;
	}
	//----------------------------------------------------------------------------
	bool string_to_num_fast(const std::string& buff, int& val)
	{
		val = atoi(buff.c_str());
		if(buff != "0" && val == 0)
			return false;

		return true;
	}
	//----------------------------------------------------------------------------
#ifdef WINDOWS_PLATFORM
	std::string system_time_to_string(const SYSTEMTIME& st)
	{
	
		/*
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		SystemTimeToTzSpecificLocalTime(&tzi, &stUTC, &stLocal);
		*/

		char szTime[25], szDate[25];
		::GetTimeFormatA(
			LOCALE_USER_DEFAULT,    // locale
			TIME_FORCE24HOURFORMAT, // options
			&st,					// time
			NULL,                   // time format string
			szTime,                 // formatted string buffer
			25                      // size of string buffer
			);

		::GetDateFormatA(
			LOCALE_USER_DEFAULT,    // locale
			NULL,                   // options
			&st,					// date
			NULL,                   // date format
			szDate,                 // formatted string buffer
			25                      // size of buffer
			);
		szTime[24] = szDate[24] = 0; //be happy :)
		
		std::string res = szDate;
		(res += " " )+= szTime;
		return res; 

	}
#endif
	//----------------------------------------------------------------------------
	
	bool compare_no_case(const std::string& str1, const std::string& str2)
	{
		
		return !boost::iequals(str1, str2);
	}
	//----------------------------------------------------------------------------
	bool compare_no_case(const std::wstring& str1, const std::wstring& str2)
	{
		return !boost::iequals(str1, str2);
	}
	//----------------------------------------------------------------------------
	bool is_match_prefix(const std::wstring& str1, const std::wstring& prefix)
	{	
		if(prefix.size()>str1.size())
			return false;

		if(!compare_no_case(str1.substr(0, prefix.size()), prefix))
			return true;
		else
			return false;
	}
	//----------------------------------------------------------------------------
	bool is_match_prefix(const std::string& str1, const std::string& prefix)
	{	
		if(prefix.size()>str1.size())
			return false;

		if(!compare_no_case(str1.substr(0, prefix.size()), prefix))
			return true;
		else
			return false;
	}
	//----------------------------------------------------------------------------
	std::string& get_current_module_name()
	{
		static std::string module_name;
		return module_name;
	}
	//----------------------------------------------------------------------------
	std::string& get_current_module_folder()
	{	
		static std::string module_folder;
		return module_folder;
	}
  //----------------------------------------------------------------------------
#ifdef _WIN32
  std::string get_current_module_path()
  {
    char pname [5000] = {0};
    GetModuleFileNameA( NULL, pname, sizeof(pname));
    pname[sizeof(pname)-1] = 0; //be happy ;)
    return pname;
  }
#endif
	//----------------------------------------------------------------------------
	bool set_module_name_and_folder(const std::string& path_to_process_)
	{
    std::string path_to_process = path_to_process_;
#ifdef _WIN32
    path_to_process = get_current_module_path();
#endif 
		std::string::size_type a = path_to_process.rfind( '\\' );
		if(a == std::string::npos )
		{
			a = path_to_process.rfind( '/' );
		}
		if ( a != std::string::npos )
		{	
			get_current_module_name() = path_to_process.substr(a+1, path_to_process.size());
			get_current_module_folder() = path_to_process.substr(0, a);
			return true;
		}else
			return false;

	}

	//----------------------------------------------------------------------------
	bool trim_left(std::string& str)
	{
		for(std::string::iterator it = str.begin(); it!= str.end() && isspace(static_cast<unsigned char>(*it));)
			str.erase(str.begin());
			
		return true;
	}
	//----------------------------------------------------------------------------
	bool trim_right(std::string& str)
	{

		for(std::string::reverse_iterator it = str.rbegin(); it!= str.rend() && isspace(static_cast<unsigned char>(*it));)
			str.erase( --((it++).base()));

		return true;
	}
	//----------------------------------------------------------------------------
	std::string& trim(std::string& str)
	{

		trim_left(str);
		trim_right(str);
		return str;
	}
  //----------------------------------------------------------------------------
  std::string trim(const std::string& str_)
  {
    std::string str = str_;
    trim_left(str);
    trim_right(str);
    return str;
  }
  //----------------------------------------------------------------------------
	std::string get_extension(const std::string& str)
	{
		std::string res;
		std::string::size_type pos = str.rfind('.');
		if(std::string::npos == pos)
			return res;
		
		res = str.substr(pos+1, str.size()-pos);
		return res;
	}
	//----------------------------------------------------------------------------
	std::string get_filename_from_path(const std::string& str)
	{
		std::string res;
		std::string::size_type pos = str.rfind('\\');
		if(std::string::npos == pos)
			return str;

		res = str.substr(pos+1, str.size()-pos);
		return res;
	}
	//----------------------------------------------------------------------------



	std::string cut_off_extension(const std::string& str)
	{
		std::string res;
		std::string::size_type pos = str.rfind('.');
		if(std::string::npos == pos)
			return str;

		res = str.substr(0, pos);
		return res;
	}

	//----------------------------------------------------------------------------
#ifdef _WININET_
	std::string get_string_from_systemtime(const SYSTEMTIME& sys_time)
	{
		std::string result_string;

		char buff[100] = {0};
		BOOL res = ::InternetTimeFromSystemTimeA(&sys_time, INTERNET_RFC1123_FORMAT, buff, 99);
		if(!res)
		{
			LOG_ERROR("Failed to load SytemTime to string");
		}

		result_string = buff;
		return result_string;

	}
	//-------------------------------------------------------------------------------------
	SYSTEMTIME get_systemtime_from_string(const std::string& buff)
	{
		SYSTEMTIME result_time = {0};

		BOOL res = ::InternetTimeToSystemTimeA(buff.c_str(), &result_time, NULL);
		if(!res)
		{
			LOG_ERROR("Failed to load SytemTime from string " << buff << "interval set to 15 minutes");
		}

		return result_time;
	}
#endif

#ifdef WINDOWS_PLATFORM
	const wchar_t* get_pc_name()
	{
		static wchar_t	info[INFO_BUFFER_SIZE];
		static DWORD	bufCharCount = INFO_BUFFER_SIZE;
		static bool		init = false;

		if (!init) {
			if (!GetComputerNameW( info, &bufCharCount ))
				info[0] = 0;
			else
				init = true;
		}

		return info;
	}

	const wchar_t* get_user_name()
	{
		static wchar_t	info[INFO_BUFFER_SIZE];
		static DWORD	bufCharCount = INFO_BUFFER_SIZE;
		static bool		init = false;

		if (!init) {
			if (!GetUserNameW( info, &bufCharCount ))
				info[0] = 0;
			else
				init = true;
		}

		return info;
	}
#endif

#ifdef _LM_
	const wchar_t* get_domain_name()
	{
		static wchar_t	info[INFO_BUFFER_SIZE];
		static DWORD	bufCharCount = 0;
		static bool		init = false;

		if (!init) {
			LPWSTR domain( NULL );
			NETSETUP_JOIN_STATUS status;
			info[0] = 0;

			if (NET_API_STATUS result = NetGetJoinInformation( NULL, &domain, &status )) 
			{
				LOG_ERROR("get_domain_name error: " << log_space::get_win32_err_descr(result));
			} else
			{
				StringCchCopyW( info, sizeof(info)/sizeof( info[0] ), domain );
				NetApiBufferFree((void*)domain);
				init = true;
			}
		}

		return info;
	}
#endif
#ifdef WINDOWS_PLATFORM
	inline
	std::string load_resource_string_a(int id, const char* pmodule_name = NULL)
	{
		//slow realization 
		HMODULE h = ::GetModuleHandleA( pmodule_name );
		
		char buff[2000] = {0};
		
		::LoadStringA( h, id, buff, sizeof(buff));
		buff[sizeof(buff)-1] = 0; //be happy :)
		return buff;
	}
	inline
	std::wstring load_resource_string_w(int id, const char* pmodule_name = NULL)
	{
		//slow realization 
		HMODULE h = ::GetModuleHandleA( pmodule_name );
		
		wchar_t buff[2000] = {0};
		
		::LoadStringW( h, id, buff, sizeof(buff) / sizeof( buff[0] ) );
		buff[(sizeof(buff)/sizeof(buff[0]))-1] = 0; //be happy :)
		return buff;	
	}
#endif
}
}
