#pragma once
#include "ISearchPlugin.hpp"
#include <string>
#include <vector>
#include <iostream>

class OutputFormat {
public:
  enum Value { JSON, TSV, CSV, XML };

  Value value;

  OutputFormat() : value(JSON) {}
  OutputFormat(Value v) : value(v) {}

  operator Value() const { return value; }

  std::string suffix() const {
    switch (value) {
    case JSON:
      return "json";
    case TSV:
      return "tsv";
    case CSV:
      return "csv";
    case XML:
      return "xml";
    }
    return "json";
  }
};

struct dlObjects {
  ISearchPlugin *plugin;
  void *handle;
};

struct Config {
  std::string spritespath;
  std::string atlaspath;
  bool recurse = false;
  std::ostream *output_ptr = &std::cout;
  OutputFormat outputformat = OutputFormat::JSON;
  ISearchPlugin *searchMethod = nullptr;
  bool quiet = false;
  std::vector<int> rotations = {0, 90, 180, 270};

  std::string to_string() const {
    std::string rotations_str = "[";
    for (size_t i = 0; i < rotations.size(); ++i) {
      rotations_str += std::to_string(rotations[i]);
      if (i + 1 < rotations.size()) {
        rotations_str += ", ";
      }
    }
    rotations_str += "]";

    return "\n  spritespath: " + spritespath + "\n  atlaspath: " + atlaspath +
           "\n  recurse: " + (recurse ? "true" : "false") +
           "\n  outputformat: " + outputformat.suffix() + "\n  searchMethod: " +
           (searchMethod ? searchMethod->get_name() : "nullptr") +
           "\n  quiet: " + (quiet ? "true" : "false") +
           "\n  rotations: " + rotations_str;
  }
};
