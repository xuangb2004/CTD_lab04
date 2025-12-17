/* * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 */
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"
#include "scanner.h"
#include "parser.h"
#include "error.h"
#include "debug.h"
#include "symtab.h" // <--- QUAN TRỌNG: Phải có dòng này để dùng lookupObject

Token *currentToken;
Token *lookAhead;

extern Type* intType;
extern Type* charType;
extern SymTab* symtab;

void scan(void) {
  Token* tmp = currentToken;
  currentToken = lookAhead;
  lookAhead = getValidToken();
  free(tmp);
}

void eat(TokenType tokenType) {
  if (lookAhead->tokenType == tokenType) {
    scan();
  } else missingToken(tokenType, lookAhead->lineNo, lookAhead->colNo);
}

void compileProgram(void) {
  eat(KW_PROGRAM);
  eat(TK_IDENT);
  
  Object* program = createProgramObject(currentToken->string);
  enterBlock(program->progAttrs->scope);

  eat(SB_SEMICOLON);
  compileBlock();
  eat(SB_PERIOD);

  exitBlock();
}

void compileBlock(void) {
  Object* constObj;
  ConstantValue* constValue;

  if (lookAhead->tokenType == KW_CONST) {
    eat(KW_CONST);

    do {
      eat(TK_IDENT);
      
      // Check if a constant identifier is fresh in the block
      if (findObject(symtab->currentScope->objList, currentToken->string) != NULL) {
        error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
      }

      constObj = createConstantObject(currentToken->string);
      
      eat(SB_EQ);
      constValue = compileConstant();
      constObj->constAttrs->value = constValue;
      declareObject(constObj);
      
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);

    compileBlock2();
  } 
  else compileBlock2();
}

void compileBlock2(void) {
  Object* typeObj;
  Type* actualType;

  if (lookAhead->tokenType == KW_TYPE) {
    eat(KW_TYPE);

    do {
      eat(TK_IDENT);
      
      // Check if a type identifier is fresh in the block
      if (findObject(symtab->currentScope->objList, currentToken->string) != NULL) {
        error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
      }

      typeObj = createTypeObject(currentToken->string);
      
      eat(SB_EQ);
      actualType = compileType();
      typeObj->typeAttrs->actualType = actualType;
      declareObject(typeObj);
      
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);

    compileBlock3();
  } 
  else compileBlock3();
}

void compileBlock3(void) {
  Object* varObj;
  Type* varType;

  if (lookAhead->tokenType == KW_VAR) {
    eat(KW_VAR);

    do {
      eat(TK_IDENT);
      
      // Check if a variable identifier is fresh in the block
      if (findObject(symtab->currentScope->objList, currentToken->string) != NULL) {
        error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
      }

      varObj = createVariableObject(currentToken->string);
      
      eat(SB_COLON);
      varType = compileType();
      varObj->varAttrs->type = varType;
      declareObject(varObj);
      
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);

    compileBlock4();
  } 
  else compileBlock4();
}

void compileBlock4(void) {
  compileSubDecls();
  compileBlock5();
}

void compileBlock5(void) {
  eat(KW_BEGIN);
  compileStatements();
  eat(KW_END);
}

void compileSubDecls(void) {
  while ((lookAhead->tokenType == KW_FUNCTION) || (lookAhead->tokenType == KW_PROCEDURE)) {
    if (lookAhead->tokenType == KW_FUNCTION)
      compileFuncDecl();
    else compileProcDecl();
  }
}

void compileFuncDecl(void) {
  Object* funcObj;
  Type* returnType;

  eat(KW_FUNCTION);
  eat(TK_IDENT);
  
  // Check if a function identifier is fresh in the block
  if (findObject(symtab->currentScope->objList, currentToken->string) != NULL) {
    error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
  }

  funcObj = createFunctionObject(currentToken->string);
  declareObject(funcObj);
  enterBlock(funcObj->funcAttrs->scope);
  
  compileParams();
  
  eat(SB_COLON);
  returnType = compileBasicType();
  funcObj->funcAttrs->returnType = returnType;
  
  eat(SB_SEMICOLON);
  compileBlock();
  eat(SB_SEMICOLON);

  exitBlock();
}

