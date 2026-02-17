#include "cgul/io/cgul_document.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace cgul {

namespace {

struct JsonValue {
  enum class Type {
    Null,
    Bool,
    Integer,
    String,
    Object,
    Array,
  };

  Type type = Type::Null;
  bool boolValue = false;
  int64_t intValue = 0;
  std::string stringValue;
  std::map<std::string, JsonValue> objectValue;
  std::vector<JsonValue> arrayValue;
};

class JsonParser {
 public:
  explicit JsonParser(const std::string& input) : input_(input) {}

  bool Parse(JsonValue* outValue, std::string* outError) {
    SkipWhitespace();
    if (!ParseValue(outValue, outError)) {
      return false;
    }
    SkipWhitespace();
    if (pos_ != input_.size()) {
      return Error("unexpected trailing characters", outError);
    }
    return true;
  }

 private:
  bool ParseValue(JsonValue* outValue, std::string* outError) {
    if (pos_ >= input_.size()) {
      return Error("unexpected end of input", outError);
    }

    const char ch = input_[pos_];
    if (ch == '{') {
      return ParseObject(outValue, outError);
    }
    if (ch == '[') {
      return ParseArray(outValue, outError);
    }
    if (ch == '"') {
      outValue->type = JsonValue::Type::String;
      return ParseString(&outValue->stringValue, outError);
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      outValue->type = JsonValue::Type::Integer;
      return ParseInteger(&outValue->intValue, outError);
    }
    if (StartsWith("true")) {
      pos_ += 4;
      outValue->type = JsonValue::Type::Bool;
      outValue->boolValue = true;
      return true;
    }
    if (StartsWith("false")) {
      pos_ += 5;
      outValue->type = JsonValue::Type::Bool;
      outValue->boolValue = false;
      return true;
    }
    if (StartsWith("null")) {
      pos_ += 4;
      outValue->type = JsonValue::Type::Null;
      return true;
    }

    return Error("unexpected token", outError);
  }

  bool ParseObject(JsonValue* outValue, std::string* outError) {
    if (!Consume('{', outError)) {
      return false;
    }

    outValue->type = JsonValue::Type::Object;
    outValue->objectValue.clear();

    SkipWhitespace();
    if (TryConsume('}')) {
      return true;
    }

    while (true) {
      std::string key;
      if (!ParseString(&key, outError)) {
        return false;
      }

      SkipWhitespace();
      if (!Consume(':', outError)) {
        return false;
      }

      SkipWhitespace();
      JsonValue value;
      if (!ParseValue(&value, outError)) {
        return false;
      }

      if (outValue->objectValue.find(key) != outValue->objectValue.end()) {
        return Error("duplicate object key: " + key, outError);
      }
      outValue->objectValue.emplace(std::move(key), std::move(value));

      SkipWhitespace();
      if (TryConsume('}')) {
        return true;
      }
      if (!Consume(',', outError)) {
        return false;
      }
      SkipWhitespace();
    }
  }

  bool ParseArray(JsonValue* outValue, std::string* outError) {
    if (!Consume('[', outError)) {
      return false;
    }

    outValue->type = JsonValue::Type::Array;
    outValue->arrayValue.clear();

    SkipWhitespace();
    if (TryConsume(']')) {
      return true;
    }

    while (true) {
      JsonValue item;
      if (!ParseValue(&item, outError)) {
        return false;
      }
      outValue->arrayValue.push_back(std::move(item));

      SkipWhitespace();
      if (TryConsume(']')) {
        return true;
      }
      if (!Consume(',', outError)) {
        return false;
      }
      SkipWhitespace();
    }
  }

  bool ParseString(std::string* outString, std::string* outError) {
    if (!Consume('"', outError)) {
      return false;
    }

    outString->clear();
    while (pos_ < input_.size()) {
      const char ch = input_[pos_++];
      if (ch == '"') {
        return true;
      }
      if (ch == '\\') {
        if (pos_ >= input_.size()) {
          return Error("unterminated escape sequence", outError);
        }
        const char esc = input_[pos_++];
        switch (esc) {
          case '"': outString->push_back('"'); break;
          case '\\': outString->push_back('\\'); break;
          case 'n': outString->push_back('\n'); break;
          case 'r': outString->push_back('\r'); break;
          case 't': outString->push_back('\t'); break;
          default:
            return Error(std::string("unsupported string escape: \\") + esc, outError);
        }
        continue;
      }

      const unsigned char uch = static_cast<unsigned char>(ch);
      if (uch < 0x20) {
        return Error("control character in string", outError);
      }
      outString->push_back(ch);
    }

    return Error("unterminated string literal", outError);
  }

