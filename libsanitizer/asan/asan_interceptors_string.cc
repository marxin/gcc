//===-- asan_interceptors.cc ----------------------------------------------===//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of AddressSanitizer, an address sanity checker.
//
// Intercept various libc functions.
//===----------------------------------------------------------------------===//

#include "asan_interceptors.h"
#include "asan_allocator.h"
#include "asan_internal.h"
#include "asan_mapping.h"
#include "asan_poisoning.h"
#include "asan_report.h"
#include "asan_stack.h"
#include "asan_stats.h"
#include "asan_suppressions.h"
#include "lsan/lsan_common.h"
#include "sanitizer_common/sanitizer_libc.h"

// There is no general interception at all on Fuchsia.
// Only the functions in asan_interceptors_memintrinsics.cc are
// really defined to replace libc functions.
#if !SANITIZER_FUCHSIA

#if SANITIZER_POSIX
#include "sanitizer_common/sanitizer_posix.h"
#endif

#if defined(__i386) && SANITIZER_LINUX
#define ASAN_PTHREAD_CREATE_VERSION "GLIBC_2.1"
#elif defined(__mips__) && SANITIZER_LINUX
#define ASAN_PTHREAD_CREATE_VERSION "GLIBC_2.2"
#endif

namespace __asan {

#define ASAN_READ_STRING_OF_LEN(ctx, s, len, n)                 \
  ASAN_READ_RANGE((ctx), (s),                                   \
    common_flags()->strict_string_checks ? (len) + 1 : (n))

#define ASAN_READ_STRING(ctx, s, n)                             \
  ASAN_READ_STRING_OF_LEN((ctx), (s), REAL(strlen)(s), (n))

static inline uptr MaybeRealStrnlen(const char *s, uptr maxlen) {
#if SANITIZER_INTERCEPT_STRNLEN
  if (REAL(strnlen)) {
    return REAL(strnlen)(s, maxlen);
  }
#endif
  return internal_strnlen(s, maxlen);
}

} // namespace __asan

// ---------------------- Wrappers ---------------- {{{1
using namespace __asan;  // NOLINT

DECLARE_REAL_AND_INTERCEPTOR(void *, malloc, uptr)
DECLARE_REAL_AND_INTERCEPTOR(void, free, void *)

