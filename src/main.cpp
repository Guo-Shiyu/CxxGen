#include "matcher.h"

std::string read_to_string(const char *filename) {
  std::ifstream f(filename, std::ios::in);
  assert(f.is_open());
  return std::string{std::istreambuf_iterator<char>(f),
                     std::istreambuf_iterator<char>()};
}

int main(int argc, char **argv) {
  if (argc >= 2) {
    auto FileName = argv[1];
    auto Target = argv[2];
    auto Src = read_to_string(FileName);

    return clang::tooling::runToolOnCode(
        std::make_unique<DeclFindingAction>(Target), Src, FileName);
  }
}
