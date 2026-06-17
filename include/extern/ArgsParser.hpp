/*
 * Copyright (C) 2026 Nikole Smith (ApptsolutioNZ - appsolutionz.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// Forward declare the class so users can include this header
class ArgsParser;

/**
 * @brief Defines the supported data types for argument values.
 */
enum class ArgType { STRING, INTEGER, BOOLEAN, DOUBLE, REGEX, ULONGLONG };

/**
 * @brief Represents the values parsed from the command line.
 */
using ArgValue = std::variant<std::string, int, double, bool,
                              unsigned long long, std::vector<std::string>>;

/**
 * @brief Defines the specification for a single command-line argument.
 * Designed to be used with C++20 designated initializers.
 * e.g., parser.addArgument({.type=ArgType::STRING, .shortFlag="-f",
 * .longFlag="--file", .required=true});
 */
struct ArgSpec {
  // Core details (usually initialized first)
  ArgType type;
  std::string shortFlag = "";
  std::string longFlag = "";
  std::string description = "";

  // Configuration / Constraints (with sensible defaults)
  bool required = false;
  std::optional<ArgValue> defaultValue = std::nullopt;

  // Type-specific Constraints
  int length = -1;                       // -1 means use default based on type
  std::optional<int> min = std::nullopt; // For INTEGER
  std::optional<int> max = std::nullopt; // For INTEGER
  std::optional<unsigned long long> min_ull = std::nullopt; // For ULONGLONG
  std::optional<unsigned long long> max_ull = std::nullopt; // For ULONGLONG
  std::optional<std::vector<std::string>> allowedValues =
      std::nullopt;       // For STRING
  std::string regex = ""; // For REGEX
};

/**
 * @brief Command-line argument parser.
 */
class ArgsParser {
public:
  /**
   * @brief Adds an argument specification to the parser.
   * @param spec The specification configuration struct.
   * @return true on success, false on failure (e.g., duplicated flags).
   */
  bool addArgument(const ArgSpec &spec);

  /**
   * @brief Parses the command line arguments.
   * @param argc Argument count.
   * @param argv Argument vector.
   * @param results Output map to store parsed results. The key is the canonical
   * name without dashes. Missing optional arguments will not be keys in this
   * map, or they will hold std::nullopt.
   * @return true if parsing and validation were successful, false otherwise.
   */
  bool parse(int argc, char **argv,
             std::map<std::string, std::optional<ArgValue>> &results);

  /**
   * @brief Prints help/usage information to standard output (std::cout).
   */
  void showHelp() const;

private:
  std::vector<std::unique_ptr<ArgSpec>> m_specs;
  std::map<std::string, ArgSpec *> m_specMap;