#define ASAN_INTERCEPTOR_ENTER(ctx, func)                                      \
  AsanInterceptorContext _ctx = {#func};                                       \
  ctx = (void *)&_ctx;                                                         \
  (void) ctx;                                                                  \

#define COMMON_INTERCEPT_FUNCTION(name) ASAN_INTERCEPT_FUNC(name)
#define COMMON_INTERCEPT_FUNCTION_VER(name, ver)                          \
  ASAN_INTERCEPT_FUNC_VER(name, ver)
#define COMMON_INTERCEPTOR_WRITE_RANGE(ctx, ptr, size) \
  ASAN_WRITE_RANGE(ctx, ptr, size)
#define COMMON_INTERCEPTOR_READ_RANGE(ctx, ptr, size) \
  ASAN_READ_RANGE(ctx, ptr, size)
#define COMMON_INTERCEPTOR_ENTER(ctx, func, ...)                               \
  ASAN_INTERCEPTOR_ENTER(ctx, func);                                           \
  do {                                                                         \
    if (asan_init_is_running)                                                  \
      return REAL(func)(__VA_ARGS__);                                          \
    if (SANITIZER_MAC && UNLIKELY(!asan_inited))                               \
      return REAL(func)(__VA_ARGS__);                                          \
    ENSURE_ASAN_INITED();                                                      \
  } while (false)
#define COMMON_INTERCEPTOR_DIR_ACQUIRE(ctx, path) \
  do {                                            \
  } while (false)
#define COMMON_INTERCEPTOR_FD_ACQUIRE(ctx, fd) \
  do {                                         \
  } while (false)
#define COMMON_INTERCEPTOR_FD_RELEASE(ctx, fd) \
  do {                                         \
  } while (false)
#define COMMON_INTERCEPTOR_FD_SOCKET_ACCEPT(ctx, fd, newfd) \
  do {                                                      \
  } while (false)
#define COMMON_INTERCEPTOR_SET_THREAD_NAME(ctx, name) SetThreadName(name)
// Should be asanThreadRegistry().SetThreadNameByUserId(thread, name)
// But asan does not remember UserId's for threads (pthread_t);
// and remembers all ever existed threads, so the linear search by UserId
// can be slow.
#define COMMON_INTERCEPTOR_SET_PTHREAD_NAME(ctx, thread, name) \
  do {                                                         \
  } while (false)
#define COMMON_INTERCEPTOR_BLOCK_REAL(name) REAL(name)
// Strict init-order checking is dlopen-hostile:
// https://github.com/google/sanitizers/issues/178
#define COMMON_INTERCEPTOR_ON_DLOPEN(filename, flag)                           \
  do {                                                                         \
    if (flags()->strict_init_order)                                            \
      StopInitOrderChecking();                                                 \
    CheckNoDeepBind(filename, flag);                                           \
  } while (false)
#define COMMON_INTERCEPTOR_ON_EXIT(ctx) OnExit()
#define COMMON_INTERCEPTOR_LIBRARY_LOADED(filename, handle)
#define COMMON_INTERCEPTOR_LIBRARY_UNLOADED()
#define COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED (!asan_inited)
#define COMMON_INTERCEPTOR_GET_TLS_RANGE(begin, end)                           \
  if (AsanThread *t = GetCurrentThread()) {                                    \
    *begin = t->tls_begin();                                                   \
    *end = t->tls_end();                                                       \
  } else {                                                                     \
    *begin = *end = 0;                                                         \
  }

#define COMMON_INTERCEPTOR_MEMMOVE_IMPL(ctx, to, from, size) \
  do {                                                       \
    ASAN_INTERCEPTOR_ENTER(ctx, memmove);                    \
    ASAN_MEMMOVE_IMPL(ctx, to, from, size);                  \
  } while (false)

#define COMMON_INTERCEPTOR_MEMCPY_IMPL(ctx, to, from, size) \
  do {                                                      \
    ASAN_INTERCEPTOR_ENTER(ctx, memcpy);                    \
    ASAN_MEMCPY_IMPL(ctx, to, from, size);                  \
  } while (false)

#define COMMON_INTERCEPTOR_MEMSET_IMPL(ctx, block, c, size) \
  do {                                                      \
    ASAN_INTERCEPTOR_ENTER(ctx, memset);                    \
    ASAN_MEMSET_IMPL(ctx, block, c, size);                  \
  } while (false)

#include "sanitizer_common/sanitizer_common_interceptors_string.inc"

#if ASAN_INTERCEPT_INDEX
# if ASAN_USE_ALIAS_ATTRIBUTE_FOR_INDEX
INTERCEPTOR(char*, index, const char *string, int c)
  ALIAS(WRAPPER_NAME(strchr));
# else
#  if SANITIZER_MAC
DECLARE_REAL(char*, index, const char *string, int c)
OVERRIDE_FUNCTION(index, strchr);
#  else
DEFINE_REAL(char*, index, const char *string, int c)
#  endif
# endif
#endif  // ASAN_INTERCEPT_INDEX

// For both strcat() and strncat() we need to check the validity of |to|
// argument irrespective of the |from| length.
INTERCEPTOR(char*, strcat, char *to, const char *from) {  // NOLINT
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strcat);  // NOLINT
  ENSURE_ASAN_INITED();
  if (flags()->replace_str) {
    uptr from_length = REAL(strlen)(from);
    ASAN_READ_RANGE(ctx, from, from_length + 1);
    uptr to_length = REAL(strlen)(to);
    ASAN_READ_STRING_OF_LEN(ctx, to, to_length, to_length);
    ASAN_WRITE_RANGE(ctx, to + to_length, from_length + 1);
    // If the copying actually happens, the |from| string should not overlap
    // with the resulting string starting at |to|, which has a length of
    // to_length + from_length + 1.
    if (from_length > 0) {
      CHECK_RANGES_OVERLAP("strcat", to, from_length + to_length + 1,
                           from, from_length + 1);
    }
  }
  return REAL(strcat)(to, from);  // NOLINT
}

