/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/

#include "dali/timing/delay_site_metadata.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace dali {
namespace delay_site_metadata_detail {

/** Minimal JSON value used for the small, versioned metadata contract. */
struct JsonValue {
  enum class Type { kObject, kArray, kString, kNumber, kBoolean, kNull };

  Type type = Type::kNull;
  std::map<std::string, JsonValue> object;
  std::vector<JsonValue> array;
  std::string string;
  double number = 0.0;
  bool boolean = false;
};

/** Parse JSON without adding a runtime dependency to Dali's core library. */
class JsonReader {
public:
  explicit JsonReader(const std::string &text) : text_(text) {}

  bool Parse(JsonValue *value, std::string *error_message) {
    SkipWhitespace();
    if (!ParseValue(value, error_message))
      return false;
    SkipWhitespace();
    if (position_ != text_.size()) {
      *error_message = "unexpected trailing JSON input";
      return false;
    }
    return true;
  }

private:
  void SkipWhitespace() {
    while (position_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[position_]))) {
      ++position_;
    }
  }

  bool Consume(char character) {
    if (position_ >= text_.size() || text_[position_] != character)
      return false;
    ++position_;
    return true;
  }

  bool ParseValue(JsonValue *value, std::string *error_message) {
    SkipWhitespace();
    if (position_ == text_.size()) {
      *error_message = "unexpected end of JSON input";
      return false;
    }
    switch (text_[position_]) {
    case '{':
      return ParseObject(value, error_message);
    case '[':
      return ParseArray(value, error_message);
    case '"':
      value->type = JsonValue::Type::kString;
      return ParseString(&value->string, error_message);
    case 't':
      return ParseLiteral("true", JsonValue::Type::kBoolean, true, value,
                          error_message);
    case 'f':
      return ParseLiteral("false", JsonValue::Type::kBoolean, false, value,
                          error_message);
    case 'n':
      return ParseLiteral("null", JsonValue::Type::kNull, false, value,
                          error_message);
    default:
      return ParseNumber(value, error_message);
    }
  }

  bool ParseObject(JsonValue *value, std::string *error_message) {
    value->type = JsonValue::Type::kObject;
    value->object.clear();
    ++position_;
    SkipWhitespace();
    if (Consume('}'))
      return true;
    while (true) {
      std::string key;
      if (!ParseString(&key, error_message))
        return false;
      SkipWhitespace();
      if (!Consume(':')) {
        *error_message = "expected ':' after JSON object key";
        return false;
      }
      JsonValue member;
      if (!ParseValue(&member, error_message))
        return false;
      if (!value->object.emplace(std::move(key), std::move(member)).second) {
        *error_message = "duplicate JSON object key";
        return false;
      }
      SkipWhitespace();
      if (Consume('}'))
        return true;
      if (!Consume(',')) {
        *error_message = "expected ',' or '}' in JSON object";
        return false;
      }
      SkipWhitespace();
    }
  }

  bool ParseArray(JsonValue *value, std::string *error_message) {
    value->type = JsonValue::Type::kArray;
    value->array.clear();
    ++position_;
    SkipWhitespace();
    if (Consume(']'))
      return true;
    while (true) {
      JsonValue element;
      if (!ParseValue(&element, error_message))
        return false;
      value->array.push_back(std::move(element));
      SkipWhitespace();
      if (Consume(']'))
        return true;
      if (!Consume(',')) {
        *error_message = "expected ',' or ']' in JSON array";
        return false;
      }
      SkipWhitespace();
    }
  }

  bool ParseString(std::string *value, std::string *error_message) {
    if (!Consume('"')) {
      *error_message = "expected JSON string";
      return false;
    }
    value->clear();
    while (position_ < text_.size()) {
      const char character = text_[position_++];
      if (character == '"')
        return true;
      if (static_cast<unsigned char>(character) < 0x20) {
        *error_message = "control character in JSON string";
        return false;
      }
      if (character != '\\') {
        value->push_back(character);
        continue;
      }
      if (position_ == text_.size()) {
        *error_message = "unterminated JSON escape";
        return false;
      }
      const char escaped = text_[position_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        value->push_back(escaped);
        break;
      case 'b':
        value->push_back('\b');
        break;
      case 'f':
        value->push_back('\f');
        break;
      case 'n':
        value->push_back('\n');
        break;
      case 'r':
        value->push_back('\r');
        break;
      case 't':
        value->push_back('\t');
        break;
      default:
        *error_message = "unsupported JSON string escape";
        return false;
      }
    }
    *error_message = "unterminated JSON string";
    return false;
  }

  bool ParseLiteral(const char *literal, JsonValue::Type type, bool boolean,
                    JsonValue *value, std::string *error_message) {
    const std::string text(literal);
    if (text_.compare(position_, text.size(), text) != 0) {
      *error_message = "invalid JSON literal";
      return false;
    }
    position_ += text.size();
    value->type = type;
    value->boolean = boolean;
    return true;
  }

  bool ParseNumber(JsonValue *value, std::string *error_message) {
    const char *begin = text_.c_str() + position_;
    char *end = nullptr;
    errno = 0;
    const double number = std::strtod(begin, &end);
    if (end == begin || errno == ERANGE || !std::isfinite(number)) {
      *error_message = "invalid JSON number";
      return false;
    }
    position_ += static_cast<std::size_t>(end - begin);
    value->type = JsonValue::Type::kNumber;
    value->number = number;
    return true;
  }

  const std::string &text_;
  std::size_t position_ = 0;
};

