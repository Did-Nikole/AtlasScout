// ISearchPlugin.hpp
#pragma once
#include <string>

class ISearchPlugin {
public:
  virtual ~ISearchPlugin() = default;

  // The required methods every plugin MUST implement
  virtual std::string get_name() = 0;
  virtual bool is_supported() = 0;
  virtual int priority() = 0;

  // Using a dummy parameters for the example
  virtual bool bitwise_xor_match(const uint32_t *, int, const uint32_t *,
                                 const uint32_t *, int, int, int, int) = 0;
};

// Define a standardized type for the C-style factory function
typedef ISearchPlugin *(*CreatePluginFunc)();