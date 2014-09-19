// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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

#if defined(WIN32)
#include <io.h>
#endif

#include <fstream>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

// epee
#include "include_base_utils.h"
#include "misc_os_dependent.h"

namespace tools
{
  template<class t_object>
  bool serialize_obj_to_file(t_object& obj, const std::string& file_path)
  {
    TRY_ENTRY();
#if defined(_MSC_VER)
    // Need to know HANDLE of file to call FlushFileBuffers
    HANDLE data_file_handle = ::CreateFile(file_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == data_file_handle)
      return false;

    int data_file_descriptor = _open_osfhandle((intptr_t)data_file_handle, 0);
    if (-1 == data_file_descriptor)
    {
      ::CloseHandle(data_file_handle);
      return false;
    }

    FILE* data_file_file = _fdopen(data_file_descriptor, "wb");
    if (0 == data_file_file)
    {
      // Call CloseHandle is not necessary
      _close(data_file_descriptor);
      return false;
    }

    // HACK: undocumented constructor, this code may not compile
    std::ofstream data_file(data_file_file);
    if (data_file.fail())
    {
      // Call CloseHandle and _close are not necessary
      fclose(data_file_file);
      return false;
    }
#else
    std::ofstream data_file;
    data_file.open(file_path , std::ios_base::binary | std::ios_base::out| std::ios::trunc);
    if (data_file.fail())
      return false;
#endif

    boost::archive::binary_oarchive a(data_file);
    a << obj;
    if (data_file.fail())
      return false;

    data_file.flush();
#if defined(_MSC_VER)
    // To make sure the file is fully stored on disk
    ::FlushFileBuffers(data_file_handle);
    fclose(data_file_file);
#endif

    return true;
    CATCH_ENTRY_L0("serialize_obj_to_file", false);
  }

  template<class t_object>
  bool unserialize_obj_from_file(t_object& obj, const std::string& file_path)
  {
    TRY_ENTRY();

    std::ifstream data_file;  
    data_file.open( file_path, std::ios_base::binary | std::ios_base::in);
    if(data_file.fail())
      return false;
    boost::archive::binary_iarchive a(data_file);

    a >> obj;
    return !data_file.fail();
    CATCH_ENTRY_L0("unserialize_obj_from_file", false);
  }
}
