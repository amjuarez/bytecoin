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

#include "JsonValue.h"
#include <iomanip>
#include <sstream>
#include <assert.h>

namespace cryptonote {

JsonValue::JsonValue() : d_type(NIL) {
}

JsonValue::JsonValue(JsonValue::Type type) {
  switch(type) {
  case OBJECT:
    new(d_valueObject)JsonValue::Object();
    break;
  case ARRAY:
    new(d_valueArray)JsonValue::Array();
    break;
  default:
    throw std::runtime_error("Wrong JsonValue type. Object or Array are possible only");
  }

  d_type = type;
}

JsonValue::JsonValue(const JsonValue& other) : d_type(other.d_type) {
  switch (d_type) {
  case ARRAY:
    new(d_valueArray)JsonValue::Array(*reinterpret_cast<const Array*>(other.d_valueArray));
    break;
  case BOOL:
    d_valueBool = other.d_valueBool;
    break;
  case INT64:
    d_valueInt64 = other.d_valueInt64;
    break;
  case NIL:
    break;
  case OBJECT:
    new(d_valueObject)JsonValue::Object(*reinterpret_cast<const Object*>(other.d_valueObject));
    break;
  case DOUBLE:
    d_valueDouble = other.d_valueDouble;
    break;
  case STRING:
    new(d_valueString)std::string(*reinterpret_cast<const std::string*>(other.d_valueString));
    break;
  default:
    throw(std::runtime_error("Invalid type"));
  }
}

JsonValue::~JsonValue() {
  destructValue();
}

bool JsonValue::isArray() const {
  return d_type == ARRAY;
}

bool JsonValue::isBool() const {
  return d_type == BOOL;
}

bool JsonValue::isInt64() const {
  return d_type == INT64;
}

bool JsonValue::isNil() const {
  return d_type == NIL;
}

bool JsonValue::isObject() const {
  return d_type == OBJECT;
}

bool JsonValue::isDouble() const {
  return d_type == DOUBLE;
}

bool JsonValue::isString() const {
  return d_type == STRING;
}

bool JsonValue::getBool() const {
  assert(d_type == BOOL);
  if (d_type != BOOL) {
    throw(std::runtime_error("Value type is not BOOL"));
  }

  return d_valueBool;
}

int64_t JsonValue::getNumber() const {
  assert(d_type == INT64);
  if (d_type != INT64) {
    throw(std::runtime_error("Value type is not INT64"));
  }

  return d_valueInt64;
}

const JsonValue::Object& JsonValue::getObject() const {
  assert(d_type == OBJECT);
  if (d_type != OBJECT) {
    throw(std::runtime_error("Value type is not OBJECT"));
  }

  return *reinterpret_cast<const Object*>(d_valueObject);
}

double JsonValue::getDouble() const {
  assert(d_type == DOUBLE);
  if (d_type != DOUBLE) {
    throw(std::runtime_error("Value type is not DOUBLE"));
  }

  return d_valueDouble;
}

std::string JsonValue::getString() const {
  assert(d_type == STRING);
  if (d_type != STRING) {
    throw(std::runtime_error("Value type is not STRING"));
  }

  return *reinterpret_cast<const std::string*>(d_valueString);
}

const JsonValue& JsonValue::operator()(const std::string& name) const {
  assert(d_type == OBJECT);
  assert(reinterpret_cast<const Object*>(d_valueObject)->count(name) > 0);
  if (d_type != OBJECT) {
    throw(std::runtime_error("Value type is not OBJECT"));
  }

  return reinterpret_cast<const Object*>(d_valueObject)->at(name);
}

size_t JsonValue::count(const std::string& name) const {
  assert(d_type == OBJECT);
  if (d_type != OBJECT) {
    throw(std::runtime_error("Value type is not OBJECT"));
  }

  return reinterpret_cast<const Object*>(d_valueObject)->count(name);
}

const JsonValue& JsonValue::operator[](size_t index) const {
  assert(d_type == ARRAY);
  if (d_type != ARRAY) {
    throw(std::runtime_error("Value type is not ARRAY"));
  }

  return reinterpret_cast<const Array*>(d_valueArray)->at(index);
}

size_t JsonValue::size() const {
  assert(d_type == ARRAY || d_type == OBJECT);
  switch (d_type) {
  case OBJECT:
    return reinterpret_cast<const Object*>(d_valueString)->size();
  case ARRAY:
    return reinterpret_cast<const Array*>(d_valueString)->size();
  default:
    throw(std::runtime_error("Value type is not ARRAY or OBJECT"));
  }
}

JsonValue::Array::const_iterator JsonValue::begin() const {
  assert(d_type == ARRAY);
  if (d_type != ARRAY) {
    throw(std::runtime_error("Value type is not ARRAY"));
  }

  return reinterpret_cast<const Array*>(d_valueArray)->begin();
}

JsonValue::Array::const_iterator JsonValue::end() const {
  assert(d_type == ARRAY);
  if (d_type != ARRAY) {
    throw(std::runtime_error("Value type is not ARRAY"));
  }

  return reinterpret_cast<const Array*>(d_valueArray)->end();
}

void JsonValue::readArray(std::istream& in) {
  char c;
  JsonValue::Array value;  

  c = in.peek();
  while (true) {
    if (!isspace(in.peek())) break;
    in.read(&c, 1);
  }

  if (in.peek() != ']') {
    for (;;) {
      value.resize(value.size() + 1);
      in >> value.back();
      in >> c;
      while (isspace(c)) in >> c;
      if (c == ']') {
        break;
      }

      if (c != ',') {
        throw(std::runtime_error("Unable to parse"));
      }
    }
  } else {
    in.read(&c, 1);
  }

  if (d_type != JsonValue::ARRAY) {
    destructValue();
    d_type = JsonValue::NIL;
    new(d_valueArray)JsonValue::Array;
    d_type = JsonValue::ARRAY;
  }

  reinterpret_cast<JsonValue::Array*>(d_valueArray)->swap(value);
}

void JsonValue::readTrue(std::istream& in) {
  char data[3];
  in.read(data, 3);
  if (data[0] != 'r' || data[1] != 'u' || data[2] != 'e') {
    throw(std::runtime_error("Unable to parse"));
  }

  if (d_type != JsonValue::BOOL) {
    destructValue();
    d_type = JsonValue::BOOL;
  }

  d_valueBool = true;
}

void JsonValue::readFalse(std::istream& in) {
  char data[4];
  in.read(data, 4);
  if (data[0] != 'a' || data[1] != 'l' || data[2] != 's' || data[3] != 'e') {
    throw(std::runtime_error("Unable to parse"));
  }

  if (d_type != JsonValue::BOOL) {
    destructValue();
    d_type = JsonValue::BOOL;
  }

  d_valueBool = false;
}

void JsonValue::readNumber(std::istream& in, char c) {
  std::string text;
  text += c;
  size_t dots = 0;
  for (;;) {
    int i = in.peek();
    if (i >= '0' && i <= '9') {
      in.read(&c, 1);
      text += c;
    } else  if (i == '.') {
      in.read(&c, 1);
      text += '.';
      ++dots;
    } else {
      break;
    }
  }

  if (dots > 0) {
    if (dots > 1) {
      throw(std::runtime_error("Unable to parse"));
    }

    int i = in.peek();
    if (in.peek() == 'e') {
      in.read(&c, 1);
      text += c;
      i = in.peek();
      if (i == '+') {
        in.read(&c, 1);
        text += c;
        i = in.peek();
      } else if (i == '-') {
        in.read(&c, 1);
        text += c;
        i = in.peek();
      }

      if (i < '0' || i > '9') {
        throw(std::runtime_error("Unable to parse"));
      }

      do {
        in.read(&c, 1);
        text += c;
        i = in.peek();
      } while (i >= '0' && i <= '9');
    }

    double value;
    std::istringstream(text) >> value;
    if (d_type != JsonValue::DOUBLE) {
      destructValue();
      d_type = JsonValue::DOUBLE;
    }

    d_valueDouble = value;
  } else {
    if (text.size() > 1 && ((text[0] == '0') || (text[0] == '-' && text[1] == '0'))) {
      throw(std::runtime_error("Unable to parse"));
    }

    int64_t value;
    std::istringstream(text) >> value;
    if (d_type != JsonValue::INT64) {
      destructValue();
      d_type = JsonValue::INT64;
    }

    d_valueInt64 = value;
  }
}

void JsonValue::readNull(std::istream& in) {
  char data[3];
  in.read(data, 3);
  if (data[0] != 'u' || data[1] != 'l' || data[2] != 'l') {
    throw(std::runtime_error("Unable to parse"));
  }

  if (d_type != JsonValue::NIL) {
    destructValue();
    d_type = JsonValue::NIL;
  }
}

void JsonValue::readObject(std::istream& in) {
  char c;
  JsonValue::Object value;
  in >> c;
  while (isspace(c)) in >> c;

  if (c != '}') {
    std::string name;
    for (;;) {
      if (c != '"') {
        throw(std::runtime_error("Unable to parse"));
      }

      name.clear();
      for (;;) {
        in >> c;
        if (c == '"') {
          break;
        }

        if (c == '\\') {
          name += c;
          in >> c;
        }

        name += c;
      }

      in >> c;
      while (isspace(c)) in >> c;
      if (c != ':') {
        throw(std::runtime_error("Unable to parse"));
      }

      in >> value[name];
      in >> c;
      while (isspace(c)) in >> c;
      if (c == '}') {
        break;
      }

      if (c != ',') {
        throw(std::runtime_error("Unable to parse"));
      }
      in >> c;
      while (isspace(c)) in >> c;
    }
  }

  if (d_type != JsonValue::OBJECT) {
    destructValue();
    d_type = JsonValue::NIL;
    new(d_valueObject)JsonValue::Object;
    d_type = JsonValue::OBJECT;
  }

  reinterpret_cast<JsonValue::Object*>(d_valueObject)->swap(value);
}

void JsonValue::readString(std::istream& in) {
  char c;
  std::string value;

  for (;;) {
    in.read(&c, 1);
    if (c == '"') {
      break;
    }

    if (c == '\\') {
      value += c;
      in >> c;
    }

    value += c;
  }

  if (d_type != JsonValue::STRING) {
    destructValue();
    d_type = JsonValue::NIL;
    new(d_valueString)std::string;
    d_type = JsonValue::STRING;
  }

  reinterpret_cast<std::string*>(d_valueString)->swap(value);
}

std::istream& operator>>(std::istream& in, JsonValue& jsonValue) {
  char c;
  in >> c;
  while (isspace(c)) in >> c;
  if (c == '[') {
    jsonValue.readArray(in);
  } else if (c == 't') {
    jsonValue.readTrue(in);
  } else if (c == 'f') {
    jsonValue.readFalse(in);
  } else if ((c == '-') || (c >= '0' && c <= '9')) {
    jsonValue.readNumber(in, c);
  } else if (c == 'n') {
    jsonValue.readNull(in);
  } else if (c == '{') {
    jsonValue.readObject(in);
  } else if (c == '"') {
    jsonValue.readString(in);
  } else {
    throw(std::runtime_error("Unable to parse"));
  }

  return in;
}

std::ostream& operator<<(std::ostream& out, const JsonValue& jsonValue) {
  if (jsonValue.d_type == JsonValue::ARRAY) {
    const JsonValue::Array& array = *reinterpret_cast<const JsonValue::Array*>(jsonValue.d_valueArray);
    out << '[';
    if (array.size() > 0) {
      out << array[0];
      for (size_t i = 1; i < array.size(); ++i) {
        out << ',' << array[i];
      }
    }

    out << ']';
  } else if (jsonValue.d_type == JsonValue::BOOL) {
    out << (jsonValue.d_valueBool ? "true" : "false");
  } else if (jsonValue.d_type == JsonValue::INT64) {
    out << jsonValue.d_valueInt64;
  } else if (jsonValue.d_type == JsonValue::NIL) {
    out << "null";
  } else if (jsonValue.d_type == JsonValue::OBJECT) {
    const JsonValue::Object& object = *reinterpret_cast<const JsonValue::Object*>(jsonValue.d_valueObject);
    out << '{';
    auto iter = object.begin();
    if (iter != object.end()) {
      out << '"' << iter->first << "\":" << iter->second;
      ++iter;
      for (; iter != object.end(); ++iter) {
        out << ",\"" << iter->first << "\":" << iter->second;
      }
    }

    out << '}';
  } else if (jsonValue.d_type == JsonValue::DOUBLE) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(11) << jsonValue.d_valueDouble;
    std::string value = stream.str();
    while (value.size() > 1 && value[value.size() - 2] != '.' && value[value.size() - 1] == '0') {
      value.resize(value.size() - 1);
    }

    out << value;
  } else if (jsonValue.d_type == JsonValue::STRING) {
    out << '"' << *reinterpret_cast<const std::string*>(jsonValue.d_valueString) << '"';
  } else {
    throw(std::runtime_error("Invalid type"));
  }

