// Main application
#include "ISearchPlugin.hpp"
#include <dlfcn.h>
#include <iostream>

int main() {
  // 1. Load the shared object
  void *handle = dlopen("./plugins/avx2_search.so", RTLD_LAZY);

  // 2. Find the C-style factory function
  CreatePluginFunc create_func =
      (CreatePluginFunc)dlsym(handle, "create_plugin");

  // 3. Create the enforced C++ object
  ISearchPlugin *plugin = create_func();

  // 4. Use the required methods
  if (plugin->is_supported()) {
    std::cout << "Loaded: " << plugin->get_name() << "\n";
    plugin->bitwise_xor_match(nullptr, 0, nullptr, 0, 0, 0, 0);
  }

  delete plugin;
  dlclose(handle);
  return 0;
}