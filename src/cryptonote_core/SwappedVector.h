// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <vector>
//#include <boost/archive/binary_iarchive.hpp>
//#include <boost/archive/binary_oarchive.hpp>
#include "serialization/binary_archive.h"

template<class T> class SwappedVector {
public:
  SwappedVector();
  //SwappedVector(const SwappedVector&) = delete;
  ~SwappedVector();
  //SwappedVector& operator=(const SwappedVector&) = delete;

  bool open(const std::string& itemFileName, const std::string& indexFileName, size_t poolSize);
  void close();

  bool empty() const;
  uint64_t size() const;
  const T& operator[](uint64_t index);
  const T& front();
  const T& back();
  void clear();
  void pop_back();
  void push_back(const T& item);

private:
  struct ItemEntry;
  struct CacheEntry;

  struct ItemEntry {
  public:
    T item;
    typename std::list<CacheEntry>::iterator cacheIter;
  };

  struct CacheEntry {
  public:
    typename std::map<uint64_t, ItemEntry>::iterator itemIter;
  };

  std::fstream m_itemsFile;
  std::fstream m_indexesFile;
  size_t m_poolSize;
  std::vector<uint64_t> m_offsets;
  uint64_t m_itemsFileSize;
  std::map<uint64_t, ItemEntry> m_items;
  std::list<CacheEntry> m_cache;
  uint64_t m_cacheHits;
  uint64_t m_cacheMisses;

  T* prepare(uint64_t index);
};

template<class T> SwappedVector<T>::SwappedVector() {
}

template<class T> SwappedVector<T>::~SwappedVector() {
  close();
}

template<class T> bool SwappedVector<T>::open(const std::string& itemFileName, const std::string& indexFileName, size_t poolSize) {
  if (poolSize == 0) {
    return false;
  }

  m_itemsFile.open(itemFileName, std::ios::in | std::ios::out | std::ios::binary);
  m_indexesFile.open(indexFileName, std::ios::in | std::ios::out | std::ios::binary);
  if (m_itemsFile && m_indexesFile) {
    uint64_t count;
    m_indexesFile.read(reinterpret_cast<char*>(&count), sizeof count);
    if (!m_indexesFile) {
      return false;
    }

    std::vector<uint64_t> offsets;
    uint64_t itemsFileSize = 0;
    for (uint64_t i = 0; i < count; ++i) {
      uint32_t itemSize;
      m_indexesFile.read(reinterpret_cast<char*>(&itemSize), sizeof itemSize);
      if (!m_indexesFile) {
        return false;
      }

      offsets.emplace_back(itemsFileSize);
      itemsFileSize += itemSize;
    }

    m_offsets.swap(offsets);
    m_itemsFileSize = itemsFileSize;
  } else {
    m_itemsFile.open(itemFileName, std::ios::out | std::ios::binary);
    m_itemsFile.close();
    m_itemsFile.open(itemFileName, std::ios::in | std::ios::out | std::ios::binary);
    m_indexesFile.open(indexFileName, std::ios::out | std::ios::binary);
    uint64_t count = 0;
    m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
    if (!m_indexesFile) {
      return false;
    }

    m_indexesFile.close();
    m_indexesFile.open(indexFileName, std::ios::in | std::ios::out | std::ios::binary);
    m_offsets.clear();
    m_itemsFileSize = 0;
  }

  m_poolSize = poolSize;
  m_items.clear();
  m_cache.clear();
  m_cacheHits = 0;
  m_cacheMisses = 0;
  return true;
}

template<class T> void SwappedVector<T>::close() {
  std::cout << "SwappedVector cache hits: " << m_cacheHits << ", misses: " << m_cacheMisses << " (" << std::fixed << std::setprecision(2) << static_cast<double>(m_cacheMisses) / (m_cacheHits + m_cacheMisses) * 100 << "%)" << std::endl;
}

template<class T> bool SwappedVector<T>::empty() const {
  return m_offsets.empty();
}

template<class T> uint64_t SwappedVector<T>::size() const {
  return m_offsets.size();
}

