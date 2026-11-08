#include <catch2/catch_test_macros.hpp>

#include "TestSupport.hpp"

#include "llvm/ADT/APInt.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include <cstdint>
#include <iterator>
#include <optional>
#include <string>

namespace {

std::string optimizeOrFail(const std::string &source) {
  TestCompiler compiler;
  std::string ir;
  std::string error;
  REQUIRE(compiler.compile(source, /*optimize=*/true, &ir, &error));
  return ir;
}

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

std::string runSCCPAndVerify(llvm::Module &module) {
  SCCP pass(module);
  pass.run();

  std::string error;
  llvm::raw_string_ostream errorStream(error);
  bool broken = llvm::verifyModule(module, &errorStream);
  errorStream.flush();
  INFO(error);
  REQUIRE_FALSE(broken);

  std::string text;
  llvm::raw_string_ostream stream(text);
  module.print(stream, nullptr);
  stream.flush();
  return text;
}

llvm::ConstantInt *int32(llvm::LLVMContext &context, uint64_t value) {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), value);
}

size_t instructionCount(const llvm::BasicBlock &block) {
  return static_cast<size_t>(std::distance(block.begin(), block.end()));
}

llvm::ReturnInst *firstReturn(llvm::Function *function) {
  for (llvm::BasicBlock &block : *function)
    if (auto *ret = llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator()))
      return ret;
  return nullptr;
}

} // namespace

TEST_CASE("mem2reg promotes local scalars to SSA", "[sccp]") {
  std::string ir = optimizeOrFail("int f(int a) { int x; x = a + 1; return x; }");
  REQUIRE_FALSE(contains(ir, "alloca"));
  REQUIRE_FALSE(contains(ir, "load i32"));
}

TEST_CASE("SCCP folds a constant conditional branch", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) {
  int x;
  x = 1;
  if (1) { x = 2; } else { x = 3; }
  return x;
}
)");
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "if.else"));
  REQUIRE_FALSE(contains(ir, "icmp"));
}

TEST_CASE("SCCP removes an unreachable then-branch", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(void) { if (0) { return 1; } return 2; }
)");
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "ret i32 1"));
  REQUIRE_FALSE(contains(ir, "if.then"));
}

TEST_CASE("SCCP removes a while loop with a constant false condition", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) { int x; x = 1; while (0) { x = x + 1; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 1"));
  REQUIRE_FALSE(contains(ir, "while.body"));
}

TEST_CASE("SCCP propagates constants through arithmetic", "[sccp]") {
  std::string ir = optimizeOrFail("int main(void) { int x; x = 3 * 4 + 1; return x; }");
  REQUIRE(contains(ir, "ret i32 13"));
}

TEST_CASE("SCCP folds PHIs with equal constant inputs", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(int a) { int x; if (a) { x = 1; } else { x = 1; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 1"));
  REQUIRE_FALSE(contains(ir, "phi i32"));
}

TEST_CASE("SCCP keeps a branch whose condition is unknown", "[sccp]") {
  std::string ir = optimizeOrFail("int f(int a) { if (a) { return 1; } return 2; }");
  REQUIRE(contains(ir, "br i1"));
  REQUIRE(contains(ir, "ret i32 1"));
  REQUIRE(contains(ir, "ret i32 2"));
}

TEST_CASE("SCCP preserves loops with unknown bounds", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(int n) {
  int s;
  s = 0;
  while (n) {
    s = s + n;
    n = n - 1;
  }
  return s;
}
)");
  REQUIRE(contains(ir, "while.cond"));
  REQUIRE(contains(ir, "phi i32"));
  REQUIRE(contains(ir, "br i1"));
}

TEST_CASE("SCCP removes dead instructions after folding", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) { int x; x = 1; if (x) { x = 2; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "icmp"));
}

TEST_CASE("SCCP keeps a live PHI after folding branches", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(int a) { int x; if (1) { x = 7; } else { x = a; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 7"));
}

TEST_CASE("SCCP keeps blocks reachable through goto", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) {
  int x;
  x = 0;
  if (1) {
    x = 1;
  } else {
  target:
    return 2;
  }
  goto target;
  return x;
}
)");
  REQUIRE(contains(ir, "target:"));
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "if.else"));
}

TEST_CASE("SCCP deletes mutually dependent unreachable blocks", "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_dead_cycle", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, {i32}, false),
      llvm::Function::ExternalLinkage, "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);
  llvm::BasicBlock *dead1 =
      llvm::BasicBlock::Create(context, "dead1", function);
  llvm::BasicBlock *dead2 =
      llvm::BasicBlock::Create(context, "dead2", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateRet(builder.getInt32(0));

  builder.SetInsertPoint(dead1);
  llvm::PHINode *phi = builder.CreatePHI(i32, 1, "p");
  phi->addIncoming(builder.getInt32(0), dead2);
  llvm::Value *a = builder.CreateAdd(phi, function->getArg(0), "a");
  builder.CreateBr(dead2);

  builder.SetInsertPoint(dead2);
  llvm::Value *b = builder.CreateAdd(a, builder.getInt32(1), "b");
  builder.CreateBr(dead1);

  std::string ir = runSCCPAndVerify(module);
  REQUIRE_FALSE(contains(ir, "dead1"));
  REQUIRE_FALSE(contains(ir, "dead2"));
  (void)b;
}