void compileProcDecl(void) {
  Object* procObj;

  eat(KW_PROCEDURE);
  eat(TK_IDENT);

  // Check if a procedure identifier is fresh in the block
  if (findObject(symtab->currentScope->objList, currentToken->string) != NULL) {
    error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
  }

  procObj = createProcedureObject(currentToken->string);
  declareObject(procObj);
  enterBlock(procObj->procAttrs->scope);

  compileParams();

  eat(SB_SEMICOLON);
  compileBlock();
  eat(SB_SEMICOLON);

  exitBlock();
}

ConstantValue* compileUnsignedConstant(void) {
  ConstantValue* constValue = NULL;
  Object* obj;

  switch (lookAhead->tokenType) {
  case TK_NUMBER:
    eat(TK_NUMBER);
    constValue = makeIntConstant(currentToken->value);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    // Tra cứu đối tượng hằng số
    obj = lookupObject(currentToken->string);
    if (obj == NULL)
        error(ERR_UNDECLARED_CONSTANT, currentToken->lineNo, currentToken->colNo);
    if (obj->kind != OBJ_CONSTANT)
        error(ERR_INVALID_CONSTANT, currentToken->lineNo, currentToken->colNo);
    
    constValue = duplicateConstantValue(obj->constAttrs->value);
    break;
  case TK_CHAR:
    eat(TK_CHAR);
    constValue = makeCharConstant(currentToken->string[0]);
    break;
  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return constValue;
}

ConstantValue* compileConstant(void) {
  ConstantValue* constValue;

  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    constValue = compileConstant2();
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    constValue = compileConstant2();
    constValue->intValue = - constValue->intValue;
    break;
  case TK_CHAR:
    eat(TK_CHAR);
    constValue = makeCharConstant(currentToken->string[0]);
    break;
  default:
    constValue = compileConstant2();
    break;
  }
  return constValue;
}

