// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "PathTools.h"
#include <algorithm>

namespace {

const char GENERIC_PATH_SEPARATOR = '/';

#ifdef _WIN32
const char NATIVE_PATH_SEPARATOR = '\\';
#else
const char NATIVE_PATH_SEPARATOR = '/';
#endif


std::string::size_type findExtensionPosition(const std::string& filename) {
  auto pos = filename.rfind('.');
  
  if (pos != std::string::npos) {
    auto slashPos = filename.rfind(GENERIC_PATH_SEPARATOR);
    if (slashPos != std::string::npos && slashPos > pos) {
      return std::string::npos;
    }
  }

  return pos;
}

} // anonymous namespace

namespace Common {

std::string NativePathToGeneric(const std::string& nativePath) {
  if (GENERIC_PATH_SEPARATOR == NATIVE_PATH_SEPARATOR) {
    return nativePath;
  }
  std::string genericPath(nativePath);
  std::replace(genericPath.begin(), genericPath.end(), NATIVE_PATH_SEPARATOR, GENERIC_PATH_SEPARATOR);
  return genericPath;
}

std::string GetPathDirectory(const std::string& path) {
  auto slashPos = path.rfind(GENERIC_PATH_SEPARATOR);
  if (slashPos == std::string::npos) {
    return std::string();
  }
  return path.substr(0, slashPos);
}

std::string GetPathFilename(const std::string& path) {
  auto slashPos = path.rfind(GENERIC_PATH_SEPARATOR);
  if (slashPos == std::string::npos) {
    return path;
  }
  return path.substr(slashPos + 1);
}

void SplitPath(const std::string& path, std::string& directory, std::string& filename) {
  directory = GetPathDirectory(path);
  filename = GetPathFilename(path);
}

std::string CombinePath(const std::string& path1, const std::string& path2) {
  return path1 + GENERIC_PATH_SEPARATOR + path2;
}

std::string ReplaceExtenstion(const std::string& path, const std::string& extension) {
  return RemoveExtension(path) + extension;
}

std::string GetExtension(const std::string& path) {
  auto pos = findExtensionPosition(path);
  if (pos != std::string::npos) {
    return path.substr(pos);
  }
  return std::string();
}

std::string RemoveExtension(const std::string& filename) { 
  auto pos = findExtensionPosition(filename);

  if (pos == std::string::npos) {
    return filename;
  }

  return filename.substr(0, pos);
}


bool HasParentPath(const std::string& path) {
  return path.find(GENERIC_PATH_SEPARATOR) != std::string::npos;
}


}
