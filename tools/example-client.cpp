#include "../ir/function.h"
#include "../tools/transform.h"
#include "../smt/smt.h"
#include "../smt/solver.h"

#include <iostream>
#include <unordered_set>
class FunctionBuilder {
public:
  FunctionBuilder(IR::Function& F_) : F(F_) {}

  template <typename A, typename B, typename ...Others>
  IR::Value *BinOp(IR::Type &t, std::string name, A a, B b,Others... others) {
    auto copy = name;
    return Append
        (copy, std::make_unique<IR::BinOp>
         (t, std::move(name), *ToValue(t, a), *ToValue(t, b), others...));
  }

  template <typename A>
  IR::Value *ConversionOp(IR::Type &t, std::string name, A a,
                          IR::ConversionOp::Op Op) {
    auto copy = name;
    return Append
        (copy, std::make_unique<IR::ConversionOp>
         (t, std::move(name), *ToValue(t, a), Op));
  }

  template <typename A, typename B, typename C>
  IR::Value *Select(IR::Type &t, std::string name, A a, B b, C c) {
    auto copy = name;
    return Append
        (copy, std::make_unique<IR::Select>
         (t, std::move(name), *ToValue(t, a), *ToValue(t, b), *ToValue(t, c)));
  }

  template <typename A, typename B>
  IR::Value *ICmp(IR::Type &t, std::string name,
                  IR::ICmp::Cond cond, A a, B b) {
    auto copy = name;
    return Append
        (copy, std::make_unique<IR::ICmp>
         (t, std::move(name), cond, *ToValue(t, a), *ToValue(t, b)));
  }

  IR::Value *Var(IR::Type &t, std::string name) {
    return ToValue(t, name);
  }

  IR::Value *Val(IR::Type &t, int64_t val) {
    return ToValue(t, val);
  }

  template<typename T>
  IR::Value *Ret(IR::Type &t, T ret) {
    return Append(
      std::make_unique<IR::Return>(t, *ToValue(t, ret)));
  }

  // Unimplemented : Freeze, CopyOp, Unreachable

private:
  template <typename T>
  IR::Value* Append(T &&p) {
    auto ptr = p.get();
    F.getBB("").addIntr(std::move(p));
    return ptr;
  }

  template <typename T>
  IR::Value* Append(std::string name, T &&p) {
    auto ptr = p.get();
    F.getBB("").addIntr(std::move(p));
    identifiers[name] = ptr;
    return ptr;
  }

  IR::Function& F;
  std::unordered_map<std::string, IR::Value *> identifiers;

  IR::Value *ToValue(IR::Type& t, int64_t x) {
    auto c = std::make_unique<IR::IntConst>(t, x);
    auto ptr = c.get();
    F.addConstant(std::move(c));
    return ptr;
  }

  IR::Value *ToValue(IR::Type& t, std::string x) {
    if (auto It = identifiers.find(x); It != identifiers.end()) {
      return It->second;
    } else {
      auto i = std::make_unique<IR::Input>(t, std::move(x));
      auto ptr = i.get();

      // for (auto y : intermediates) {
      //   std::cout << y << "\t";
      // }
      // std::cout << "\nV(" << x << ")\n";

      F.addInput(std::move(i));

      identifiers[x] = ptr;
      return ptr;
    }
  }

  IR::Value *ToValue(IR::Type& t, const char *x) {
    return ToValue(t, std::string(x));
  }

  IR::Value *ToValue(IR::Type& t, IR::Value *v) {
    return v;
  }
};

int main() {

  auto type = IR::IntType("i15", 15);

  IR::Function src, tgt;

  FunctionBuilder lhs(src);
  lhs.BinOp(type, "%na", 0, "%a", IR::BinOp::Sub);
  lhs.BinOp(type, "%c", "%na", "%b", IR::BinOp::Add);
  lhs.Ret(type, "%c");
  src.setType(type);
  // Doesn't work without %

  FunctionBuilder rhs(tgt);
  rhs.BinOp(type, "%c", "%b", "%a", IR::BinOp::Sub);
  rhs.Ret(type, "%c");
  tgt.setType(type);

  smt::solver_print_queries(true);
  smt::smt_initializer smt_init;

  tools::TransformPrintOpts print_opts;
  print_opts.print_fn_header = false;

  src.print(std::cout);
  tgt.print(std::cout);

  tools::Transform t;
  t.src = std::move(src);
  t.tgt = std::move(tgt);

  tools::TransformVerify tv(t, /*check_each_var=*/false);
  auto types = tv.getTypings();
  if (!types) {
    std::cerr << "Doesn't type check!\n";
    // ++num_errors;
    return 1;
  }

  unsigned i = 0;
  bool correct = true;
  for (; types; ++types) {
    tv.fixupTypes(types);
    if (auto errs = tv.verify()) {
      std::cerr << errs;
      // ++num_errors;
      correct = false;
      break;
    }
    std::cout << "\rDone: " << ++i << std::flush;
  }
  std::cout << '\n';
  if (correct)
    std::cout << "Optimization is correct!\n";
  return 0;
}
