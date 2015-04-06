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

#include <map>
#include <vector>
#include <string>

#include <stdint.h>

namespace cryptonote {

class JsonValue {
public:
  typedef std::vector<JsonValue> Array;
  typedef std::map<std::string, JsonValue> Object;

  enum Type {
    ARRAY,
    BOOL,
    INT64,
    NIL,
    OBJECT,
    DOUBLE,
    STRING
  };

  JsonValue();
  JsonValue(Type type);
  JsonValue(const JsonValue& other);
  ~JsonValue();
  JsonValue& operator=(const JsonValue& other) = delete;
  bool isArray() const;
  bool isBool() const;
  bool isInt64() const;
  bool isNil() const;
  bool isObject() const;
  bool isDouble() const;
  bool isString() const;
  bool getBool() const;
  int64_t getNumber() const;
  const Object& getObject() const;
  double getDouble() const;
  std::string getString() const;
  const JsonValue& operator()(const std::string& name) const;
  size_t count(const std::string& name) const;
  const JsonValue& operator[](size_t index) const;
  size_t size() const;
  Array::const_iterator begin() const;
  Array::const_iterator end() const;

  JsonValue& pushBack(const JsonValue& val);
  JsonValue& insert(const std::string& key, const JsonValue& value);

  JsonValue& operator=(bool value);
  JsonValue& operator=(int64_t value);
//  JsonValue& operator=(NilType value);
  JsonValue& operator=(double value);
  JsonValue& operator=(const std::string& value);
  JsonValue& operator=(const char* value);


  friend std::istream& operator>>(std::istream& in, JsonValue& jsonValue);
  friend std::ostream& operator<<(std::ostream& out, const JsonValue& jsonValue);

private:
  size_t d_type;
  union {
    uint8_t d_valueArray[sizeof(Array)];
    bool d_valueBool;
    int64_t d_valueInt64;
    uint8_t d_valueObject[sizeof(Object)];
    double d_valueDouble;
    uint8_t d_valueString[sizeof(std::string)];
  };

  void destructValue();

  void readArray(std::istream& in);
  void readTrue(std::istream& in);
  void readFalse(std::istream& in);
  void readNumber(std::istream& in, char c);
  void readNull(std::istream& in);
  void readObject(std::istream& in);
  void readString(std::istream& in);
};

} //namespace cryptonote