TEST_CASE("SCCP deduplicates PHI entries when both branch edges merge",
          "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_duplicate_phi", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false),
      llvm::Function::ExternalLinkage, "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);
  llvm::BasicBlock *merge =
      llvm::BasicBlock::Create(context, "merge", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateCondBr(builder.getTrue(), merge, merge);
  builder.SetInsertPoint(merge);
  llvm::PHINode *phi = builder.CreatePHI(i32, 2, "p");
  phi->addIncoming(builder.getInt32(1), entry);
  phi->addIncoming(builder.getInt32(2), entry);
  builder.CreateRet(phi);

  std::string ir = runSCCPAndVerify(module);
  REQUIRE(contains(ir, "phi i32 [ 1, %entry ]"));
  REQUIRE_FALSE(contains(ir, "[ 2, %entry ]"));
}

TEST_CASE("SCCP removes nested dead regions but keeps goto targets",
          "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) {
  int x;
  x = 0;
  if (1) {
    if (0) { x = 1; }
    x = 2;
  } else {
    if (0) { x = 3; }
    goto target;
  }
  goto target;
  target:
  return x;
}
)");
  REQUIRE(contains(ir, "target:"));
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "ret i32 3"));
}

TEST_CASE("SCCP lattice joins unknown with a constant", "[sccp][lattice]") {
  llvm::LLVMContext context;
  sccp::LatticeValue unknown = sccp::Unknown{};
  sccp::LatticeValue seven = sccp::Constant(int32(context, 7));

  REQUIRE(sccp::isUnknown(unknown));
  REQUIRE(sccp::isConstant(seven));
  REQUIRE(sccp::join(unknown, seven) == seven);
  REQUIRE(sccp::join(seven, unknown) == seven);
  REQUIRE(sccp::constantValue(sccp::join(unknown, seven)) ==
          sccp::constantValue(seven));
}

TEST_CASE("SCCP lattice keeps equal constants and merges different ones",
          "[sccp][lattice]") {
  llvm::LLVMContext context;
  llvm::LLVMContext otherContext;
  llvm::ConstantInt *seven = int32(context, 7);
  llvm::ConstantInt *sevenAgain = int32(otherContext, 7);
  llvm::ConstantInt *eight = int32(context, 8);

  sccp::LatticeValue first = sccp::Constant(seven);
  sccp::LatticeValue second = sccp::Constant(sevenAgain);
  sccp::LatticeValue different = sccp::Constant(eight);

  REQUIRE(seven != sevenAgain);
  REQUIRE(first == second);
  REQUIRE(sccp::isConstant(sccp::join(first, second)));
  REQUIRE(sccp::constantValue(sccp::join(first, second))->getValue() ==
          seven->getValue());
  REQUIRE(sccp::isOverdefined(sccp::join(first, different)));
}

TEST_CASE("SCCP lattice propagates overdefined and unknown states",
          "[sccp][lattice]") {
  llvm::LLVMContext context;
  sccp::LatticeValue unknown = sccp::Unknown{};
  sccp::LatticeValue seven = sccp::Constant(int32(context, 7));
  sccp::LatticeValue overdefined = sccp::Overdefined{};

  REQUIRE(sccp::isOverdefined(sccp::join(overdefined, seven)));
  REQUIRE(sccp::isOverdefined(sccp::join(unknown, overdefined)));
  REQUIRE(sccp::isOverdefined(sccp::constantOrOverdefined(nullptr)));
  REQUIRE(sccp::isConstant(sccp::constantOrOverdefined(int32(context, 9))));

  std::optional<sccp::LatticeValue> folded = sccp::propagatedState({&seven});
  REQUIRE_FALSE(folded.has_value());

  folded = sccp::propagatedState({&unknown});
  REQUIRE(folded.has_value());
  REQUIRE(sccp::isUnknown(*folded));

  folded = sccp::propagatedState({&unknown, &overdefined});
  REQUIRE(folded.has_value());
  REQUIRE(sccp::isOverdefined(*folded));
}

