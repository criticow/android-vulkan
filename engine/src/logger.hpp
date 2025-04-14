#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <iostream>
#endif

#include <sstream>
#include <string>

/*
  TODO: implement different levels
  TODO: implement logging to file
*/

namespace logger
{
  enum class LOGGER_LEVEL
  {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
  };

  template<typename T>
  void replacePlaceholders(std::string &buffer, size_t index, const T &value)
  {
    std::ostringstream sstream;

    if(std::is_same_v<T, bool>)
    {
      sstream << std::boolalpha << value;
    }
    else
    {
      sstream << value;
    }

    std::string pattern = "{}";
    size_t pos = buffer.find(pattern);

    if(pos != std::string::npos)
    {
      buffer.replace(pos, pattern.length(), sstream.str());
    }

    pattern = "{" + std::to_string(index) + "}";
    pos = buffer.find(pattern);

    while(pos != std::string::npos)
    {
      buffer.replace(pos, pattern.length(), sstream.str());
      pos = buffer.find(pattern);
    }
  }


  template<typename T>
  std::string prepareBuffer(const T &fmt)
  {
    return std::string(fmt);
  }

  template<typename T, typename... Args>
  std::string prepareBuffer(const T &fmt, Args&&... args)
  {
    std::string buffer(fmt);
    size_t index = 0;

    (..., replacePlaceholders(buffer, index, args));

    return buffer;
  }



  template<typename... Args>
  void print(LOGGER_LEVEL level, Args&&... args)
  {
    std::string buffer = prepareBuffer(std::forward<Args>(args)...);

    #ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_DEBUG, "NDK_ENGINE", "%s", buffer.c_str());
    #else
    std::cout << buffer << '\n';
    #endif
  }

  template<typename... Args>
  void debug(Args&&... args)
  {
    print(LOGGER_LEVEL::LOG_DEBUG, std::forward<Args>(args)...);
  }
};

#define LOG_DEBUG(...) logger::debug(__VA_ARGS__)