  return out;
}

void JsonValue::destructValue() {
  switch (d_type) {
  case ARRAY:
    reinterpret_cast<Array*>(d_valueArray)->~Array();
    break;
  case OBJECT:
    reinterpret_cast<Object*>(d_valueObject)->~Object();
    break;
  case STRING:
    reinterpret_cast<std::string*>(d_valueString)->~basic_string();
    break;
  default:
    break;
  }
}

JsonValue& JsonValue::pushBack(const JsonValue& val) {
  if (d_type != ARRAY) {
    throw std::runtime_error("JsonValue error. pushBack is only possible for arrays");
  }

  Array* array = reinterpret_cast<Array*>(d_valueArray);
  array->push_back(val);

  return array->back();
}

JsonValue& JsonValue::insert(const std::string& key, const JsonValue& value) {
  if (d_type != OBJECT) {
    throw std::runtime_error("JsonValue error. insert is only possible for objects");
  }

  Object* obj = reinterpret_cast<Object*>(d_valueObject);

  auto res = obj->insert(std::make_pair(key, value));
  return res.first->second;
}

JsonValue& JsonValue::operator=(bool value) {
  if (d_type != BOOL) {
    destructValue();
    d_type = BOOL;
  }

  d_valueBool = value;

  return *this;
}

JsonValue& JsonValue::operator=(int64_t value) {
  if (d_type != INT64) {
    destructValue();
    d_type = INT64;
  }

  d_valueInt64 = value;

  return *this;
}

//JsonValue& JsonValue::operator=(NilType value) {
//  if (d_type != NIL) {
//    destructValue();
//    d_type = NIL;
//  }
//}

JsonValue& JsonValue::operator=(double value) {
  if (d_type != DOUBLE) {
    destructValue();
    d_type = DOUBLE;
  }

  d_valueDouble = value;

  return *this;
}

JsonValue& JsonValue::operator=(const std::string& value) {
  if (d_type != STRING) {
    destructValue();
    new(d_valueString)std::string;
    d_type = STRING;
  }

  reinterpret_cast<std::string*>(d_valueString)->assign(value.data(), value.size());

  return *this;
}

JsonValue& JsonValue::operator=(const char* value) {
  return operator=(std::string(value));
}

} //namespace cryptonote
