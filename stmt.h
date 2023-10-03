#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

enum class Kind { Var, Alloc, AddrOf, Copy, Load, Store };

inline std::string_view getKindAsString(Kind k) {
#define CASE(X)                                                                \
  case Kind::X:                                                                \
    return #X;
  switch (k) {
  default:
    assert(false && "Unknown Stmt kind");

    CASE(Var)
    CASE(Alloc)
    CASE(AddrOf)
    CASE(Copy)
    CASE(Load)
    CASE(Store)
  }
}

struct Stmt {
  Kind kind;
  explicit Stmt(Kind k) : kind(k) {}
  virtual ~Stmt() = default;
  virtual void dump() = 0;

  template <typename T> bool is() { return dynamic_cast<T *>(this) != nullptr; }

  template <typename T> T *as() { return dynamic_cast<T *>(this); }
};

struct VarStmt : Stmt {
  std::string name;
  VarStmt(Kind k, std::string n) : Stmt(k), name(std::move(n)) {}
  void dump() override { std::cout << "var " << name << "\n"; }
};

struct AllocStmt : Stmt {
  VarStmt *target;
  AllocStmt(Kind k, VarStmt *t) : Stmt(k), target(t) {}
  void dump() override { std::cout << target->name << " = alloc\n"; }
};

struct AddrOfStmt : Stmt {
  VarStmt *target;
  VarStmt *operand;
  AddrOfStmt(Kind k, VarStmt *t, VarStmt *op)
      : Stmt(k), target(t), operand(op) {}
  void dump() override {
    std::cout << target->name << " = &" << operand->name << "\n";
  }
};

struct CopyStmt : Stmt {
  VarStmt *target;
  VarStmt *operand;
  CopyStmt(Kind k, VarStmt *t, VarStmt *op) : Stmt(k), target(t), operand(op) {}
  void dump() override {
    std::cout << target->name << " = " << operand->name << "\n";
  }
};

struct LoadStmt : Stmt {
  VarStmt *target;
  VarStmt *operand;
  LoadStmt(Kind k, VarStmt *t, VarStmt *op) : Stmt(k), target(t), operand(op) {}
  void dump() override {
    std::cout << target->name << " = *" << operand->name << "\n";
  }
};

struct StoreStmt : Stmt {
  VarStmt *target;
  VarStmt *operand;
  StoreStmt(Kind k, VarStmt *t, VarStmt *op)
      : Stmt(k), target(t), operand(op) {}
  void dump() override {
    std::cout << "*" << target->name << " = " << operand->name << "\n";
  }
};