TEST_CASE("SCCP lattice advances monotonically", "[sccp][lattice]") {
  llvm::LLVMContext context;
  sccp::LatticeValue state = sccp::Unknown{};
  sccp::LatticeValue seven = sccp::Constant(int32(context, 7));

  REQUIRE_FALSE(sccp::advance(state, sccp::Unknown{}));
  REQUIRE(sccp::advance(state, seven));
  REQUIRE(sccp::isConstant(state));
  REQUIRE_FALSE(sccp::advance(state, seven));
  REQUIRE_FALSE(sccp::advance(state, sccp::Unknown{}));
  REQUIRE(sccp::isConstant(state));
  REQUIRE(sccp::advance(state, sccp::Overdefined{}));
  REQUIRE(sccp::isOverdefined(state));
  REQUIRE_FALSE(sccp::advance(state, seven));
  REQUIRE(sccp::isOverdefined(state));
}

TEST_CASE("SCCP evaluates PHIs over executable edges only", "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_phi_edges", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);
  llvm::BasicBlock *thenBlock =
      llvm::BasicBlock::Create(context, "then", function);
  llvm::BasicBlock *elseBlock =
      llvm::BasicBlock::Create(context, "else", function);
  llvm::BasicBlock *merge =
      llvm::BasicBlock::Create(context, "merge", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateCondBr(builder.getTrue(), thenBlock, elseBlock);
  builder.SetInsertPoint(thenBlock);
  builder.CreateBr(merge);
  builder.SetInsertPoint(elseBlock);
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  llvm::PHINode *phi = builder.CreatePHI(i32, 2, "p");
  phi->addIncoming(builder.getInt32(11), thenBlock);
  phi->addIncoming(builder.getInt32(22), elseBlock);
  builder.CreateRet(phi);

  std::string ir = runSCCPAndVerify(module);
  REQUIRE(function->size() == 3);
  REQUIRE(llvm::isa<llvm::UncondBrInst>(entry->getTerminator()));
  llvm::ReturnInst *ret = firstReturn(function);
  REQUIRE(ret != nullptr);
  auto *returned = llvm::dyn_cast<llvm::ConstantInt>(ret->getReturnValue());
  REQUIRE(returned != nullptr);
  REQUIRE(returned->getValue() == llvm::APInt(32, 11));
  REQUIRE(phi->getNumIncomingValues() == 1);
  REQUIRE(phi->getIncomingBlock(0) == thenBlock);
  REQUIRE(phi->getIncomingValue(0) == builder.getInt32(11));
  REQUIRE_FALSE(contains(ir, "else:"));
}

TEST_CASE("SCCP marks both edges for an overdefined branch condition",
          "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_overdefined_branch", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Type *i1 = llvm::Type::getInt1Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, {i1}, false),
      llvm::Function::ExternalLinkage, "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);
  llvm::BasicBlock *thenBlock =
      llvm::BasicBlock::Create(context, "then", function);
  llvm::BasicBlock *elseBlock =
      llvm::BasicBlock::Create(context, "else", function);
  llvm::BasicBlock *merge =
      llvm::BasicBlock::Create(context, "merge", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateCondBr(function->getArg(0), thenBlock, elseBlock);
  builder.SetInsertPoint(thenBlock);
  builder.CreateBr(merge);
  builder.SetInsertPoint(elseBlock);
  builder.CreateBr(merge);
  builder.SetInsertPoint(merge);
  llvm::PHINode *phi = builder.CreatePHI(i32, 2, "p");
  phi->addIncoming(builder.getInt32(1), thenBlock);
  phi->addIncoming(builder.getInt32(2), elseBlock);
  builder.CreateRet(phi);

  runSCCPAndVerify(module);
  REQUIRE(function->size() == 4);
  REQUIRE(llvm::isa<llvm::CondBrInst>(entry->getTerminator()));
  llvm::ReturnInst *ret = firstReturn(function);
  REQUIRE(ret != nullptr);
  REQUIRE(ret->getReturnValue() == phi);
  REQUIRE(phi->getNumIncomingValues() == 2);
}

TEST_CASE("SCCP keeps the taken edge's PHI value when both edges merge",
          "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_duplicate_phi_false", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);
  llvm::BasicBlock *merge =
      llvm::BasicBlock::Create(context, "merge", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateCondBr(builder.getFalse(), merge, merge);
  builder.SetInsertPoint(merge);
  llvm::PHINode *phi = builder.CreatePHI(i32, 2, "p");
  phi->addIncoming(builder.getInt32(1), entry);
  phi->addIncoming(builder.getInt32(2), entry);
  builder.CreateRet(phi);

  runSCCPAndVerify(module);
  REQUIRE(function->size() == 2);
  REQUIRE(llvm::isa<llvm::UncondBrInst>(entry->getTerminator()));
  llvm::ReturnInst *ret = firstReturn(function);
  REQUIRE(ret != nullptr);
  REQUIRE(ret->getReturnValue() == phi);
  REQUIRE(phi->getNumIncomingValues() == 1);
  REQUIRE(phi->getIncomingBlock(0) == entry);
  REQUIRE(phi->getIncomingValue(0) == builder.getInt32(2));
}

