#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <map>
#include <mutex>
#include <type_traits>

#include "rlbox_helpers.hpp"
#include "rlbox_typetraits.hpp"

namespace rlbox {

template<typename T_Sbx>
class RLBoxSandbox : protected T_Sbx
{
  KEEP_CLASSES_FRIENDLY

private:
  std::mutex func_ptr_cache_lock;
  std::map<std::string, void*> func_ptr_map;

public:
  /***** Function to adjust for custom machine models *****/

  template<typename T>
  using convert_to_sandbox_equivalent_nonclass_t =
    detail::convert_base_types_t<T,
                                 typename T_Sbx::T_IntType,
                                 typename T_Sbx::T_LongType,
                                 typename T_Sbx::T_LongLongType,
                                 typename T_Sbx::T_PointerType>;

  T_Sbx* get_sandbox_impl() { return this; }

  template<typename... T_Args>
  inline auto create_sandbox(T_Args... args)
  {
    return this->impl_create_sandbox(std::forward<T_Args>(args)...);
  }

  inline auto destroy_sandbox() { return this->impl_destroy_sandbox(); }

  template<typename T>
  inline T* get_unsandboxed_pointer(
    convert_to_sandbox_equivalent_nonclass_t<T*> p) const
  {
    if (p == 0) {
      return nullptr;
    }
    auto ret = this->template impl_get_unsandboxed_pointer<T>(p);
    return reinterpret_cast<T*>(ret);
  }

  template<typename T>
  inline convert_to_sandbox_equivalent_nonclass_t<T*> get_sandboxed_pointer(
    const void* p) const
  {
    if (p == nullptr) {
      return 0;
    }
    return this->template impl_get_sandboxed_pointer<T>(p);
  }

  template<typename T>
  static inline T* get_unsandboxed_pointer(
    convert_to_sandbox_equivalent_nonclass_t<T*> p,
    const void* example_unsandboxed_ptr)
  {
    if (p == 0) {
      return nullptr;
    }
    auto ret = T_Sbx::template impl_get_unsandboxed_pointer<T>(
      p, example_unsandboxed_ptr);
    return reinterpret_cast<T*>(ret);
  }

  template<typename T>
  static inline convert_to_sandbox_equivalent_nonclass_t<T*>
  get_sandboxed_pointer(const void* p, const void* example_unsandboxed_ptr)
  {
    if (p == nullptr) {
      return 0;
    }
    return T_Sbx::template impl_get_sandboxed_pointer<T>(
      p, example_unsandboxed_ptr);
  }

  template<typename T>
  inline tainted<T*, T_Sbx> malloc_in_sandbox()
  {
    const uint32_t defaultCount = 1;
    return malloc_in_sandbox<T>(defaultCount);
  }
  template<typename T>
  inline tainted<T*, T_Sbx> malloc_in_sandbox(uint32_t count)
  {
    detail::dynamic_check(count != 0, "Malloc tried to allocate 0 bytes");
    auto ptr_in_sandbox = this->impl_malloc_in_sandbox(sizeof(T) * count);
    auto ptr = get_unsandboxed_pointer<T>(ptr_in_sandbox);
    detail::dynamic_check(is_pointer_in_sandbox_memory(ptr),
                          "Malloc returned pointer outside the sandbox memory");
    auto ptrEnd = reinterpret_cast<uintptr_t>(ptr + (count - 1));
    detail::dynamic_check(
      is_in_same_sandbox(ptr, reinterpret_cast<void*>(ptrEnd)),
      "Malloc returned a pointer whose range goes beyond sandbox memory");
    auto cast_ptr = reinterpret_cast<T*>(ptr);
    return tainted<T*, T_Sbx>(cast_ptr);
  }

  template<typename T>
  inline void free_in_sandbox(tainted<T*, T_Sbx> ptr)
  {
    this->impl_free_in_sandbox(ptr.get_raw_sandbox_value());
  }

  static inline bool is_in_same_sandbox(const void* p1, const void* p2)
  {
    return T_Sbx::impl_is_in_same_sandbox(p1, p2);
  }

  inline bool is_pointer_in_sandbox_memory(const void* p)
  {
    return this->impl_is_pointer_in_sandbox_memory(p);
  }

  inline size_t get_total_memory() { return this->impl_get_total_memory(); }

  void* lookup_symbol(const char* func_name)
  {
    std::lock_guard<std::mutex> lock(func_ptr_cache_lock);

    auto func_ptr_ref = func_ptr_map.find(func_name);

    void* func_ptr;
    if (func_ptr_ref == func_ptr_map.end()) {
      func_ptr = this->impl_lookup_symbol(func_name);
      func_ptr_map[func_name] = func_ptr;
    } else {
      func_ptr = func_ptr_ref->second;
    }

    return func_ptr;
  }
};

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(__GNUC__) || defined(__GNUG__)
// Can't turn off the variadic macro warning emitted from -pedantic
#  pragma GCC system_header
#elif defined(_MSC_VER)
// Doesn't seem to emit the warning
#else
// Don't know the compiler... just let it go through
#endif

#if defined(RLBOX_USE_STATIC_CALLS)
#  define sandbox_invoke(sandbox, func_name, ...)                              \
    RLBOX_USE_STATIC_CALLS##_lookup_symbol(sandbox, func_name)
#else
#  define sandbox_invoke(sandbox, func_name, ...)                              \
    sandbox.lookup_symbol(#func_name)
#endif

#if defined(__clang__)
#  pragma clang diagnostic pop
#else
#endif

}