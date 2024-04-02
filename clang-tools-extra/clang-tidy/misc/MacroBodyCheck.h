#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MACROBODYCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MACROBODYCHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::misc {

class MacroBodyCheck : public ClangTidyCheck {
public:
  MacroBodyCheck(StringRef Name, ClangTidyContext *Context);
  ~MacroBodyCheck();
  
  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *) override;
};

} // namespace clang::tidy::misc

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_MACROBODYCHECK_H
