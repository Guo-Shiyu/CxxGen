#include "matcher.h"

#include <string>
#include <fstream>

std::string read_to_string(const char *filename) {
  std::ifstream f(filename, std::ios::in);
  assert(f.is_open());
  return std::string{std::istreambuf_iterator<char>(f),
                     std::istreambuf_iterator<char>()};
}

std::string preprocess(const char *target, const char *filename,
                       bool enable_dump = false) {
  constexpr auto kTmpPPFilePath = "/tmp/_.cc";
  auto cmd = fmt::format("cc -E {} -o {} > /dev/null", filename, kTmpPPFilePath);
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

int main(int argc, char **argv) {
  if (argc >= 3) {
    auto fileName = argv[1];
    auto target = argv[2];
    auto src = preprocess(target, fileName, argc >= 4);

    bool status = clang::tooling::runToolOnCode(
        std::make_unique<DeclFindingAction>(target, fileName), src, fileName);

    // assert(status);
    return 0;
  }
}
