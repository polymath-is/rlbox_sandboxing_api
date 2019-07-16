// NOLINTNEXTLINE
#define RLBOX_USE_STATIC_CALLS() rlbox_test_sandbox_lookup_symbol
#include "test_include.hpp"

using rlbox::tainted;
using RL = rlbox::RLBoxSandbox<TestSandbox>;

int test_fn(int a)
{
  return a;
}

// NOLINTNEXTLINE
TEST_CASE("Test sandbox_function assignment", "[sandbox_function]")
{
  RL sandbox;
  sandbox.create_sandbox();

  using T_F = int (*)(int);

  auto ptr = sandbox.malloc_in_sandbox<T_F>();
  auto cb = sandbox_function_address(sandbox, test_fn); // NOLINT

  tainted<T_F, TestSandbox> val = nullptr;

  // Assignment to tainted is not ok
  REQUIRE_COMPILE_ERR(val = cb);

  // Assignment to tainted_volatile is fine
  REQUIRE_NO_COMPILE_ERR(*ptr = cb);

  sandbox.destroy_sandbox();
}
