#include "fmt/format.h"

#include <cassert>
#include <fstream>
#include <string>

inline std::string read_to_string(const char *filename) {
  std::ifstream f(filename, std::ios::in);
  assert(f.is_open());
  return std::string{std::istreambuf_iterator<char>(f),
                     std::istreambuf_iterator<char>()};
}

inline std::string preprocess(const char *target, const char *filename,
                              bool enable_dump = false) {
  constexpr auto kTmpPPFilePath = "/tmp/_.cc";
  auto cmd =
      fmt::format("cc -E {} -o {} > /dev/null", filename, kTmpPPFilePath);
  int ret = ::system(cmd.c_str());
  assert(ret == 0);
  // fmt::println("Execute command: {} ... return: {}", cmd, ret);

  if (enable_dump) {
    cmd = fmt::format("clang-check -ast-dump --ast-dump-filter {} {}", //
                      target, filename);
    ret = ::system(cmd.c_str());
    // fmt::println("Execute command: {} ... return: {}", cmd, ret);
  }
  return read_to_string(kTmpPPFilePath);
}