  // Internal helpers
  std::string getCanonicalName(const ArgSpec &spec) const;
  std::optional<ArgValue>
  processValue(const ArgSpec &spec,
               const std::vector<std::string> &values) const;
  bool validateValue(const ArgSpec &spec, const ArgValue &value) const;
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

#ifdef ARGSPARSERRENEWED_IMPLEMENTATION

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

// --- Helper Functions ---

static std::string argTypeToString(ArgType type) {
  switch (type) {
  case ArgType::STRING:
    return "STRING";
  case ArgType::INTEGER:
    return "INTEGER";
  case ArgType::BOOLEAN:
    return "BOOLEAN";
  case ArgType::DOUBLE:
    return "DOUBLE";
  case ArgType::REGEX:
    return "REGEX";
  case ArgType::ULONGLONG:
    return "ULONGLONG";
  default:
    return "UNKNOWN";
  }
}

static bool isCompatible(ArgType type, const ArgValue &value) {
  switch (type) {
  case ArgType::STRING:
    return std::holds_alternative<std::string>(value);
  case ArgType::INTEGER:
    return std::holds_alternative<int>(value);
  case ArgType::BOOLEAN:
    return std::holds_alternative<bool>(value);
  case ArgType::DOUBLE:
    return std::holds_alternative<double>(value);
  case ArgType::REGEX:
    return std::holds_alternative<std::vector<std::string>>(value);
  case ArgType::ULONGLONG:
    return std::holds_alternative<unsigned long long>(value);
  default:
    return false;
  }
}

// --- ArgsParser Implementation ---

bool ArgsParser::addArgument(const ArgSpec &spec) {
  ArgSpec modifiedSpec = spec;
  // Initialize default length based on type if not explicitly set
  if (modifiedSpec.length == -1) {
    if (modifiedSpec.type == ArgType::BOOLEAN) {
      modifiedSpec.length = 0;
    } else {
      modifiedSpec.length = 1;
    }
  }

  if (modifiedSpec.shortFlag.empty() && modifiedSpec.longFlag.empty()) {
    std::cerr
        << "Error: Argument must define at least one flag (short or long)."
        << std::endl;
    return false;
  }

  // Check for duplicate flags
  if (!modifiedSpec.shortFlag.empty() &&
      m_specMap.count(modifiedSpec.shortFlag)) {
    std::cerr << "Error: Duplicate argument short flag found: "
              << modifiedSpec.shortFlag << std::endl;
    return false;
  }
  if (!modifiedSpec.longFlag.empty() &&
      m_specMap.count(modifiedSpec.longFlag)) {
    std::cerr << "Error: Duplicate argument long flag found: "
              << modifiedSpec.longFlag << std::endl;
    return false;
  }

  // Add validation for lengths
  if (modifiedSpec.type == ArgType::BOOLEAN && modifiedSpec.length != 0) {
    std::cerr << "Error: BOOLEAN type arguments must have length 0."
              << std::endl;
    return false;
  }

  if (modifiedSpec.type == ArgType::REGEX && modifiedSpec.length != 1) {
    std::cerr << "Error: REGEX type arguments must have length 1." << std::endl;
    return false;
  }

  if (modifiedSpec.type == ArgType::REGEX && modifiedSpec.regex.empty()) {
    std::cerr << "Error: REGEX arguments must define a pattern." << std::endl;
    return false;
  }

  if (modifiedSpec.defaultValue.has_value() &&
      !isCompatible(modifiedSpec.type, modifiedSpec.defaultValue.value())) {
    std::cerr << "Error: Default value type does not match argument type."
              << std::endl;
    return false;
  }

  // Add it to our owned specs vector
  m_specs.push_back(std::make_unique<ArgSpec>(modifiedSpec));

  // Wire up map pointers
  ArgSpec *ptr = m_specs.back().get();
  if (!modifiedSpec.shortFlag.empty())
    m_specMap[modifiedSpec.shortFlag] = ptr;
  if (!modifiedSpec.longFlag.empty())
    m_specMap[modifiedSpec.longFlag] = ptr;

  return true;
}

std::string ArgsParser::getCanonicalName(const ArgSpec &spec) const {
  if (!spec.longFlag.empty()) {
    if (spec.longFlag.rfind("--", 0) == 0)
      return spec.longFlag.substr(2);
    return spec.longFlag;
  }
  if (!spec.shortFlag.empty()) {
    if (spec.shortFlag.rfind("-", 0) == 0)
      return spec.shortFlag.substr(1);
    return spec.shortFlag;
  }
  return "";
}

std::optional<ArgValue>
ArgsParser::processValue(const ArgSpec &spec,
                         const std::vector<std::string> &values) const {
  if (spec.length > 1) {
    if (spec.type == ArgType::STRING) {
      return values; // Variant handles vector<string> implicitly
    }
    std::cerr << "Error: Multi-value support is limited to STRING arguments."
              << std::endl;
    return std::nullopt;
  }

  if (spec.type == ArgType::BOOLEAN) {
    if (!values.empty()) {
      std::cerr << "Error: BOOLEAN flag does not accept a value." << std::endl;
      return std::nullopt;
    }
    return true;
  }

  if (values.empty()) {
    std::cerr << "Error: Missing required value. (values vector is empty)"
              << std::endl;
    return std::nullopt;
  }

  const std::string &valStr = values[0];

  switch (spec.type) {
  case ArgType::STRING:
    return valStr;
  case ArgType::INTEGER: {
    try {
      size_t pos;
      int val = std::stoi(valStr, &pos);
      if (pos != valStr.length())
        throw std::invalid_argument("Remaining chars");
      return val;
    } catch (...) {
      std::cerr << "Error: Value '" << valStr << "' is not a valid integer."
                << std::endl;
      return std::nullopt;
    }
  }
  case ArgType::ULONGLONG: {
    if (!valStr.empty() && valStr[0] == '-') {
      std::cerr << "Error: Value '" << valStr
                << "' cannot be negative for an unsigned argument."
                << std::endl;
      return std::nullopt;
    }
    try {
      size_t pos;
      unsigned long long val = std::stoull(valStr, &pos);
      if (pos != valStr.length())
        throw std::invalid_argument("Remaining chars");
      return val;
    } catch (...) {
      std::cerr << "Error: Value '" << valStr
                << "' is not a valid unsigned long long." << std::endl;
      return std::nullopt;
    }
  }
  case ArgType::DOUBLE: {
    try {
      size_t pos;
      double val = std::stod(valStr, &pos);
      if (pos != valStr.length())
        throw std::invalid_argument("Remaining chars");
      return val;
    } catch (...) {
      std::cerr << "Error: Value '" << valStr << "' is not a valid double."
                << std::endl;
      return std::nullopt;
    }
  }
  case ArgType::REGEX: {
    try {
      std::regex pattern(spec.regex);
      std::smatch match;
      std::vector<std::string> captured;

      if (std::regex_match(valStr, match, pattern)) {
        for (size_t i = 1; i < match.size(); ++i) {
          captured.push_back(match[i].str());
        }
      }
      return captured;
    } catch (const std::regex_error &e) {
      std::cerr << "Error: Invalid regex pattern: " << e.what() << std::endl;
      return std::nullopt;
    }
  }
  default:
    return std::nullopt;
  }
}

bool ArgsParser::validateValue(const ArgSpec &spec,
                               const ArgValue &value) const {
  if (spec.type == ArgType::INTEGER) {
    int val = std::get<int>(value);
    if (spec.min.has_value() && val < spec.min.value()) {
      std::cerr << "Error: Value " << val << " is less than minimum "
                << spec.min.value() << std::endl;
      return false;
    }
    if (spec.max.has_value() && val > spec.max.value()) {
      std::cerr << "Error: Value " << val << " is greater than maximum "
                << spec.max.value() << std::endl;
      return false;
    }
  } else if (spec.type == ArgType::ULONGLONG) {
    unsigned long long val = std::get<unsigned long long>(value);
    if (spec.min_ull.has_value() && val < spec.min_ull.value()) {
      std::cerr << "Error: Value " << val << " is less than minimum "
                << spec.min_ull.value() << std::endl;
      return false;
    }
    if (spec.max_ull.has_value() && val > spec.max_ull.value()) {
      std::cerr << "Error: Value " << val << " is greater than maximum "
                << spec.max_ull.value() << std::endl;
      return false;
    }
  } else if (spec.type == ArgType::STRING &&
             !std::holds_alternative<std::vector<std::string>>(value)) {
    std::string val = std::get<std::string>(value);
    if (spec.allowedValues.has_value()) {
      const auto &allowed = spec.allowedValues.value();
      if (std::find(allowed.begin(), allowed.end(), val) == allowed.end()) {
        std::cerr << "Error: Value '" << val << "' is not in allowed list."
                  << std::endl;
        return false;
      }
    }
  }
  return true;
}

bool ArgsParser::parse(
    int argc, char **argv,
    std::map<std::string, std::optional<ArgValue>> &results) {
  results.clear();

  // Apply defaults
  for (const auto &specPtr : m_specs) {
    const auto &spec = *specPtr;
    std::string canon = getCanonicalName(spec);
    if (spec.defaultValue.has_value()) {
      results[canon] = spec.defaultValue.value();
    } else if (spec.type == ArgType::BOOLEAN) {
      results[canon] = false;
    } else if (spec.type == ArgType::REGEX) {
      results[canon] = std::vector<std::string>{};
    }
  }

  // Process arguments
  for (int i = 1; i < argc; ++i) {
    std::string currentArg = argv[i];
    auto it = m_specMap.find(currentArg);

    if (it != m_specMap.end()) {
      const ArgSpec *spec = it->second;
      std::string canon = getCanonicalName(*spec);

      if (spec->type == ArgType::BOOLEAN) {
        results[canon] = true;
        continue;
      }

      std::vector<std::string> values;
      for (int j = 0; j < spec->length; ++j) {
        if (i + 1 < argc) {
          std::string nextArg = argv[i + 1];
          // Check if it looks like a flag, but allow negative numbers as
          // values.
          if ((nextArg.length() > 1 && nextArg[0] == '-' &&
               !std::isdigit(nextArg[1])) ||
              (nextArg.length() > 2 && nextArg.rfind("--", 0) == 0)) {
            break;
          }
          values.push_back(nextArg);
          i++; // Consume value
        }
      }

      if (values.size() != static_cast<size_t>(spec->length)) {
        std::cerr << "Error: Argument '" << currentArg << "' requires "
                  << spec->length << " values." << std::endl;
        return false;
      }

      std::optional<ArgValue> parsedOpt = processValue(*spec, values);
      if (!parsedOpt.has_value()) {
        std::cerr << "ProcessValue failed for " << currentArg << std::endl;
        return false;
      }

      if (spec->length <= 1 && !validateValue(*spec, parsedOpt.value())) {
        std::cerr << "ValidateValue failed for " << currentArg << std::endl;
        return false;
      }

      results[canon] = parsedOpt.value();
    } else {
      std::cerr << "Warning: Unknown argument skipped: " << currentArg
                << std::endl;
    }
  }

  // Check required constraints
  for (const auto &specPtr : m_specs) {
    const auto &spec = *specPtr;
    if (spec.required) {
      std::string canon = getCanonicalName(spec);
      if (!spec.defaultValue.has_value() &&
          results.find(canon) == results.end()) {
        std::cerr << "Error: Missing required argument: " << canon << std::endl;
        return false;
      }
    }
  }

  return true;
}

void ArgsParser::showHelp() const {
  size_t maxFlagsWidth = 0;
  for (const auto &specPtr : m_specs) {
    const auto &spec = *specPtr;
    std::stringstream ss;
    if (!spec.shortFlag.empty())
      ss << spec.shortFlag;
    if (!spec.shortFlag.empty() && !spec.longFlag.empty())
      ss << ", ";
    if (!spec.longFlag.empty())
      ss << spec.longFlag;

    if (spec.type != ArgType::BOOLEAN) {
      ss << " <" << argTypeToString(spec.type);
      if (spec.length > 1)
        ss << "[" << spec.length << "]";
      ss << ">";
    }
    maxFlagsWidth = std::max(maxFlagsWidth, ss.str().length());
  }

  int columnWidth = static_cast<int>(maxFlagsWidth) + 4;
  std::cout << "Usage: [program_name] [options...]\nOptions:\n";

  for (const auto &specPtr : m_specs) {
    const auto &spec = *specPtr;
    std::stringstream flagsStream;
    if (!spec.shortFlag.empty())
      flagsStream << spec.shortFlag;
    if (!spec.shortFlag.empty() && !spec.longFlag.empty())
      flagsStream << ", ";
    if (!spec.longFlag.empty())
      flagsStream << spec.longFlag;

    if (spec.type != ArgType::BOOLEAN) {
      flagsStream << " <" << argTypeToString(spec.type);
      if (spec.length > 1)
        flagsStream << "[" << spec.length << "]";
      flagsStream << ">";
    }

    std::cout << "  " << std::left << std::setw(columnWidth)
              << flagsStream.str() << spec.description;

    if (spec.required) {
      std::cout << " (REQUIRED)";
    } else if (spec.defaultValue.has_value() && spec.type != ArgType::REGEX) {
      std::cout << " (Default: ";
      if (std::holds_alternative<std::string>(spec.defaultValue.value())) {
        std::cout << std::get<std::string>(spec.defaultValue.value());
      } else if (std::holds_alternative<int>(spec.defaultValue.value())) {
        std::cout << std::get<int>(spec.defaultValue.value());
      } else if (std::holds_alternative<unsigned long long>(
                     spec.defaultValue.value())) {
        std::cout << std::get<unsigned long long>(spec.defaultValue.value());
      } else if (std::holds_alternative<double>(spec.defaultValue.value())) {
        std::cout << std::get<double>(spec.defaultValue.value());
      } else if (std::holds_alternative<bool>(spec.defaultValue.value())) {
        std::cout << (std::get<bool>(spec.defaultValue.value()) ? "true"
                                                                : "false");
      }
      std::cout << ")";
    }
    std::cout << "\n";
  }
}

#endif // ARGSPARSERRENEWED_IMPLEMENTATION
