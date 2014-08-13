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

#ifndef _MISC_LOG_EX_H_
#define _MISC_LOG_EX_H_

#if defined(WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <iostream>
#include <list>
#include <map>
#include <string>
#include <sstream>

#include "misc_os_dependent.h"
#include "static_initializer.h"
#include "syncobj.h"
#include "warnings.h"


#define LOG_LEVEL_SILENT     -1
#define LOG_LEVEL_0     0
#define LOG_LEVEL_1     1
#define LOG_LEVEL_2     2
#define LOG_LEVEL_3     3
#define LOG_LEVEL_4     4
#define LOG_LEVEL_MIN   LOG_LEVEL_SILENT
#define LOG_LEVEL_MAX   LOG_LEVEL_4


#define   LOGGER_NULL       0
#define   LOGGER_FILE       1
#define   LOGGER_DEBUGGER   2
#define   LOGGER_CONSOLE    3
#define   LOGGER_DUMP       4


#ifndef LOCAL_ASSERT
#include <assert.h>
#if (defined _MSC_VER)
#define LOCAL_ASSERT(expr) {if(epee::debug::get_set_enable_assert()){_ASSERTE(expr);}}
#else
#define LOCAL_ASSERT(expr)
#endif

#endif

namespace epee
{
namespace debug
{
  inline bool get_set_enable_assert(bool set = false, bool v = false)
  {
    static bool e = true;
    if(set)
      e = v;
    return e;
  }
}
namespace log_space
{
  class  logger;
  class  log_message;
  class  log_singletone;

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  enum console_colors
  {
    console_color_default,
    console_color_white,
    console_color_red,
    console_color_green,
    console_color_blue,
    console_color_cyan,
    console_color_magenta,
    console_color_yellow
  };


  struct ibase_log_stream
  {
    ibase_log_stream() {}
    virtual ~ibase_log_stream() {}
    virtual bool out_buffer(const char* buffer, int buffer_len , int log_level, int color, const char* plog_name = NULL)=0;
    virtual int get_type() const { return 0; }

