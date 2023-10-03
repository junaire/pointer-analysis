#pragma once

#include "stmt.h"
#include <memory>
#include <string>
#include <vector>

struct Function {
  std::string name;
  std::vector<std::unique_ptr<Stmt>> stmts;

#define CREATE(STMT)                                                           \
  do {                                                                         \
    auto *ptr = STMT.get();                                                    \
    stmts.push_back(std::move(STMT));                                          \
    return ptr;                                                                \
  } while (0)

  VarStmt *createVar(std::string name) {
    auto stmt = std::make_unique<VarStmt>(Kind::Var, name);
    CREATE(stmt);
  }

  AllocStmt *createAlloc(VarStmt *target) {
    auto stmt = std::make_unique<AllocStmt>(Kind::Alloc, target);
    CREATE(stmt);
  }

  AddrOfStmt *createAddrOf(VarStmt *target, VarStmt *operand) {
    auto stmt = std::make_unique<AddrOfStmt>(Kind::AddrOf, target, operand);
    CREATE(stmt);
  }

  CopyStmt *createCopy(VarStmt *target, VarStmt *operand) {
    auto stmt = std::make_unique<CopyStmt>(Kind::Copy, target, operand);
    CREATE(stmt);
  }

  LoadStmt *createLoad(VarStmt *target, VarStmt *operand) {
    auto stmt = std::make_unique<LoadStmt>(Kind::Load, target, operand);
    CREATE(stmt);
  }

  StoreStmt *createStore(VarStmt *target, VarStmt *operand) {
    auto stmt = std::make_unique<StoreStmt>(Kind::Store, target, operand);
    CREATE(stmt);
  }

  explicit Function(std::string n) : name(std::move(n)) {}

  void dump() {
    std::cout << "function @" << name << " {\n";
    for (const std::unique_ptr<Stmt> &stmt : stmts) {
      if (stmt->kind == Kind::Var)
        continue;
      std::cout << "  ";
      stmt->dump();
    }
    std::cout << "}\n";
  }

  std::vector<Stmt *> getStmts() {
    std::vector<Stmt *> ret;
    for (const std::unique_ptr<Stmt> &stmt : stmts)
      ret.push_back(stmt.get());
    return ret;
  }

  std::vector<LoadStmt *> getLoads(Stmt *s) {
    std::vector<LoadStmt *> ret;
    for (const std::unique_ptr<Stmt> &stmt : stmts)
      if (stmt->kind == Kind::Load) {
        auto *load = stmt->as<LoadStmt>();
        assert(load && "Not a load");
        if (load->operand == s)
          ret.push_back(load);
      }
    return ret;
  }

  std::vector<StoreStmt *> getStores(Stmt *s) {
    std::vector<StoreStmt *> ret;
    for (const std::unique_ptr<Stmt> &stmt : stmts)
      if (stmt->kind == Kind::Store) {
        auto *store = stmt->as<StoreStmt>();
        assert(store && "Not a store");
        if (store->target == s)
          ret.push_back(store);
      }
    return ret;
  }
};
