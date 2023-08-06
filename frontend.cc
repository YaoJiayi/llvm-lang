#include <string>
#include <iostream>
#include <vector>
#include <memory>

//"unknown tokens" are represented by their ascii code
enum Token { //why negative? bc ascii values are al positive
    tok_eof = -1, //end of file
    tok_def = -2, //func def
    tok_extern = -3, //extern
    tok_identifier = -4, //identifier
    tok_number = -5 //num
};

static std::string IdentifierStr; //Fill in if tok_identifier
static double NumVal; //Fill in if tok_number

static int gettok(){
    static int LastChar = ' '; //why int? ascii value

    while (isspace(LastChar)){ //skip spaces
        LastChar = getchar();
    }

    //parser identifier
    if (isalpha(LastChar)){//identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while(isalnum(LastChar=getchar())){//get the complete token
            IdentifierStr += LastChar;
        }

        if (IdentifierStr == "def"){
            return tok_def;
        }
        if (IdentifierStr == "extern"){
            return tok_extern;
        }
        return tok_identifier;
    }

    //parse num
    if (isdigit(LastChar) || LastChar == '.'){
        std::string NumStr;
        do{
            NumStr += LastChar;
            LastChar = getchar();
        }while (isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(NumStr.c_str(),0); //no err handling for 1.2.34
        return tok_number;
    }

    //parsr comments
    if (LastChar == '#'){
       do{
        LastChar = getchar();
        }while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    }

    //parse EOF
    if (LastChar == EOF){
        return tok_eof;
    }

    //Otherwise, just return the character as its ascii value
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;   
}

/*
int main(){
    while(true){
        int tok = gettok();
        std::cout << "got token: " << tok << std::endl;
    }
}
*/

class ExprAST{
public:
    virtual ~ExprAST() {}
};

class NumberExprAST : public ExprAST{
    double Val;

public:
    NumberExprAST(double Val) : Val(Val){}
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
    : Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
    : Proto(std::move(Proto)), Body(std::move(Body)) {}
};


static int CurTok;
static int getNextToken() {
    return CurTok = gettok();
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr; // return nullptr to align with the return type of parserer functions
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}


static std::unique_ptr<ExprAST> ParseExpression();

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr(){
    getNextToken(); //eat (.
    auto V = ParseExpression();
    if (!V){
        return nullptr;
    }
    if (CurTok != ')'){
        return LogError("expected ')'");
    getNextToken(); //eat ).
    }
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // eat identifier

    if (CurTok != '('){
        return std::make_unique<VariableExprAST>(IdName);
    }

    //func call
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression()){
                Args.push_back(std::move(Arg));
            } else {
                return nullptr;
            }

            if (CurTok == ')'){
                break;
            }

            if (CurTok != ','){
                return LogError("Expected ')' or ',' in argument list");
            }
            getNextToken();
        }
    }

    getNextToken(); // eat ')'

    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

static int GetTokPrecedence(){
  switch (CurTok) {
    case '<':
    case '>':
      return 10;
    case '+':
    case '-':
      return 20;
    case '*':
    case '/':
      return 40;
    default:
      return -1;
  }
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);

/// expression
///   ::= primary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS){
    return nullptr;
  }

  return ParseBinOpRHS(0, std::move(LHS));
}

/// binoprhs
///   ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS){
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec){
      return LHS;
    }

    //we know this is a binop
    int BinOp = CurTok;
    getNextToken();

    //parse the primary expression after binary operator
    auto RHS = ParsePrimary();
    if (!RHS){
      return nullptr;
    }

    int NextPrec = GetTokPrecedence();

    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(NextPrec+1, std::move(RHS));
      if (!RHS){
        return nullptr;
      }
    }

    LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  
  }
}

/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype(){
  if (CurTok != tok_identifier){
    return LogErrorP("Expected function name in prototype");
  }

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '('){
    return LogErrorP("Expected '(' in prototype");
  }

  // Read list of argument names
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier){
    ArgNames.push_back(IdentifierStr);
  }
  if (CurTok != ')'){
    return LogErrorP("Expected ')' in prototype");
  }

  getNextToken(); // eat ')'

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); //eat def
  auto Proto = ParsePrototype();
  if (!Proto) {
    return nullptr;
  }

  if (auto E = ParseExpression()){
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}


int main() {

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}