  bool ParseInteger(int64_t* outInteger, std::string* outError) {
    if (pos_ >= input_.size()) {
      return Error("expected integer", outError);
    }

    bool negative = false;
    if (input_[pos_] == '-') {
      negative = true;
      ++pos_;
      if (pos_ >= input_.size()) {
        return Error("expected digits after '-'", outError);
      }
    }

    if (input_[pos_] == '0' && pos_ + 1 < input_.size() &&
        std::isdigit(static_cast<unsigned char>(input_[pos_ + 1])) != 0) {
      return Error("leading zeros are not allowed", outError);
    }

    if (std::isdigit(static_cast<unsigned char>(input_[pos_])) == 0) {
      return Error("expected integer digits", outError);
    }

    uint64_t magnitude = 0;
    while (pos_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[pos_])) != 0) {
      const uint64_t digit = static_cast<uint64_t>(input_[pos_] - '0');
      if (magnitude > (std::numeric_limits<uint64_t>::max() - digit) / 10ULL) {
        return Error("integer out of range", outError);
      }
      magnitude = magnitude * 10ULL + digit;
      ++pos_;
    }

    if (pos_ < input_.size() && (input_[pos_] == '.' || input_[pos_] == 'e' || input_[pos_] == 'E')) {
      return Error("floating-point numbers are not supported", outError);
    }

    if (negative) {
      const uint64_t maxNegative = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL;
      if (magnitude > maxNegative) {
        return Error("integer out of int64 range", outError);
      }
      if (magnitude == maxNegative) {
        *outInteger = std::numeric_limits<int64_t>::min();
      } else {
        *outInteger = -static_cast<int64_t>(magnitude);
      }
    } else {
      if (magnitude > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return Error("integer out of int64 range", outError);
      }
      *outInteger = static_cast<int64_t>(magnitude);
    }

    return true;
  }

  bool Consume(char expected, std::string* outError) {
    if (pos_ >= input_.size() || input_[pos_] != expected) {
      return Error(std::string("expected '") + expected + "'", outError);
    }
    ++pos_;
    return true;
  }

  bool TryConsume(char expected) {
    if (pos_ < input_.size() && input_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool StartsWith(const char* token) const {
    size_t i = 0;
    while (token[i] != '\0') {
      if (pos_ + i >= input_.size() || input_[pos_ + i] != token[i]) {
        return false;
      }
      ++i;
    }
    return true;
  }

  void SkipWhitespace() {
    while (pos_ < input_.size()) {
      const unsigned char ch = static_cast<unsigned char>(input_[pos_]);
      if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool Error(const std::string& message, std::string* outError) const {
    if (outError != nullptr) {
      *outError = "Parse error at byte " + std::to_string(pos_) + ": " + message;
    }
    return false;
  }

  const std::string& input_;
  size_t pos_ = 0;
};

bool RequireType(const JsonValue& value, JsonValue::Type expected, const std::string& label,
                 std::string* outError) {
  if (value.type != expected) {
    if (outError != nullptr) {
      *outError = "Expected '" + label + "' to be " +
                  (expected == JsonValue::Type::Object ? "an object" :
                   expected == JsonValue::Type::Array ? "an array" :
                   expected == JsonValue::Type::String ? "a string" :
                   expected == JsonValue::Type::Integer ? "an integer" :
                   expected == JsonValue::Type::Bool ? "a boolean" : "null");
    }
    return false;
  }
  return true;
}

const JsonValue* FindObjectKey(const JsonValue& objectValue, const std::string& key) {
  const auto it = objectValue.objectValue.find(key);
  if (it == objectValue.objectValue.end()) {
    return nullptr;
  }
  return &it->second;
}

bool ReadRequiredInt(const JsonValue& objectValue, const std::string& key, int* out,
                     std::string* outError) {
  const JsonValue* value = FindObjectKey(objectValue, key);
  if (value == nullptr) {
    if (outError != nullptr) {
      *outError = "Missing required key: " + key;
    }
    return false;
  }
  if (!RequireType(*value, JsonValue::Type::Integer, key, outError)) {
    return false;
  }
  if (value->intValue < static_cast<int64_t>(std::numeric_limits<int>::min()) ||
      value->intValue > static_cast<int64_t>(std::numeric_limits<int>::max())) {
    if (outError != nullptr) {
      *outError = "Integer out of range for key: " + key;
    }
    return false;
  }
  *out = static_cast<int>(value->intValue);
  return true;
}

bool ReadRequiredUInt32(const JsonValue& objectValue, const std::string& key, uint32_t* out,
                        std::string* outError) {
  const JsonValue* value = FindObjectKey(objectValue, key);
  if (value == nullptr) {
    if (outError != nullptr) {
      *outError = "Missing required key: " + key;
    }
    return false;
  }
  if (!RequireType(*value, JsonValue::Type::Integer, key, outError)) {
    return false;
  }
  if (value->intValue < 0 ||
      value->intValue > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
    if (outError != nullptr) {
      *outError = "Unsigned integer out of range for key: " + key;
    }
    return false;
  }
  *out = static_cast<uint32_t>(value->intValue);
  return true;
}

bool ReadRequiredString(const JsonValue& objectValue, const std::string& key, std::string* out,
                        std::string* outError) {
  const JsonValue* value = FindObjectKey(objectValue, key);
  if (value == nullptr) {
    if (outError != nullptr) {
      *outError = "Missing required key: " + key;
    }
    return false;
  }
  if (!RequireType(*value, JsonValue::Type::String, key, outError)) {
    return false;
  }
  *out = value->stringValue;
  return true;
}

std::string EscapeString(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped.push_back(ch); break;
    }
  }
  return escaped;
}

}  // namespace

