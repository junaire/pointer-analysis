#pragma once

#include "function.h"
#include "stmt.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

struct Node {
  Stmt *stmt;
  std::vector<Node *> pts;
  std::vector<Node *> succs;

  explicit Node(VarStmt *var) : stmt(var) {}

  explicit Node(AllocStmt *alloc) : stmt(alloc) {}

  void addPointsTo(Node *pt) { pts.push_back(pt); }

  void addSucc(Node *node) { succs.push_back(node); }

  std::string getName() {
    if (auto *var = stmt->as<VarStmt>())
      return var->name;
    if (auto *alloc = stmt->as<AllocStmt>()) {
      std::ostringstream ss;
      ss << alloc;
      return "alloc-" + ss.str();
    }
    assert(false && "Cannot use other stmt");
  }

  void dump() {
    if (!pts.empty()) {
      std::cout << "[[" << getName() << "]] pts {";
      for (Node *pt : pts)
        std::cout << pt->getName() << " ";
      std::cout << "} ";
    } else {
      std::cout << "[[" << getName() << "]] ";
    }

    if (succs.empty()) {
      std::cout << "\n";
    } else {
      std::cout << "succ {";
      for (Node *succ : succs)
        std::cout << succ->getName() << " ";
      std::cout << "}\n";
    }
  }
};

struct ConstraintGraph {
  std::vector<std::unique_ptr<Node>> nodes;
  std::unordered_map<Stmt *, Node *> mapping;
  Function &fn;

  explicit ConstraintGraph(Function &f) : fn(f) {}

  template <typename T> Node *getOrCreateNode(T *stmt) {
    if (auto it = mapping.find(stmt); it != mapping.end())
      return it->second;
    nodes.push_back(std::make_unique<Node>(stmt));
    mapping[stmt] = nodes.back().get();
    return nodes.back().get();
  }

  bool addEdge(Node *src, Node *dst) {
    auto it = std::find(src->succs.begin(), src->succs.end(), dst);
    if (it != src->succs.end())
      return false;
    src->addSucc(dst);
    return true;
  }

  void dump() {
    for (auto &node : nodes)
      node->dump();
    std::cout << "\n";
  }

  void dumpDot(const std::string &filepath) {
    std::string file = filepath + "/" + fn.name + ".dot";
    std::ofstream out(file);

    out << "digraph " << fn.name << "{\n";
    out << "  node [shape=box, style=filled]\n";

    for (std::unique_ptr<Node> &node : nodes)
      out << "  \"" << node->getName() << "\"\n";

    for (std::unique_ptr<Node> &node : nodes)
      for (Node *succ : node->succs)
        out << "  \"" << node->getName() << "\" -> \"" << succ->getName()
            << "\" [color=\"blue\"]\n";

    out << "}\n";
  }
};

struct WorkList {
  std::deque<Node *> q;

  void push(Node *node) {
    if (std::find(q.begin(), q.end(), node) == q.end())
      q.push_back(node);
  }

  Node *pop() {
    assert(!q.empty() && "Cannot pop an empty worklist");
    Node *node = q.front();
    q.pop_front();
    return node;
  }

  bool empty() { return q.empty(); }

  void dump() {
    std::cout << "[";
    for (Node *node : q)
      std::cout << node->getName() << " ";
    std::cout << "]\n";
  }
};

inline std::pair<ConstraintGraph, WorkList>
createConstraintGraph(Function &fn) {
  ConstraintGraph g(fn);
  WorkList wl;

  for (Stmt *stmt : fn.getStmts()) {
    switch (stmt->kind) {
    case Kind::Var: {
      VarStmt *var = stmt->as<VarStmt>();
      g.getOrCreateNode(var);
      break;
    }
    case Kind::AddrOf: {
      AddrOfStmt *addr = stmt->as<AddrOfStmt>();
      Node *node = g.getOrCreateNode(addr->target);
      wl.push(node);
      node->addPointsTo(g.getOrCreateNode(addr->operand));
      break;
    }
    case Kind::Alloc: {
      AllocStmt *alloc = stmt->as<AllocStmt>();
      Node *node = g.getOrCreateNode(alloc->target);
      wl.push(node);
      node->addPointsTo(g.getOrCreateNode(alloc));
      break;
    }
    case Kind::Copy: {
      CopyStmt *copy = stmt->as<CopyStmt>();
      Node *node = g.getOrCreateNode(copy->operand);
      node->addSucc(g.getOrCreateNode(copy->target));
      break;
    }
    default:
      continue;
    }
  }
  return {std::move(g), std::move(wl)};
}

inline void solveConstraint(ConstraintGraph &graph, WorkList &worklist) {
  while (!worklist.empty()) {
    Node *v = worklist.pop();
    // std::cout << "Select " << v->getName() << "\n";

    for (Node *a : v->pts) {
      for (LoadStmt *load : graph.fn.getLoads(v->stmt)) {
        Node *p = graph.getOrCreateNode(load->target);
        if (graph.addEdge(a, p))
          worklist.push(a);
      }
      for (StoreStmt *store : graph.fn.getStores(v->stmt)) {
        Node *p = graph.getOrCreateNode(store->operand);
        if (graph.addEdge(p, a))
          worklist.push(p);
      }
    }
    for (Node *q : v->succs) {
      std::vector<Node *> pts = q->pts;
      for (Node *pt : v->pts)
        pts.push_back(pt);
      if (pts != q->pts) {
        q->pts = std::move(pts);
        worklist.push(q);
      }
    }
    // std::cout << "WorkList ";
    // worklist.dump();
  }
}

using PTAResult = std::map<std::string, std::vector<std::string>>;
inline PTAResult andersonPTA(Function &f) {
  auto [g, q] = createConstraintGraph(f);

  // TODO: Optimize the graph.

  solveConstraint(g, q);

  PTAResult pts_info;
  for (std::unique_ptr<Node> &node : g.nodes) {
    if (node->pts.empty())
      continue;
    for (Node *pt : node->pts)
      pts_info[node->getName()].push_back(pt->getName());
  }

  return pts_info;
}