template<class T> const T& SwappedVector<T>::operator[](uint64_t index) {
  auto itemIter = m_items.find(index);
  if (itemIter != m_items.end()) {
    if (itemIter->second.cacheIter != --m_cache.end()) {
      m_cache.splice(m_cache.end(), m_cache, itemIter->second.cacheIter);
    }

    ++m_cacheHits;
    return itemIter->second.item;
  }

  if (index >= m_offsets.size()) {
    throw std::runtime_error("SwappedVector::operator[]");
  }

  if (!m_itemsFile) {
    throw std::runtime_error("SwappedVector::operator[]");
  }

  m_itemsFile.seekg(m_offsets[index]);
  T tempItem;
  //try {
    //boost::archive::binary_iarchive archive(m_itemsFile);
    //archive & tempItem;
  //} catch (std::exception&) {
  //  throw std::runtime_error("SwappedVector::operator[]");
  //}

  binary_archive<false> archive(m_itemsFile);
  if (!do_serialize(archive, tempItem)) {
    throw std::runtime_error("SwappedVector::operator[]");
  }

  T* item = prepare(index);
  std::swap(tempItem, *item);
  ++m_cacheMisses;
  return *item;
}

template<class T> const T& SwappedVector<T>::front() {
  return operator[](0);
}

template<class T> const T& SwappedVector<T>::back() {
  return operator[](m_offsets.size() - 1);
}

template<class T> void SwappedVector<T>::clear() {
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedVector::clear");
  }

  m_indexesFile.seekp(0);
  uint64_t count = 0;
  m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedVector::clear");
  }

  m_offsets.clear();
  m_itemsFileSize = 0;
  m_items.clear();
  m_cache.clear();
}

template<class T> void SwappedVector<T>::pop_back() {
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedVector::pop_back");
  }

  m_indexesFile.seekp(0);
  uint64_t count = m_offsets.size() - 1;
  m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
  if (!m_indexesFile) {
    throw std::runtime_error("SwappedVector::pop_back");
  }

  m_itemsFileSize = m_offsets.back();
  m_offsets.pop_back();
  auto itemIter = m_items.find(m_offsets.size());
  if (itemIter != m_items.end()) {
    m_cache.erase(itemIter->second.cacheIter);
    m_items.erase(itemIter);
  }
}

template<class T> void SwappedVector<T>::push_back(const T& item) {
  uint64_t itemsFileSize;

  {
    if (!m_itemsFile) {
      throw std::runtime_error("SwappedVector::push_back");
    }

    m_itemsFile.seekp(m_itemsFileSize);
    //try {
    //  boost::archive::binary_oarchive archive(m_itemsFile);
    //  archive & item;
    //} catch (std::exception&) {
    //  throw std::runtime_error("SwappedVector::push_back");
    //}

    binary_archive<true> archive(m_itemsFile);
    if (!do_serialize(archive, *const_cast<T*>(&item))) {
      throw std::runtime_error("SwappedVector::push_back");
    }

    itemsFileSize = m_itemsFile.tellp();
  }

  {
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedVector::push_back");
    }

    m_indexesFile.seekp(sizeof(uint64_t) + sizeof(uint32_t) * m_offsets.size());
    uint32_t itemSize = static_cast<uint32_t>(itemsFileSize - m_itemsFileSize);
    m_indexesFile.write(reinterpret_cast<char*>(&itemSize), sizeof itemSize);
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedVector::push_back");
    }

    m_indexesFile.seekp(0);
    uint64_t count = m_offsets.size() + 1;
    m_indexesFile.write(reinterpret_cast<char*>(&count), sizeof count);
    if (!m_indexesFile) {
      throw std::runtime_error("SwappedVector::push_back");
    }
  }

  m_offsets.push_back(m_itemsFileSize);
  m_itemsFileSize = itemsFileSize;

  T* newItem = prepare(m_offsets.size() - 1);
  *newItem = item;
}

template<class T> T* SwappedVector<T>::prepare(uint64_t index) {
  if (m_items.size() == m_poolSize) {
    auto cacheIter = m_cache.begin();
    m_items.erase(cacheIter->itemIter);
    m_cache.erase(cacheIter);
  }

  auto itemIter = m_items.insert(std::make_pair(index, ItemEntry()));
  CacheEntry cacheEntry = { itemIter.first };
  auto cacheIter = m_cache.insert(m_cache.end(), cacheEntry);
  itemIter.first->second.cacheIter = cacheIter;
  return &itemIter.first->second.item;
}