const char* ToString(WidgetKind kind) {
  switch (kind) {
    case WidgetKind::Window: return "window";
    case WidgetKind::Panel: return "panel";
    case WidgetKind::Label: return "label";
    case WidgetKind::Button: return "button";
  }
  return "panel";
}

bool ParseWidgetKind(const std::string& text, WidgetKind* outKind) {
  if (outKind == nullptr) {
    return false;
  }
  if (text == "window") {
    *outKind = WidgetKind::Window;
    return true;
  }
  if (text == "panel") {
    *outKind = WidgetKind::Panel;
    return true;
  }
  if (text == "label") {
    *outKind = WidgetKind::Label;
    return true;
  }
  if (text == "button") {
    *outKind = WidgetKind::Button;
    return true;
  }
  return false;
}

bool SaveCgulFile(const std::string& path, const CgulDocument& doc, std::string* outError) {
  if (outError != nullptr) {
    outError->clear();
  }

  std::ostringstream os;
  os << "{\n";
  os << "  \"cgulVersion\": \"" << EscapeString(doc.cgulVersion) << "\",\n";
  os << "  \"grid\": {\n";
  os << "    \"w\": " << doc.gridWCells << ",\n";
  os << "    \"h\": " << doc.gridHCells << "\n";
  os << "  },\n";
  os << "  \"seed\": " << doc.seed << ",\n";
  if (!doc.meta.empty()) {
    os << "  \"meta\": {\n";
    size_t metaIndex = 0;
    for (const auto& entry : doc.meta) {
      os << "    \"" << EscapeString(entry.first) << "\": \"" << EscapeString(entry.second) << "\"";
      if (metaIndex + 1 < doc.meta.size()) {
        os << ",";
      }
      os << "\n";
      ++metaIndex;
    }
    os << "  },\n";
  }
  os << "  \"widgets\": [";

  if (!doc.widgets.empty()) {
    os << "\n";
  }

  for (size_t i = 0; i < doc.widgets.size(); ++i) {
    const Widget& widget = doc.widgets[i];
    os << "    {\n";
    os << "      \"id\": " << widget.id << ",\n";
    os << "      \"kind\": \"" << ToString(widget.kind) << "\",\n";
    os << "      \"bounds\": {\n";
    os << "        \"x\": " << widget.boundsCells.x << ",\n";
    os << "        \"y\": " << widget.boundsCells.y << ",\n";
    os << "        \"w\": " << widget.boundsCells.w << ",\n";
    os << "        \"h\": " << widget.boundsCells.h << "\n";
    os << "      }";
    if (!widget.title.empty()) {
      os << ",\n";
      os << "      \"title\": \"" << EscapeString(widget.title) << "\"\n";
    } else {
      os << "\n";
    }
    os << "    }";
    if (i + 1 < doc.widgets.size()) {
      os << ",";
    }
    os << "\n";
  }

  if (!doc.widgets.empty()) {
    os << "  ";
  }
  os << "]\n";
  os << "}\n";

  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    if (outError != nullptr) {
      *outError = "Failed to open file for writing: " + path;
    }
    return false;
  }

  const std::string text = os.str();
  file.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!file.good()) {
    if (outError != nullptr) {
      *outError = "Failed to write file: " + path;
    }
    return false;
  }

  return true;
}