INTERCEPTOR(char*, strncat, char *to, const char *from, uptr size) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strncat);
  ENSURE_ASAN_INITED();
  if (flags()->replace_str) {
    uptr from_length = MaybeRealStrnlen(from, size);
    uptr copy_length = Min(size, from_length + 1);
    ASAN_READ_RANGE(ctx, from, copy_length);
    uptr to_length = REAL(strlen)(to);
    ASAN_READ_STRING_OF_LEN(ctx, to, to_length, to_length);
    ASAN_WRITE_RANGE(ctx, to + to_length, from_length + 1);
    if (from_length > 0) {
      CHECK_RANGES_OVERLAP("strncat", to, to_length + copy_length + 1,
                           from, copy_length);
    }
  }
  return REAL(strncat)(to, from, size);
}

INTERCEPTOR(char*, strcpy, char *to, const char *from) {  // NOLINT
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strcpy);  // NOLINT
#if SANITIZER_MAC
  if (UNLIKELY(!asan_inited)) return REAL(strcpy)(to, from);  // NOLINT
#endif
  // strcpy is called from malloc_default_purgeable_zone()
  // in __asan::ReplaceSystemAlloc() on Mac.
  if (asan_init_is_running) {
    return REAL(strcpy)(to, from);  // NOLINT
  }
  ENSURE_ASAN_INITED();
  if (flags()->replace_str) {
    uptr from_size = REAL(strlen)(from) + 1;
    CHECK_RANGES_OVERLAP("strcpy", to, from_size, from, from_size);
    ASAN_READ_RANGE(ctx, from, from_size);
    ASAN_WRITE_RANGE(ctx, to, from_size);
  }
  return REAL(strcpy)(to, from);  // NOLINT
}

INTERCEPTOR(char*, strdup, const char *s) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strdup);
  if (UNLIKELY(!asan_inited)) return internal_strdup(s);
  ENSURE_ASAN_INITED();
  uptr length = REAL(strlen)(s);
  if (flags()->replace_str) {
    ASAN_READ_RANGE(ctx, s, length + 1);
  }
  GET_STACK_TRACE_MALLOC;
  void *new_mem = asan_malloc(length + 1, &stack);
  REAL(memcpy)(new_mem, s, length + 1);
  return reinterpret_cast<char*>(new_mem);
}

#if ASAN_INTERCEPT___STRDUP
INTERCEPTOR(char*, __strdup, const char *s) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strdup);
  if (UNLIKELY(!asan_inited)) return internal_strdup(s);
  ENSURE_ASAN_INITED();
  uptr length = REAL(strlen)(s);
  if (flags()->replace_str) {
    ASAN_READ_RANGE(ctx, s, length + 1);
  }
  GET_STACK_TRACE_MALLOC;
  void *new_mem = asan_malloc(length + 1, &stack);
  REAL(memcpy)(new_mem, s, length + 1);
  return reinterpret_cast<char*>(new_mem);
}
#endif // ASAN_INTERCEPT___STRDUP

INTERCEPTOR(char*, strncpy, char *to, const char *from, uptr size) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strncpy);
  ENSURE_ASAN_INITED();
  if (flags()->replace_str) {
    uptr from_size = Min(size, MaybeRealStrnlen(from, size) + 1);
    CHECK_RANGES_OVERLAP("strncpy", to, from_size, from, from_size);
    ASAN_READ_RANGE(ctx, from, from_size);
    ASAN_WRITE_RANGE(ctx, to, size);
  }
  return REAL(strncpy)(to, from, size);
}

INTERCEPTOR(long, strtol, const char *nptr,  // NOLINT
            char **endptr, int base) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strtol);
  ENSURE_ASAN_INITED();
  if (!flags()->replace_str) {
    return REAL(strtol)(nptr, endptr, base);
  }
  char *real_endptr;
  long result = REAL(strtol)(nptr, &real_endptr, base);  // NOLINT
  StrtolFixAndCheck(ctx, nptr, endptr, real_endptr, base);
  return result;
}

