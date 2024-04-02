#include "MacroBodyCheck.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/SetVector.h"
#include <memory>

using namespace llvm;
using namespace clang;

namespace clang::tidy::misc {

namespace {
const StringRef MacroBodyNote = "macro body should be enclosed in parentheses";
const StringRef ParameterNote =
    "parameter '{0}' should be enclosed in parentheses";
const StringRef LParen = StringRef("(");
const StringRef RParen = StringRef(")");

class MacroParenComplianceChecker {
public:
  explicit MacroParenComplianceChecker(const MacroInfo *MI) {
    Tokens = MI->tokens();
    Parameters.insert(MI->param_begin(), MI->param_end());
    traverseMacroBody();
  }

  SourceRange getMacroBodyRange() const {
    return {Tokens[0].getLocation(), Tokens[Tokens.size() - 1].getEndLoc()};
  }

  const std::vector<Token> &getParamTokenWithoutParentheses() const {
    return ParamTokenWithoutParentheses;
  }

  bool isMacroBodyCompliant() const { return IsMacroBodyCompliant; }

private:
  llvm::ArrayRef<Token> Tokens;
  llvm::SetVector<const IdentifierInfo *> Parameters;
  std::vector<Token> ParamTokenWithoutParentheses;
  bool IsMacroBodyCompliant = true;
  bool IsMacroBodyEnclosedInBraces = false;
  unsigned int ParenDepth = 0U;
  unsigned int TokenIndex = 0U;

private:
  void traverseMacroBody() {
    IsMacroBodyEnclosedInBraces = isEnclosedInBraces();
    for (TokenIndex = 0U; TokenIndex < Tokens.size(); TokenIndex++) {
      checkMacroBodyCompliant();
      if (Tokens.size() < 2U) {
        break;
      }
      auto Token = Tokens[TokenIndex];
      if (Parameters.count(Token.getIdentifierInfo()) == 0U) {
        continue;
      }
      if (TokenIndex >= 1U && TokenIndex + 2 <= Tokens.size() &&
          isHashOrHashHashOperand()) {
        continue;
      }
      if (TokenIndex > 1U && TokenIndex + 2 < Tokens.size() &&
          isEnclosedInParentheses()) {
        continue;
      }
      ParamTokenWithoutParentheses.push_back(Token);
    }
    return;
  }

  bool isEnclosedInParentheses() {
    return Tokens[TokenIndex - 1].is(tok::l_paren) &&
           Tokens[TokenIndex + 1].is(tok::r_paren);
  }

  bool isEnclosedInBraces() {
    return Tokens[0].is(tok::l_brace) &&
           Tokens[Tokens.size() - 1].is(tok::r_brace);
  }

  bool isHashOrHashHashOperand() {
    return Tokens[TokenIndex - 1].isOneOf(tok::hash, tok::hashhash) ||
           Tokens[TokenIndex + 1].is(tok::hashhash);
  }

  void checkMacroBodyCompliant() {
    if (IsMacroBodyEnclosedInBraces) {
      return;
    }
    if (!IsMacroBodyCompliant) {
      return;
    }
    if (!Tokens[0].isOneOf(tok::l_paren, tok::l_brace)) {
      IsMacroBodyCompliant = false;
      return;
    }
    if (Tokens[TokenIndex].is(tok::l_paren)) {
      ++ParenDepth;
    }
    if (Tokens[TokenIndex].is(tok::r_paren)) {
      --ParenDepth;
    }
    if (0U == ParenDepth) {
      IsMacroBodyCompliant = (TokenIndex == Tokens.size() - 1);
    }
    return;
  }
};

class MacroDefCallback : public PPCallbacks {
public:
  MacroDefCallback(MacroBodyCheck *Checker, const SourceManager &SM)
      : Checker(Checker), SM(SM) {}

  void MacroDefined(const Token &MacroNameTok,
                    const MacroDirective *MD) override {
    auto *MI = MD->getMacroInfo();
    if (nullptr == MI || !MI->isFunctionLike() || 0U == MI->getNumParams() ||
        MI->tokens().empty()) {
      return;
    }
    MacroParenComplianceChecker MPCC = MacroParenComplianceChecker(MI);
    auto ParamTokenWithoutParentheses = MPCC.getParamTokenWithoutParentheses();
    if (!MPCC.isMacroBodyCompliant() || !ParamTokenWithoutParentheses.empty()) {
      Checker->diag(MD->getLocation(), "warning");
      if (!MPCC.isMacroBodyCompliant()) {
        reportMacroBodyWithoutParentheses(MPCC.getMacroBodyRange());
      }
      reportParamTokens(ParamTokenWithoutParentheses);
    }
    return;
  }

private:
  void reportMacroBodyWithoutParentheses(SourceRange Range) {
    if (Range.isInvalid()) {
      return;
    }
    Range.getBegin().dump(SM);
    Range.getEnd().dump(SM);
    Checker->diag(Range.getBegin(), MacroBodyNote, DiagnosticIDs::Note)
        << Range << FixItHint::CreateInsertion(Range.getBegin(), LParen)
        << FixItHint::CreateInsertion(Range.getEnd(), RParen);
    return;
  }

  void reportParamTokens(std::vector<Token> &ParamTokenWithoutParentheses) {
    return;
    for (auto ParamToken : ParamTokenWithoutParentheses) {
      Checker->diag(ParamToken.getLocation(), ParameterNote,

                    DiagnosticIDs::Note)
          << FixItHint::CreateInsertion(ParamToken.getLocation(), LParen)
          << FixItHint::CreateInsertion(ParamToken.getEndLoc(), RParen);
    }
    return;
  }

private:
  MacroBodyCheck *Checker;
  const SourceManager &SM;
};
} // namespace

MacroBodyCheck::MacroBodyCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

MacroBodyCheck::~MacroBodyCheck() = default;

void MacroBodyCheck::registerPPCallbacks(const SourceManager &SM,
                                         Preprocessor *PP, Preprocessor *) {
  PP->addPPCallbacks(std::make_unique<MacroDefCallback>(this, SM));
}

} // namespace clang::tidy::misc