TEST_CASE("SCCP folds integer arithmetic, comparisons, casts, and select",
          "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_fold_chain", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);

  llvm::IRBuilder<> builder(entry);
  llvm::Value *sum =
      builder.CreateAdd(builder.getInt32(3), builder.getInt32(4), "sum");
  llvm::Value *product = builder.CreateMul(sum, builder.getInt32(2), "product");
  llvm::Value *less =
      builder.CreateICmpSLT(product, builder.getInt32(15), "less");
  llvm::Value *selected =
      builder.CreateSelect(less, product, builder.getInt32(0), "selected");
  llvm::Value *narrow = builder.CreateTrunc(
      selected, llvm::Type::getInt8Ty(context), "narrow");
  builder.CreateSExt(narrow, i32, "widen");
  llvm::Value *zeroExtend = builder.CreateZExt(narrow, i32, "zeroextend");
  builder.CreateRet(zeroExtend);

  runSCCPAndVerify(module);
  REQUIRE(instructionCount(*entry) == 1);
  llvm::ReturnInst *ret = firstReturn(function);
  REQUIRE(ret != nullptr);
  auto *returned = llvm::dyn_cast<llvm::ConstantInt>(ret->getReturnValue());
  REQUIRE(returned != nullptr);
  REQUIRE(returned->getValue() == llvm::APInt(32, 14));
}

TEST_CASE("SCCP keeps unsupported operations overdefined", "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_unsupported", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);

  llvm::IRBuilder<> builder(entry);
  llvm::Value *slot = builder.CreateAlloca(i32, nullptr, "slot");
  builder.CreateStore(builder.getInt32(5), slot);
  llvm::Value *loaded = builder.CreateLoad(i32, slot, "loaded");
  llvm::Value *sum = builder.CreateAdd(loaded, builder.getInt32(1), "sum");
  builder.CreateRet(sum);

  std::string ir = runSCCPAndVerify(module);
  REQUIRE(instructionCount(*entry) == 5);
  llvm::ReturnInst *ret = firstReturn(function);
  REQUIRE(ret != nullptr);
  REQUIRE(ret->getReturnValue() == sum);
  REQUIRE(contains(ir, "load i32"));
  REQUIRE(contains(ir, "add i32"));
}

TEST_CASE("SCCP keeps calls with side effects", "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_side_effects", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *callee = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "side", module);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateAdd(builder.getInt32(1), builder.getInt32(2), "unused");
  builder.CreateCall(callee->getFunctionType(), callee);
  builder.CreateRet(builder.getInt32(0));

  runSCCPAndVerify(module);
  REQUIRE(instructionCount(*entry) == 2);
  REQUIRE(llvm::isa<llvm::CallInst>(entry->front()));
}

TEST_CASE("SCCP converges on a cyclic CFG with a constant PHI", "[sccp]") {
  llvm::LLVMContext context;
  llvm::Module module("sccp_constant_loop", context);
  llvm::Type *i32 = llvm::Type::getInt32Ty(context);
  llvm::Function *function = llvm::Function::Create(
      llvm::FunctionType::get(i32, false), llvm::Function::ExternalLinkage,
      "f", module);
  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", function);
  llvm::BasicBlock *loopBlock =
      llvm::BasicBlock::Create(context, "loop", function);
  llvm::BasicBlock *exitBlock =
      llvm::BasicBlock::Create(context, "exit", function);

  llvm::IRBuilder<> builder(entry);
  builder.CreateBr(loopBlock);
  builder.SetInsertPoint(loopBlock);
  llvm::PHINode *phi = builder.CreatePHI(i32, 2, "i");
  phi->addIncoming(builder.getInt32(7), entry);
  phi->addIncoming(builder.getInt32(7), loopBlock);
  llvm::Value *done = builder.CreateICmpEQ(phi, builder.getInt32(7), "done");
  builder.CreateCondBr(done, exitBlock, loopBlock);
  builder.SetInsertPoint(exitBlock);
  builder.CreateRet(phi);

  runSCCPAndVerify(module);
  REQUIRE(function->size() == 3);
  REQUIRE(llvm::isa<llvm::UncondBrInst>(loopBlock->getTerminator()));
  REQUIRE(loopBlock->getTerminator()->getSuccessor(0) == exitBlock);
  llvm::ReturnInst *ret = firstReturn(function);
  REQUIRE(ret != nullptr);
  auto *returned = llvm::dyn_cast<llvm::ConstantInt>(ret->getReturnValue());
  REQUIRE(returned != nullptr);
  REQUIRE(returned->getValue() == llvm::APInt(32, 7));
  REQUIRE(phi->getNumIncomingValues() == 1);
  REQUIRE(phi->getIncomingBlock(0) == entry);
}