INTERCEPTOR(int, atoi, const char *nptr) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, atoi);
#if SANITIZER_MAC
  if (UNLIKELY(!asan_inited)) return REAL(atoi)(nptr);
#endif
  ENSURE_ASAN_INITED();
  if (!flags()->replace_str) {
    return REAL(atoi)(nptr);
  }
  char *real_endptr;
  // "man atoi" tells that behavior of atoi(nptr) is the same as
  // strtol(nptr, 0, 10), i.e. it sets errno to ERANGE if the
  // parsed integer can't be stored in *long* type (even if it's
  // different from int). So, we just imitate this behavior.
  int result = REAL(strtol)(nptr, &real_endptr, 10);
  FixRealStrtolEndptr(nptr, &real_endptr);
  ASAN_READ_STRING(ctx, nptr, (real_endptr - nptr) + 1);
  return result;
}

INTERCEPTOR(long, atol, const char *nptr) {  // NOLINT
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, atol);
#if SANITIZER_MAC
  if (UNLIKELY(!asan_inited)) return REAL(atol)(nptr);
#endif
  ENSURE_ASAN_INITED();
  if (!flags()->replace_str) {
    return REAL(atol)(nptr);
  }
  char *real_endptr;
  long result = REAL(strtol)(nptr, &real_endptr, 10);  // NOLINT
  FixRealStrtolEndptr(nptr, &real_endptr);
  ASAN_READ_STRING(ctx, nptr, (real_endptr - nptr) + 1);
  return result;
}

#if ASAN_INTERCEPT_ATOLL_AND_STRTOLL
INTERCEPTOR(long long, strtoll, const char *nptr,  // NOLINT
            char **endptr, int base) {
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, strtoll);
  ENSURE_ASAN_INITED();
  if (!flags()->replace_str) {
    return REAL(strtoll)(nptr, endptr, base);
  }
  char *real_endptr;
  long long result = REAL(strtoll)(nptr, &real_endptr, base);  // NOLINT
  StrtolFixAndCheck(ctx, nptr, endptr, real_endptr, base);
  return result;
}

INTERCEPTOR(long long, atoll, const char *nptr) {  // NOLINT
  void *ctx;
  ASAN_INTERCEPTOR_ENTER(ctx, atoll);
  ENSURE_ASAN_INITED();
  if (!flags()->replace_str) {
    return REAL(atoll)(nptr);
  }
  char *real_endptr;
  long long result = REAL(strtoll)(nptr, &real_endptr, 10);  // NOLINT
  FixRealStrtolEndptr(nptr, &real_endptr);
  ASAN_READ_STRING(ctx, nptr, (real_endptr - nptr) + 1);
  return result;
}
#endif  // ASAN_INTERCEPT_ATOLL_AND_STRTOLL

// ---------------------- InitializeAsanInterceptors ---------------- {{{1
namespace __asan {
void InitializeAsanInterceptorsStrings() {
#if ASAN_INTERCEPT_INDEX && ASAN_USE_ALIAS_ATTRIBUTE_FOR_INDEX
  ASAN_INTERCEPT_FUNC(index);
#endif

  // Intercept str* functions.
  ASAN_INTERCEPT_FUNC(strcat);  // NOLINT
  ASAN_INTERCEPT_FUNC(strcpy);  // NOLINT
  ASAN_INTERCEPT_FUNC(strncat);
  ASAN_INTERCEPT_FUNC(strncpy);
  ASAN_INTERCEPT_FUNC(strdup);
#if ASAN_INTERCEPT___STRDUP
  ASAN_INTERCEPT_FUNC(__strdup);
#endif

  ASAN_INTERCEPT_FUNC(atoi);
  ASAN_INTERCEPT_FUNC(atol);
  ASAN_INTERCEPT_FUNC(strtol);
#if ASAN_INTERCEPT_ATOLL_AND_STRTOLL
  ASAN_INTERCEPT_FUNC(atoll);
  ASAN_INTERCEPT_FUNC(strtoll);
#endif


  VReport(1, "AddressSanitizer: libc interceptors initialized\n");
}

} // namespace __asan

#endif  // !SANITIZER_FUCHSIA
