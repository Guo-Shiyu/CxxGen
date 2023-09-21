#include "matcher.h"
#include "preprocess.h"

template <typename FnPass> inline int entry(int argc, char **argv) {
  if (argc >= 3) {
    auto fileName = argv[1];
    auto target = argv[2];
    auto src = preprocess(target, fileName, argc >= 4);
    bool status = clang::tooling::runToolOnCode(
        std::make_unique<DeclFindingAction<FnPass>>(target, fileName), src,
        fileName);

    // assert(status);
    return 0;
  }
}