bool LoadCgulFile(const std::string& path, CgulDocument* outDoc, std::string* outError) {
  if (outError != nullptr) {
    outError->clear();
  }
  if (outDoc == nullptr) {
    if (outError != nullptr) {
      *outError = "outDoc must not be null";
    }
    return false;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    if (outError != nullptr) {
      *outError = "Failed to open file for reading: " + path;
    }
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  const std::string text = buffer.str();

  JsonValue root;
  JsonParser parser(text);
  if (!parser.Parse(&root, outError)) {
    return false;
  }
  if (!RequireType(root, JsonValue::Type::Object, "root", outError)) {
    return false;
  }

  CgulDocument doc;

  if (!ReadRequiredString(root, "cgulVersion", &doc.cgulVersion, outError)) {
    return false;
  }

  const JsonValue* gridValue = FindObjectKey(root, "grid");
  if (gridValue == nullptr) {
    if (outError != nullptr) {
      *outError = "Missing required key: grid";
    }
    return false;
  }
  if (!RequireType(*gridValue, JsonValue::Type::Object, "grid", outError)) {
    return false;
  }
  if (!ReadRequiredInt(*gridValue, "w", &doc.gridWCells, outError) ||
      !ReadRequiredInt(*gridValue, "h", &doc.gridHCells, outError)) {
    return false;
  }

  const JsonValue* seedValue = FindObjectKey(root, "seed");
  if (seedValue == nullptr) {
    if (outError != nullptr) {
      *outError = "Missing required key: seed";
    }
    return false;
  }
  if (!RequireType(*seedValue, JsonValue::Type::Integer, "seed", outError)) {
    return false;
  }
  if (seedValue->intValue < 0) {
    if (outError != nullptr) {
      *outError = "seed must be a non-negative integer";
    }
    return false;
  }
  doc.seed = static_cast<uint64_t>(seedValue->intValue);

  const JsonValue* metaValue = FindObjectKey(root, "meta");
  if (metaValue != nullptr) {
    if (!RequireType(*metaValue, JsonValue::Type::Object, "meta", outError)) {
      return false;
    }
    for (const auto& entry : metaValue->objectValue) {
      if (!RequireType(entry.second, JsonValue::Type::String, "meta." + entry.first, outError)) {
        return false;
      }
      doc.meta[entry.first] = entry.second.stringValue;
    }
  }

  const JsonValue* widgetsValue = FindObjectKey(root, "widgets");
  if (widgetsValue == nullptr) {
    if (outError != nullptr) {
      *outError = "Missing required key: widgets";
    }
    return false;
  }
  if (!RequireType(*widgetsValue, JsonValue::Type::Array, "widgets", outError)) {
    return false;
  }

  doc.widgets.clear();
  doc.widgets.reserve(widgetsValue->arrayValue.size());

  for (size_t i = 0; i < widgetsValue->arrayValue.size(); ++i) {
    const JsonValue& item = widgetsValue->arrayValue[i];
    if (!RequireType(item, JsonValue::Type::Object, "widgets[" + std::to_string(i) + "]", outError)) {
      return false;
    }

    Widget widget;
    if (!ReadRequiredUInt32(item, "id", &widget.id, outError)) {
      return false;
    }

    std::string kindString;
    if (!ReadRequiredString(item, "kind", &kindString, outError)) {
      return false;
    }
    if (!ParseWidgetKind(kindString, &widget.kind)) {
      if (outError != nullptr) {
        *outError = "Unknown widget kind: " + kindString;
      }
      return false;
    }

    const JsonValue* boundsValue = FindObjectKey(item, "bounds");
    if (boundsValue == nullptr) {
      if (outError != nullptr) {
        *outError = "Missing required key: bounds";
      }
      return false;
    }
    if (!RequireType(*boundsValue, JsonValue::Type::Object, "bounds", outError)) {
      return false;
    }

    if (!ReadRequiredInt(*boundsValue, "x", &widget.boundsCells.x, outError) ||
        !ReadRequiredInt(*boundsValue, "y", &widget.boundsCells.y, outError) ||
        !ReadRequiredInt(*boundsValue, "w", &widget.boundsCells.w, outError) ||
        !ReadRequiredInt(*boundsValue, "h", &widget.boundsCells.h, outError)) {
      return false;
    }

    const JsonValue* titleValue = FindObjectKey(item, "title");
    if (titleValue != nullptr) {
      if (!RequireType(*titleValue, JsonValue::Type::String, "title", outError)) {
        return false;
      }
      widget.title = titleValue->stringValue;
    }

    doc.widgets.push_back(std::move(widget));
  }

  *outDoc = std::move(doc);
  return true;
}

}  // namespace cgul
