#pragma once

#include "function.h"
#include "stmt.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

struct Node {
  std::vector<Node *> pts;
  std::vector<Node *> succs;
  Stmt *stmt;
  bool is_ref = false;

  explicit Node(VarStmt *var) : stmt(var) {}

  explicit Node(VarStmt *var, bool ir) : stmt(var), is_ref(ir) {}

  explicit Node(AllocStmt *alloc, [[maybe_unused]] bool ir) : stmt(alloc) {}

  void addPointsTo(Node *pt) { pts.push_back(pt); }

  void addSucc(Node *node) { succs.push_back(node); }

  std::string getName() {
    if (auto *var = stmt->as<VarStmt>()) {
      if (is_ref)
        return "*" + var->name;
      return var->name;
    }
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

  size_t size() { return nodes.size(); }

  Node *getNode(size_t i) { return nodes[i].get(); }

  std::vector<Node *> getNodes() {
    std::vector<Node *> view;
    for (std::unique_ptr<Node> &node : nodes)
      view.push_back(node.get());
    return view;
  }

  template <typename T> Node *getOrCreateNode(T *stmt, bool is_ref = false) {
    if (auto it = mapping.find(stmt); it != mapping.end())
      return it->second;
    nodes.push_back(std::make_unique<Node>(stmt, is_ref));
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

using SCC = std::vector<Node *>;

class TarjanSCCFinder {
  ConstraintGraph &graph;
  std::set<Node *> visited;
  std::map<Node *, int> ids;
  std::map<Node *, int> low;
  std::vector<Node *> stack;
  std::vector<SCC> sccs;
  int id = 0;

  void dfs(Node *node) {
    // Mark it as visited.
    visited.insert(node);
    // Push it to the seen stack.
    stack.push_back(node);
    // Give the node an id and low value.
    ids[node] = id;
    low[node] = id;
    id += 1;

    // Visiting its succs.
    for (Node *succ : node->succs) {
      // Depth first.
      if (!visited.contains(succ))
        dfs(succ);

      // If the succ is in the stack.
      if (auto it = std::find(stack.begin(), stack.end(), succ);
          it != stack.end())
        low[node] = std::min(low[node], low[succ]);
    }

    // We form a SCC.
    if (ids[node] == low[node]) {
      SCC scc;
      while (true) {
        scc.push_back(stack.back());
        if (stack.back() == node) {
          stack.pop_back();
          break;
        }
        low[stack.back()] = ids[node];
        stack.pop_back();
      }
      sccs.push_back(std::move(scc));
    }
  }

public:
  explicit TarjanSCCFinder(ConstraintGraph &g) : graph(g) {}

  std::vector<SCC> run() {
    for (Node *node : graph.getNodes())
      if (!visited.contains(node))
        dfs(node);
    return sccs;
  }
};

inline std::vector<SCC> findSCC(ConstraintGraph &g) {
  TarjanSCCFinder finder(g);
  return finder.run();
}

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

using OptimizationInfo = std::vector<std::pair<Node *, Node *>>;

inline OptimizationInfo optimizeConstraintGraph(ConstraintGraph &cg) {
  OptimizationInfo info;

  ConstraintGraph g(cg.fn);
  for (Stmt *stmt : g.fn.getStmts()) {
    switch (stmt->kind) {
    // case Kind::Var: {
    //   VarStmt *var = stmt->as<VarStmt>();
    //   g.getOrCreateNode(var);
    //   break;
    // }
    case Kind::Copy: {
      CopyStmt *copy = stmt->as<CopyStmt>();
      Node *node = g.getOrCreateNode(copy->operand);
      node->addSucc(g.getOrCreateNode(copy->target));
      break;
    }
    case Kind::Load: {
      LoadStmt *load = stmt->as<LoadStmt>();
      Node *node = g.getOrCreateNode(load->operand, /*is_ref=*/true);
      node->addSucc(g.getOrCreateNode(load->target));
      break;
    }

    case Kind::Store: {
      StoreStmt *store = stmt->as<StoreStmt>();
      Node *node = g.getOrCreateNode(store->operand);
      node->addSucc(g.getOrCreateNode(store->target, /*is_ref=*/true));
      break;
    }
    default:
      continue;
    }
  }

  auto has_ref_node = [](SCC &scc) {
    return std::any_of(scc.begin(), scc.end(),
                       [](Node *n) { return n->is_ref; });
  };

  auto get_non_ref = [](SCC &scc) {
    for (Node *node : scc)
      if (!node->is_ref)
        return node;

    assert(false && "No non-ref node in SCC!");
  };

  // Detect SCCs.
  std::vector<SCC> sccs = findSCC(g);
  for (SCC &scc : sccs) {
    // If all nodes in the cirle are non-ref nodes,
    // collapse them into one node.
    if (!has_ref_node(scc)) {
      // Collapse
    } else {
      Node *non_ref = get_non_ref(scc);
      for (Node *node : scc)
        if (node->is_ref)
          info.emplace_back(node, non_ref);
    }
  }
  return info;
}

inline void solveConstraint(ConstraintGraph &graph, WorkList &worklist,
                            const OptimizationInfo &opt_info = {}) {
  while (!worklist.empty()) {
    Node *v = worklist.pop();
    // std::cout << "Select " << v->getName() << "\n";

    if (!opt_info.empty()) {
      for (const std::pair<Node *, Node *> info : opt_info) {
        if (v == info.first) {
          for (Node *pt : v->pts) {
            // Collapse (pt, info.second);
            // worklist.push(info.second);
          }
        }
      }
    }

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

  OptimizationInfo opt_info = optimizeConstraintGraph(g);

  solveConstraint(g, q, opt_info);

  PTAResult pts_info;
  for (std::unique_ptr<Node> &node : g.nodes) {
    if (node->pts.empty())
      continue;
    for (Node *pt : node->pts)
      pts_info[node->getName()].push_back(pt->getName());
  }

  return pts_info;
}
