#include "anderson.h"
#include "function.h"
#include "stmt.h"
#include <cassert>

static void test1() {
  Function f("test1");
  // Declarations.
  VarStmt *x = f.createVar("x");
  VarStmt *y = f.createVar("y");
  VarStmt *z = f.createVar("z");
  VarStmt *p = f.createVar("p");
  VarStmt *q = f.createVar("q");

  // p = alloc;
  f.createAlloc(p);
  // x = y;
  f.createCopy(x, y);
  // x = z;
  f.createCopy(x, z);
  // *p = z;
  f.createStore(p, z);
  // p = q;
  f.createCopy(p, q);
  // q = &y;
  f.createAddrOf(q, y);
  // x = *p;
  f.createLoad(x, p);
  // p = &z;
  f.createAddrOf(p, z);

  PTAResult pts = andersonPTA(f);

  assert(pts["p"][0].starts_with("alloc"));
  assert(pts["p"][1] == "z");
  assert(pts["p"][2] == "y");

  assert(pts["q"][0] == "y");
  std::cout << "OK\n";
}

static void test2() {
  Function f("test2");
  // Declarations.
  VarStmt *i = f.createVar("i");
  VarStmt *j = f.createVar("j");
  VarStmt *k = f.createVar("k");
  VarStmt *a = f.createVar("a");
  VarStmt *b = f.createVar("b");
  VarStmt *c = f.createVar("c");
  VarStmt *p = f.createVar("p");
  VarStmt *q = f.createVar("q");

  // a = &i;
  f.createAddrOf(a, i);
  // b = &k;
  f.createAddrOf(b, k);
  // a = &j;
  f.createAddrOf(a, j);
  // p = &a;
  f.createAddrOf(p, a);
  // q = &b;
  f.createAddrOf(q, b);
  // p = q;
  f.createCopy(p, q);
  // c = *q;
  f.createLoad(c, q);

  PTAResult pts = andersonPTA(f);
  assert(pts["a"][0] == "i");
  assert(pts["a"][1] == "j");

  assert(pts["b"][0] == "k");

  assert(pts["p"][0] == "a");
  assert(pts["p"][1] == "b");

  assert(pts["q"][0] == "b");
  std::cout << "OK\n";
}

int main() {
  test1();
  test2();
}