    virtual bool set_max_logfile_size(uint64_t max_size) { return true; }
    virtual bool set_log_rotate_cmd(const std::string& cmd) { return true; }
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  struct delete_ptr
  {
    template <class P>
      void operator() (P p)
    {
      delete p.first;
    }
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/

  bool is_stdout_a_tty();
  void set_console_color(int color, bool bright);
  void reset_console_color();
  bool rotate_log_file(const char* pfile_path);

  //------------------------------------------------------------------------
#define max_dbg_str_len 80
#ifdef _MSC_VER
  class debug_output_stream: public ibase_log_stream
  {
    virtual bool out_buffer(const char* buffer, int buffer_len , int log_level, int color, const char* plog_name = NULL) override;
  };
#endif

  class console_output_stream: public ibase_log_stream
  {
#ifdef _MSC_VER
    bool m_have_to_kill_console;
#endif

  public:
    console_output_stream();
    virtual ~console_output_stream();

    virtual int get_type() const override { return LOGGER_CONSOLE; }
    virtual bool out_buffer(const char* buffer, int buffer_len , int log_level, int color, const char* plog_name = NULL) override;
  };

  //--------------------------------------------------------------------------//
  class file_output_stream : public ibase_log_stream
  {
  public:
    typedef std::map<std::string, std::ofstream*> named_log_streams;

    file_output_stream(std::string default_log_file_name, std::string log_path);
    ~file_output_stream();

  private:
    named_log_streams m_log_file_names;
    std::string       m_default_log_path;
    std::ofstream*    m_pdefault_file_stream;
    std::string     m_log_rotate_cmd;
    std::string     m_default_log_filename;
    uint64_t   m_max_logfile_size;

    virtual int get_type() const override { return LOGGER_FILE; }
    virtual bool out_buffer(const char* buffer, int buffer_len, int log_level, int color, const char* plog_name = NULL) override;
    virtual bool set_max_logfile_size(uint64_t max_size) override;
    virtual bool set_log_rotate_cmd(const std::string& cmd) override;

    std::ofstream* add_new_stream_and_open(const char* pstream_name);
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class log_stream_splitter
  {
  public:
    typedef std::list<std::pair<ibase_log_stream*, int> > streams_container;

    log_stream_splitter() { }
    ~log_stream_splitter();

    bool set_max_logfile_size(uint64_t max_size);
    bool set_log_rotate_cmd(const std::string& cmd);
    bool do_log_message(const std::string& rlog_mes, int log_level, int color, const char* plog_name = NULL);
    bool add_logger(int type, const char* pdefault_file_name, const char* pdefault_log_folder, int log_level_limit = LOG_LEVEL_4);
    bool add_logger(ibase_log_stream* pstream, int log_level_limit = LOG_LEVEL_4);
    bool remove_logger(int type);

  private:
    streams_container m_log_streams;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  int get_set_log_detalisation_level(bool is_need_set = false, int log_level_to_set = LOG_LEVEL_1);
  int get_set_time_level(bool is_need_set = false, int time_log_level = LOG_LEVEL_0);
  bool get_set_need_thread_id(bool is_need_set = false, bool is_need_val = false);
  bool get_set_need_proc_name(bool is_need_set = false, bool is_need_val = false);

  std::string get_daytime_string2();
  std::string get_day_time_string();
  std::string get_time_string();

#ifdef _MSC_VER
  inline std::string get_time_string_adv(SYSTEMTIME* pst = NULL);
#endif

  class logger
  {
  public:
    friend class log_singletone;

    logger();
    ~logger() { }

    bool set_max_logfile_size(uint64_t max_size);
    bool set_log_rotate_cmd(const std::string& cmd);
    bool take_away_journal(std::list<std::string>& journal);
    bool do_log_message(const std::string& rlog_mes, int log_level, int color, bool add_to_journal = false, const char* plog_name = NULL);
    bool add_logger(int type, const char* pdefault_file_name, const char* pdefault_log_folder , int log_level_limit = LOG_LEVEL_4);
    bool add_logger(ibase_log_stream* pstream, int log_level_limit = LOG_LEVEL_4);
    bool remove_logger(int type);
    bool set_thread_prefix(const std::string& prefix);
    std::string get_default_log_file() { return m_default_log_file; }
    std::string get_default_log_folder() { return m_default_log_folder; }

  private:
    bool init();
    bool init_default_loggers();
    bool init_log_path_by_default();

    log_stream_splitter m_log_target;

    std::string m_default_log_folder;
    std::string m_default_log_file;
    std::string m_process_name;
    std::map<std::string, std::string> m_thr_prefix_strings;
    std::list<std::string> m_journal;
    critical_section m_critical_sec;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class log_singletone
  {
  public:
    friend class initializer<log_singletone>;
    friend class logger;

    static int get_log_detalisation_level();
    static bool is_filter_error(int error_code);
    static bool do_log_message(const std::string& rlog_mes, int log_level, int color, bool keep_in_journal, const char* plog_name = NULL);
    static bool take_away_journal(std::list<std::string>& journal);
    static bool set_max_logfile_size(uint64_t file_size);
    static bool set_log_rotate_cmd(const std::string& cmd);
    static bool add_logger(int type, const char* pdefault_file_name, const char* pdefault_log_folder, int log_level_limit = LOG_LEVEL_4);
    static std::string get_default_log_file();
    static std::string get_default_log_folder();
    static bool add_logger( ibase_log_stream* pstream, int log_level_limit = LOG_LEVEL_4);
    static bool remove_logger(int type);

PUSH_WARNINGS
DISABLE_GCC_WARNING(maybe-uninitialized)
    static int get_set_log_detalisation_level(bool is_need_set = false, int log_level_to_set = LOG_LEVEL_1);
POP_WARNINGS

    static int get_set_time_level(bool is_need_set = false, int time_log_level = LOG_LEVEL_0);
    static int  get_set_process_level(bool is_need_set = false, int process_log_level = LOG_LEVEL_0);
    static bool get_set_need_thread_id(bool is_need_set = false, bool is_need_val = false);
    static bool get_set_need_proc_name(bool is_need_set = false, bool is_need_val = false);
    static uint64_t get_set_err_count(bool is_need_set = false, uint64_t err_val = false);

#ifdef _MSC_VER
    static void SetThreadName( DWORD dwThreadID, const char* threadName);
#endif

    static bool set_thread_log_prefix(const std::string& prefix);
    static std::string get_prefix_entry();

  private:
    log_singletone() { } //restric to create an instance
    //static initializer<log_singletone> m_log_initializer;//must be in one .cpp file (for example main.cpp) via DEFINE_LOGGING macro

    static bool init();
    static bool un_init();

    static logger* get_or_create_instance();
    static logger* get_set_instance_internal(bool is_need_set = false, logger* pnew_logger_val = NULL);
    static bool get_set_is_uninitialized(bool is_need_set = false, bool is_uninitialized = false);
  };

  const static initializer<log_singletone> log_initializer;

  class log_frame
  {
    std::string m_name;
    int         m_level;
    const char* m_plog_name;

  public:
    log_frame(const std::string& name,  int dlevel = LOG_LEVEL_2 , const char* plog_name = NULL);
    ~log_frame();
  };

  inline int get_set_time_level(bool is_need_set, int time_log_level)
  {
    return log_singletone::get_set_time_level(is_need_set, time_log_level);
  }
  inline int get_set_log_detalisation_level(bool is_need_set, int log_level_to_set)
  {
    return log_singletone::get_set_log_detalisation_level(is_need_set, log_level_to_set);
  }
  inline std::string get_prefix_entry()
  {
    return log_singletone::get_prefix_entry();
  }
  inline bool get_set_need_thread_id(bool is_need_set, bool is_need_val)
  {
    return log_singletone::get_set_need_thread_id(is_need_set, is_need_val);
  }
  inline bool get_set_need_proc_name(bool is_need_set, bool is_need_val )
  {
    return log_singletone::get_set_need_proc_name(is_need_set, is_need_val);
  }

  inline std::string get_win32_err_descr(int err_no);
  inline bool getwin32_err_text(std::stringstream& ref_message, int error_no);
}

#if defined(_DEBUG) || defined(__GNUC__)
  #define  ENABLE_LOGGING_INTERNAL
#endif

#if defined(ENABLE_RELEASE_LOGGING)
  #define  ENABLE_LOGGING_INTERNAL
#endif


#if defined(ENABLE_LOGGING_INTERNAL)

#define LOG_PRINT_NO_PREFIX2(log_name, x, y) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << x << std::endl; epee::log_space::log_singletone::do_log_message(ss________.str() , y, epee::log_space::console_color_default, false, log_name);}}

#define LOG_PRINT_NO_PREFIX_NO_POSTFIX2(log_name, x, y) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << x; epee::log_space::log_singletone::do_log_message(ss________.str(), y, epee::log_space::console_color_default, false, log_name);}}


#define LOG_PRINT_NO_POSTFIX2(log_name, x, y) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << epee::log_space::log_singletone::get_prefix_entry() << x; epee::log_space::log_singletone::do_log_message(ss________.str(), y, epee::log_space::console_color_default, false, log_name);}}


#define LOG_PRINT2(log_name, x, y) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << epee::log_space::log_singletone::get_prefix_entry() << x << std::endl;epee::log_space::log_singletone::do_log_message(ss________.str(), y, epee::log_space::console_color_default, false, log_name);}}

#define LOG_PRINT_COLOR2(log_name, x, y, color) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << epee::log_space::log_singletone::get_prefix_entry() << x << std::endl;epee::log_space::log_singletone::do_log_message(ss________.str(), y, color, false, log_name);}}


#define LOG_PRINT2_JORNAL(log_name, x, y) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << epee::log_space::log_singletone::get_prefix_entry() << x << std::endl;epee::log_space::log_singletone::do_log_message(ss________.str(), y, epee::log_space::console_color_default, true, log_name);}}


#define LOG_ERROR2(log_name, x) { \
  std::stringstream ss________; ss________ << epee::log_space::log_singletone::get_prefix_entry() << "ERROR " << __FILE__ << ":" << __LINE__ << " " << x << std::endl; epee::log_space::log_singletone::do_log_message(ss________.str(), LOG_LEVEL_0, epee::log_space::console_color_red, true, log_name);LOCAL_ASSERT(0); epee::log_space::log_singletone::get_set_err_count(true, epee::log_space::log_singletone::get_set_err_count()+1);}

#define LOG_FRAME2(log_name, x, y) epee::log_space::log_frame frame(x, y, log_name)

#define LOG_WARNING2(log_name, x, y) {if ( y <= epee::log_space::log_singletone::get_log_detalisation_level() )\
  {std::stringstream ss________; ss________ << epee::log_space::log_singletone::get_prefix_entry() << "WARNING " << __FILE__ << ":" << __LINE__ << " " << x << std::endl; epee::log_space::log_singletone::do_log_message(ss________.str(), y, epee::log_space::console_color_red, true, log_name);LOCAL_ASSERT(0); epee::log_space::log_singletone::get_set_err_count(true, epee::log_space::log_singletone::get_set_err_count()+1);}}

#else


#define LOG_PRINT_NO_PREFIX2(log_name, x, y)

#define LOG_PRINT_NO_PREFIX_NO_POSTFIX2(log_name, x, y)

#define LOG_PRINT_NO_POSTFIX2(log_name, x, y)

#define LOG_PRINT_COLOR2(log_name, x, y, color)

#define LOG_PRINT2_JORNAL(log_name, x, y)

#define LOG_PRINT2(log_name, x, y)

#define LOG_ERROR2(log_name, x)


#define LOG_FRAME2(log_name, x, y)

#define LOG_WARNING2(log_name, x, level)


#endif


#ifndef LOG_DEFAULT_TARGET
  #define LOG_DEFAULT_TARGET    NULL
#endif


#define LOG_PRINT_NO_POSTFIX(mess, level) LOG_PRINT_NO_POSTFIX2(LOG_DEFAULT_TARGET, mess, level)
#define LOG_PRINT_NO_PREFIX(mess, level)  LOG_PRINT_NO_PREFIX2(LOG_DEFAULT_TARGET, mess, level)
#define LOG_PRINT_NO_PREFIX_NO_POSTFIX(mess, level) LOG_PRINT_NO_PREFIX_NO_POSTFIX2(LOG_DEFAULT_TARGET, mess, level)
#define LOG_PRINT(mess, level)        LOG_PRINT2(LOG_DEFAULT_TARGET, mess, level)

#define LOG_PRINT_COLOR(mess, level, color)       LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, color)
#define LOG_PRINT_RED(mess, level)        LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, epee::log_space::console_color_red)
#define LOG_PRINT_GREEN(mess, level)        LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, epee::log_space::console_color_green)
#define LOG_PRINT_BLUE(mess, level)       LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, epee::log_space::console_color_blue)
#define LOG_PRINT_YELLOW(mess, level)       LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, epee::log_space::console_color_yellow)
#define LOG_PRINT_CYAN(mess, level)       LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, epee::log_space::console_color_cyan)
#define LOG_PRINT_MAGENTA(mess, level)       LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, level, epee::log_space::console_color_magenta)

#define LOG_PRINT_RED_L0(mess)    LOG_PRINT_COLOR2(LOG_DEFAULT_TARGET, mess, LOG_LEVEL_0, epee::log_space::console_color_red)

#define LOG_PRINT_L0(mess)        LOG_PRINT(mess, LOG_LEVEL_0)
#define LOG_PRINT_L1(mess)        LOG_PRINT(mess, LOG_LEVEL_1)
#define LOG_PRINT_L2(mess)        LOG_PRINT(mess, LOG_LEVEL_2)
#define LOG_PRINT_L3(mess)        LOG_PRINT(mess, LOG_LEVEL_3)
#define LOG_PRINT_L4(mess)        LOG_PRINT(mess, LOG_LEVEL_4)
#define LOG_PRINT_J(mess, level)        LOG_PRINT2_JORNAL(LOG_DEFAULT_TARGET, mess, level)

#define LOG_ERROR(mess)           LOG_ERROR2(LOG_DEFAULT_TARGET, mess)
#define LOG_FRAME(mess, level)        LOG_FRAME2(LOG_DEFAULT_TARGET, mess, level)
#define LOG_VALUE(mess, level)        LOG_VALUE2(LOG_DEFAULT_TARGET, mess, level)
#define LOG_ARRAY(mess, level)        LOG_ARRAY2(LOG_DEFAULT_TARGET, mess, level)
//#define LOGWIN_PLATFORM_ERROR(err_no)       LOGWINDWOS_PLATFORM_ERROR2(LOG_DEFAULT_TARGET, err_no)
#define LOG_SOCKET_ERROR(err_no)      LOG_SOCKET_ERROR2(LOG_DEFAULT_TARGET, err_no)
//#define LOGWIN_PLATFORM_ERROR_UNCRITICAL(mess)  LOGWINDWOS_PLATFORM_ERROR_UNCRITICAL2(LOG_DEFAULT_TARGET, mess)
#define LOG_WARNING(mess, level)      LOG_WARNING2(LOG_DEFAULT_TARGET, mess, level)

#define ENDL std::endl

#define TRY_ENTRY()   try {
#define CATCH_ENTRY(location, return_val) } \
  catch(const std::exception& ex) \
{ \
  (void)(ex); \
  LOG_ERROR("Exception at [" << location << "], what=" << ex.what()); \
  return return_val; \
}\
  catch(...)\
{\
  LOG_ERROR("Exception at [" << location << "], generic exception \"...\"");\
  return return_val; \
}

#define CATCH_ENTRY_L0(lacation, return_val) CATCH_ENTRY(lacation, return_val)
#define CATCH_ENTRY_L1(lacation, return_val) CATCH_ENTRY(lacation, return_val)
#define CATCH_ENTRY_L2(lacation, return_val) CATCH_ENTRY(lacation, return_val)
#define CATCH_ENTRY_L3(lacation, return_val) CATCH_ENTRY(lacation, return_val)
#define CATCH_ENTRY_L4(lacation, return_val) CATCH_ENTRY(lacation, return_val)


#define ASSERT_MES_AND_THROW(message) {LOG_ERROR(message); std::stringstream ss; ss << message; throw std::runtime_error(ss.str());}
#define CHECK_AND_ASSERT_THROW_MES(expr, message) {if(!(expr)) ASSERT_MES_AND_THROW(message);}


#ifndef CHECK_AND_ASSERT
#define CHECK_AND_ASSERT(expr, fail_ret_val)   do{if(!(expr)){LOCAL_ASSERT(expr); return fail_ret_val;};}while(0)
#endif

#define NOTHING

#ifndef CHECK_AND_ASSERT_MES
#define CHECK_AND_ASSERT_MES(expr, fail_ret_val, message)   do{if(!(expr)) {LOG_ERROR(message); return fail_ret_val;};}while(0)
#endif

#ifndef CHECK_AND_NO_ASSERT_MES
#define CHECK_AND_NO_ASSERT_MES(expr, fail_ret_val, message)   do{if(!(expr)) {LOG_PRINT_L0(message); /*LOCAL_ASSERT(expr);*/ return fail_ret_val;};}while(0)
#endif


#ifndef CHECK_AND_ASSERT_MES_NO_RET
#define CHECK_AND_ASSERT_MES_NO_RET(expr, message)   do{if(!(expr)) {LOG_ERROR(message); return;};}while(0)
#endif


#ifndef CHECK_AND_ASSERT_MES2
#define CHECK_AND_ASSERT_MES2(expr, message)   do{if(!(expr)) {LOG_ERROR(message); };}while(0)
#endif

}
#endif //_MISC_LOG_EX_H_