ConstantValue* compileConstant2(void) {
  ConstantValue* constValue = NULL;
  Object* obj;

  switch (lookAhead->tokenType) {
  case TK_NUMBER:
    eat(TK_NUMBER);
    constValue = makeIntConstant(currentToken->value);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    obj = lookupObject(currentToken->string);
    if (obj == NULL)
        error(ERR_UNDECLARED_CONSTANT, currentToken->lineNo, currentToken->colNo);
    if (obj->kind != OBJ_CONSTANT)
        error(ERR_INVALID_CONSTANT, currentToken->lineNo, currentToken->colNo);
    
    constValue = duplicateConstantValue(obj->constAttrs->value);
    break;
  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return constValue;
}

Type* compileType(void) {
  Type* type = NULL;
  Type* elementType;
  int arraySize;
  Object* obj;

  switch (lookAhead->tokenType) {
  case KW_INTEGER: 
    eat(KW_INTEGER);
    type = makeIntType();
    break;
  case KW_CHAR: 
    eat(KW_CHAR); 
    type = makeCharType();
    break;
  case KW_ARRAY:
    eat(KW_ARRAY);
    eat(SB_LSEL);
    eat(TK_NUMBER);
    arraySize = currentToken->value;
    eat(SB_RSEL);
    eat(KW_OF);
    elementType = compileType();
    type = makeArrayType(arraySize, elementType);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    obj = lookupObject(currentToken->string);
    if (obj == NULL)
        error(ERR_UNDECLARED_TYPE, currentToken->lineNo, currentToken->colNo);
    if (obj->kind != OBJ_TYPE)
        error(ERR_INVALID_TYPE, currentToken->lineNo, currentToken->colNo);
    
    type = duplicateType(obj->typeAttrs->actualType);
    break;
  default:
    error(ERR_INVALID_TYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return type;
}

Type* compileBasicType(void) {
  Type* type = NULL;

  switch (lookAhead->tokenType) {
  case KW_INTEGER: 
    eat(KW_INTEGER); 
    type = makeIntType();
    break;
  case KW_CHAR: 
    eat(KW_CHAR); 
    type = makeCharType();
    break;
  default:
    error(ERR_INVALID_BASICTYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return type;
}

void compileParams(void) {
  if (lookAhead->tokenType == SB_LPAR) {
    eat(SB_LPAR);
    compileParam();
    while (lookAhead->tokenType == SB_SEMICOLON) {
      eat(SB_SEMICOLON);
      compileParam();
    }
    eat(SB_RPAR);
  }
}

void compileParam(void) {
  Object* param;
  Type* type;
  enum ParamKind paramKind;

  switch (lookAhead->tokenType) {
  case TK_IDENT:
    paramKind = PARAM_VALUE;
    break;
  case KW_VAR:
    eat(KW_VAR);
    paramKind = PARAM_REFERENCE;
    break;
  default:
    error(ERR_INVALID_PARAMETER, lookAhead->lineNo, lookAhead->colNo);
    break;
  }

  eat(TK_IDENT);
  // Check if a parameter identifier is fresh in the block
  if (findObject(symtab->currentScope->objList, currentToken->string) != NULL) {
    error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
  }

  param = createParameterObject(currentToken->string, paramKind, symtab->currentScope->owner);
  eat(SB_COLON);
  type = compileBasicType();
  param->paramAttrs->type = type;
  declareObject(param);
}

void compileStatements(void) {
  compileStatement();
  while (lookAhead->tokenType == SB_SEMICOLON) {
    eat(SB_SEMICOLON);
    compileStatement();
  }
}

void compileStatement(void) {
  switch (lookAhead->tokenType) {
  case TK_IDENT:
    compileAssignSt();
    break;
  case KW_CALL:
    compileCallSt();
    break;
  case KW_BEGIN:
    compileGroupSt();
    break;
  case KW_IF:
    compileIfSt();
    break;
  case KW_WHILE:
    compileWhileSt();
    break;
  case KW_FOR:
    compileForSt();
    break;
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
    break;
  default:
    error(ERR_INVALID_STATEMENT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
}

void compileLValue(void) {
  Object* var;

  eat(TK_IDENT);
  var = lookupObject(currentToken->string);
  if (var == NULL)
    error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);

  switch (var->kind) {
  case OBJ_VARIABLE:
  case OBJ_PARAMETER:
    break;
  case OBJ_FUNCTION:
    if (var != symtab->currentScope->owner)
       error(ERR_INVALID_LVALUE, currentToken->lineNo, currentToken->colNo);
    break;
  default:
    error(ERR_INVALID_LVALUE, currentToken->lineNo, currentToken->colNo);
  }

  if (var->kind == OBJ_VARIABLE)
    compileIndexes();
}

void compileAssignSt(void) {
  compileLValue();
  eat(SB_ASSIGN);
  compileExpression();
}

void compileCallSt(void) {
  Object* proc;
  eat(KW_CALL);
  eat(TK_IDENT);
  
  proc = lookupObject(currentToken->string);
  if (proc == NULL)
    error(ERR_UNDECLARED_PROCEDURE, currentToken->lineNo, currentToken->colNo);
  if (proc->kind != OBJ_PROCEDURE)
    error(ERR_INVALID_PROCEDURE, currentToken->lineNo, currentToken->colNo);

  compileArguments();
}

void compileGroupSt(void) {
  eat(KW_BEGIN);
  compileStatements();
  eat(KW_END);
}

void compileIfSt(void) {
  eat(KW_IF);
  compileCondition();
  eat(KW_THEN);
  compileStatement();
  if (lookAhead->tokenType == KW_ELSE) 
    compileElseSt();
}

void compileElseSt(void) {
  eat(KW_ELSE);
  compileStatement();
}

void compileWhileSt(void) {
  eat(KW_WHILE);
  compileCondition();
  eat(KW_DO);
  compileStatement();
}

void compileForSt(void) {
  Object* var;
  eat(KW_FOR);
  eat(TK_IDENT);

  var = lookupObject(currentToken->string);
  if (var == NULL)
    error(ERR_UNDECLARED_VARIABLE, currentToken->lineNo, currentToken->colNo);
  if (var->kind != OBJ_VARIABLE || var->varAttrs->type->typeClass != TP_INT)
    error(ERR_INVALID_VARIABLE, currentToken->lineNo, currentToken->colNo);

  eat(SB_ASSIGN);
  compileExpression();
  eat(KW_TO);
  compileExpression();
  eat(KW_DO);
  compileStatement();
}

void compileArgument(void) {
  compileExpression();
}

void compileArguments(void) {
  switch (lookAhead->tokenType) {
  case SB_LPAR:
    eat(SB_LPAR);
    compileArgument();
    while (lookAhead->tokenType == SB_COMMA) {
      eat(SB_COMMA);
      compileArgument();
    }
    eat(SB_RPAR);
    break;
  default:
    break;
  }
}

void compileCondition(void) {
  compileExpression();
  switch (lookAhead->tokenType) {
  case SB_EQ: eat(SB_EQ); break;
  case SB_NEQ: eat(SB_NEQ); break;
  case SB_LE: eat(SB_LE); break;
  case SB_LT: eat(SB_LT); break;
  case SB_GE: eat(SB_GE); break;
  case SB_GT: eat(SB_GT); break;
  default:
    error(ERR_INVALID_COMPARATOR, lookAhead->lineNo, lookAhead->colNo);
  }
  compileExpression();
}

void compileExpression(void) {
  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    compileExpression2();
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    compileExpression2();
    break;
  default:
    compileExpression2();
  }
}

void compileExpression2(void) {
  compileTerm();
  compileExpression3();
}

void compileExpression3(void) {
  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    compileTerm();
    compileExpression3();
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    compileTerm();
    compileExpression3();
    break;
  default:
    break;
  }
}

void compileTerm(void) {
  compileFactor();
  compileTerm2();
}

void compileTerm2(void) {
  switch (lookAhead->tokenType) {
  case SB_TIMES:
    eat(SB_TIMES);
    compileFactor();
    compileTerm2();
    break;
  case SB_SLASH:
    eat(SB_SLASH);
    compileFactor();
    compileTerm2();
    break;
  default:
    break;
  }
}

void compileFactor(void) {
  Object* obj;
  switch (lookAhead->tokenType) {
  case TK_NUMBER:
  case TK_CHAR:
    eat(lookAhead->tokenType);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    obj = lookupObject(currentToken->string);
    if (obj == NULL)
        error(ERR_UNDECLARED_IDENT, currentToken->lineNo, currentToken->colNo);

    switch (obj->kind) {
    case OBJ_CONSTANT:
      break;
    case OBJ_VARIABLE:
      compileIndexes();
      break;
    case OBJ_PARAMETER:
      break;
    case OBJ_FUNCTION:
      compileArguments();
      break;
    default:
      error(ERR_INVALID_FACTOR, currentToken->lineNo, currentToken->colNo);
    }
    break;
  default:
    error(ERR_INVALID_FACTOR, lookAhead->lineNo, lookAhead->colNo);
  }
}

void compileIndexes(void) {
  while (lookAhead->tokenType == SB_LSEL) {
    eat(SB_LSEL);
    compileExpression();
    eat(SB_RSEL);
  }
}

int compile(char *fileName) {
  if (openInputStream(fileName) == IO_ERROR)
    return IO_ERROR;

  currentToken = NULL;
  lookAhead = getValidToken();

  initSymTab();
  compileProgram();
  printObject(symtab->program,0);
  cleanSymTab();

  free(currentToken);
  free(lookAhead);
  closeInputStream();
  return IO_SUCCESS;
}