const JsonValue *FindMember(const JsonValue &object, const char *name) {
  if (object.type != JsonValue::Type::kObject)
    return nullptr;
  const auto it = object.object.find(name);
  return it == object.object.end() ? nullptr : &it->second;
}

bool ReadStringMember(const JsonValue &object, const char *name,
                      std::string *value, std::string *error_message) {
  const JsonValue *member = FindMember(object, name);
  if (member == nullptr || member->type != JsonValue::Type::kString ||
      member->string.empty()) {
    *error_message = std::string("missing nonempty string '") + name + "'";
    return false;
  }
  *value = member->string;
  return true;
}

bool ReadOptionalStringMember(const JsonValue &object, const char *name,
                              std::string *value, std::string *error_message) {
  const JsonValue *member = FindMember(object, name);
  if (member == nullptr)
    return true;
  if (member->type != JsonValue::Type::kString || member->string.empty()) {
    *error_message =
        std::string("optional '") + name + "' must be a nonempty string";
    return false;
  }
  *value = member->string;
  return true;
}

bool ReadNonnegativeIntegerMember(const JsonValue &object, const char *name,
                                  int *value, std::string *error_message) {
  const JsonValue *member = FindMember(object, name);
  if (member == nullptr || member->type != JsonValue::Type::kNumber ||
      member->number < 0.0 || std::floor(member->number) != member->number ||
      member->number > std::numeric_limits<int>::max()) {
    *error_message =
        std::string("'") + name + "' must be a nonnegative integer";
    return false;
  }
  *value = static_cast<int>(member->number);
  return true;
}

bool ReadBooleanMember(const JsonValue &object, const char *name, bool *value,
                       std::string *error_message) {
  const JsonValue *member = FindMember(object, name);
  if (member == nullptr || member->type != JsonValue::Type::kBoolean) {
    *error_message = std::string("'") + name + "' must be boolean";
    return false;
  }
  *value = member->boolean;
  return true;
}

} // namespace delay_site_metadata_detail

bool ReadDelayRepairSiteMetadata(const std::string &file_name,
                                 std::vector<DelayRepairSite> *sites,
                                 std::string *error_message) {
  using namespace delay_site_metadata_detail;
  std::ifstream input(file_name);
  if (!input.is_open()) {
    *error_message = "cannot open metadata file";
    return false;
  }
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  JsonValue root;
  JsonReader reader(text);
  if (!reader.Parse(&root, error_message))
    return false;

  const JsonValue *schema_version = FindMember(root, "schema_version");
  const JsonValue *time_unit = FindMember(root, "time_unit");
  const JsonValue *parameter_semantics =
      FindMember(root, "parameter_semantics");
  const JsonValue *declared_sites = FindMember(root, "delay_sites");
  if (root.type != JsonValue::Type::kObject || schema_version == nullptr ||
      schema_version->type != JsonValue::Type::kNumber ||
      schema_version->number != 1.0) {
    *error_message = "schema_version must be 1";
    return false;
  }
  if (time_unit == nullptr || time_unit->type != JsonValue::Type::kString ||
      time_unit->string != "ps") {
    *error_message = "time_unit must be ps";
    return false;
  }
  if (parameter_semantics == nullptr ||
      parameter_semantics->type != JsonValue::Type::kString ||
      parameter_semantics->string != "technology_specific") {
    *error_message = "parameter_semantics must be technology_specific";
    return false;
  }
  if (declared_sites == nullptr ||
      declared_sites->type != JsonValue::Type::kArray) {
    *error_message = "delay_sites must be an array";
    return false;
  }

  std::set<std::string> seen_ids;
  std::vector<DelayRepairSite> parsed_sites;
  parsed_sites.reserve(declared_sites->array.size());
  for (std::size_t index = 0; index < declared_sites->array.size(); ++index) {
    const JsonValue &value = declared_sites->array[index];
    if (value.type != JsonValue::Type::kObject) {
      *error_message =
          "delay_sites[" + std::to_string(index) + "] must be an object";
      return false;
    }
    DelayRepairSite site;
    if (!ReadStringMember(value, "id", &site.id, error_message) ||
        !ReadStringMember(value, "process_name", &site.process_name,
                          error_message) ||
        !ReadStringMember(value, "instance_name", &site.instance_name,
                          error_message) ||
        !ReadStringMember(value, "kind", &site.kind, error_message) ||
        !ReadStringMember(value, "parameter_name", &site.parameter_name,
                          error_message) ||
        !ReadOptionalStringMember(value, "logical_path_prefix",
                                  &site.logical_path_prefix, error_message) ||
        !ReadNonnegativeIntegerMember(value, "initial_parameter_value",
                                      &site.initial_parameter_value,
                                      error_message) ||
        !ReadBooleanMember(value, "adjustable", &site.adjustable,
                           error_message)) {
      *error_message =
          "delay_sites[" + std::to_string(index) + "]: " + *error_message;
      return false;
    }
    if (!seen_ids.insert(site.id).second) {
      *error_message = "duplicate delay-site id: " + site.id;
      return false;
    }
    parsed_sites.push_back(std::move(site));
  }

  *sites = std::move(parsed_sites);
  return true;
}

} // namespace dali
