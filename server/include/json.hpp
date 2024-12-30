// json.h
#ifndef JSON_HPP
#define JSON_HPP

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class Json {
public:
  using Object = std::map<std::string, Json>;
  using Array = std::vector<Json>;

  enum class Type { Null, Boolean, Number, String, Object, Array };

private:
  Type type;
  bool boolValue;
  int numberValue;
  std::string stringValue;
  Object objectValue;
  Array arrayValue;

public:
  Json();
  Json(bool value);
  Json(int value);
  Json(const std::string &value);
  Json(const Object &value);
  Json(const Array &value);

  Type getType() const;
  const Object &asObject() const;
  const Array &asArray() const;
  const std::string &asString() const;
  int asNumber() const;
  bool asBoolean() const;

  std::string toString() const;

  Json &operator[](const std::string &key);
  const Json &operator[](const std::string &key) const;

  Json &operator=(bool value);
  Json &operator=(int value);
  Json &operator=(const std::string &value);
  Json &operator=(const Object &value);
  Json &operator=(const Array &value);
};

#endif // JSON_HPP
