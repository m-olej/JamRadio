// json.cpp
#include "json.hpp"

// Default constructor
Json::Json() : type(Type::Null) {}

// Boolean constructor
Json::Json(bool value) : type(Type::Boolean), boolValue(value) {}

// Number constructor
Json::Json(int value) : type(Type::Number), numberValue(value) {}

// String constructor
Json::Json(const std::string &value) : type(Type::String), stringValue(value) {}

// Object constructor
Json::Json(const Object &value) : type(Type::Object), objectValue(value) {}

// Array constructor
Json::Json(const Array &value) : type(Type::Array), arrayValue(value) {}

// Type getter
Json::Type Json::getType() const { return type; }

// Object accessor
const Json::Object &Json::asObject() const {
  if (type != Type::Object) {
    throw std::runtime_error("Not a JSON object");
  }
  return objectValue;
}

// Array accessor
const Json::Array &Json::asArray() const {
  if (type != Type::Array) {
    throw std::runtime_error("Not a JSON array");
  }
  return arrayValue;
}

// String accessor
const std::string &Json::asString() const {
  if (type != Type::String) {
    throw std::runtime_error("Not a JSON string");
  }
  return stringValue;
}

// Number accessor
int Json::asNumber() const {
  if (type != Type::Number) {
    throw std::runtime_error("Not a JSON number");
  }
  return numberValue;
}

// Boolean accessor
bool Json::asBoolean() const {
  if (type != Type::Boolean) {
    throw std::runtime_error("Not a JSON boolean");
  }
  return boolValue;
}

// Indexing operator for objects (non-const)
Json &Json::operator[](const std::string &key) {
  if (type != Type::Object) {
    if (type == Type::Null) {
      type = Type::Object;
      objectValue = Object();
    } else {
      throw std::runtime_error("Not a JSON object");
    }
  }
  return objectValue[key];
}

// Indexing operator for objects (const)
const Json &Json::operator[](const std::string &key) const {
  if (type != Type::Object) {
    throw std::runtime_error("Not a JSON object");
  }
  auto it = objectValue.find(key);
  if (it == objectValue.end()) {
    throw std::runtime_error("Key not found");
  }
  return it->second;
}

// Overload assignment operator for boolean
Json &Json::operator=(bool value) {
  type = Type::Boolean;
  boolValue = value;
  return *this;
}

// Overload assignment operator for number
Json &Json::operator=(int value) {
  type = Type::Number;
  numberValue = value;
  return *this;
}

// Overload assignment operator for string
Json &Json::operator=(const std::string &value) {
  type = Type::String;
  stringValue = value;
  return *this;
}

// Overload assignment operator for object
Json &Json::operator=(const Object &value) {
  type = Type::Object;
  objectValue = value;
  return *this;
}

// Overload assignment operator for array
Json &Json::operator=(const Array &value) {
  type = Type::Array;
  arrayValue = value;
  return *this;
}
// Serialization
std::string Json::toString() const {
  switch (type) {
  case Type::Null:
    return "null";
  case Type::Boolean:
    return boolValue ? "true" : "false";
  case Type::Number:
    return std::to_string(numberValue);
  case Type::String:
    return '"' + stringValue + '"';
  case Type::Object: {
    std::ostringstream oss;
    oss << "{";
    for (auto it = objectValue.begin(); it != objectValue.end(); ++it) {
      if (it != objectValue.begin()) {
        oss << ",";
      }
      oss << '"' << it->first << "\" : " << it->second.toString();
    }
    oss << "}";
    return oss.str();
  }
  case Type::Array: {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < arrayValue.size(); ++i) {
      if (i > 0) {
        oss << ",";
      }
      oss << arrayValue[i].toString();
    }
    oss << "]";
    return oss.str();
  }
  }
  throw std::runtime_error("Invalid JSON type");
}
