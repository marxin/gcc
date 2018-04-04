//===-- sanitizer_common_interceptors.inc -----------------------*- C++ -*-===//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Common function interceptors for tools like AddressSanitizer,
// ThreadSanitizer, MemorySanitizer, etc.
//
// This file should be included into the tool's interceptor file,
// which has to define its own macros:
//   COMMON_INTERCEPTOR_ENTER
//   COMMON_INTERCEPTOR_ENTER_NOIGNORE
//   COMMON_INTERCEPTOR_READ_RANGE
//   COMMON_INTERCEPTOR_WRITE_RANGE
//   COMMON_INTERCEPTOR_INITIALIZE_RANGE
//   COMMON_INTERCEPTOR_DIR_ACQUIRE
//   COMMON_INTERCEPTOR_FD_ACQUIRE
//   COMMON_INTERCEPTOR_FD_RELEASE
//   COMMON_INTERCEPTOR_FD_ACCESS
//   COMMON_INTERCEPTOR_SET_THREAD_NAME
//   COMMON_INTERCEPTOR_ON_DLOPEN
//   COMMON_INTERCEPTOR_ON_EXIT
//   COMMON_INTERCEPTOR_MUTEX_PRE_LOCK
//   COMMON_INTERCEPTOR_MUTEX_POST_LOCK
//   COMMON_INTERCEPTOR_MUTEX_UNLOCK
//   COMMON_INTERCEPTOR_MUTEX_REPAIR
//   COMMON_INTERCEPTOR_SET_PTHREAD_NAME
//   COMMON_INTERCEPTOR_HANDLE_RECVMSG
//   COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED
//   COMMON_INTERCEPTOR_MEMSET_IMPL
//   COMMON_INTERCEPTOR_MEMMOVE_IMPL
//   COMMON_INTERCEPTOR_MEMCPY_IMPL
//   COMMON_INTERCEPTOR_COPY_STRING
//   COMMON_INTERCEPTOR_STRNDUP_IMPL
//===----------------------------------------------------------------------===//

#include "interception/interception.h"
#include "sanitizer_addrhashmap.h"
#include "sanitizer_errno.h"
#include "sanitizer_placement_new.h"
#include "sanitizer_platform_interceptors.h"
#include "sanitizer_symbolizer.h"
#include "sanitizer_tls_get_addr.h"

#include <stdarg.h>

#if SANITIZER_INTERCEPTOR_HOOKS
#define CALL_WEAK_INTERCEPTOR_HOOK(f, ...) f(__VA_ARGS__);
#define DECLARE_WEAK_INTERCEPTOR_HOOK(f, ...) \
  SANITIZER_INTERFACE_WEAK_DEF(void, f, __VA_ARGS__) {}
#else
#define DECLARE_WEAK_INTERCEPTOR_HOOK(f, ...)
#define CALL_WEAK_INTERCEPTOR_HOOK(f, ...)

#endif  // SANITIZER_INTERCEPTOR_HOOKS

#if SANITIZER_WINDOWS && !defined(va_copy)
#define va_copy(dst, src) ((dst) = (src))
#endif // _WIN32

#if SANITIZER_FREEBSD
#define pthread_setname_np pthread_set_name_np
#define inet_aton __inet_aton
#define inet_pton __inet_pton
#define iconv __bsd_iconv
#endif

// Platform-specific options.
#if SANITIZER_MAC
namespace __sanitizer {
bool PlatformHasDifferentMemcpyAndMemmove();
}
#define PLATFORM_HAS_DIFFERENT_MEMCPY_AND_MEMMOVE \
  (__sanitizer::PlatformHasDifferentMemcpyAndMemmove())
#elif SANITIZER_WINDOWS64
#define PLATFORM_HAS_DIFFERENT_MEMCPY_AND_MEMMOVE false
#else
#define PLATFORM_HAS_DIFFERENT_MEMCPY_AND_MEMMOVE true
#endif  // SANITIZER_MAC

#ifndef COMMON_INTERCEPTOR_INITIALIZE_RANGE
#define COMMON_INTERCEPTOR_INITIALIZE_RANGE(p, size) {}
#endif

#ifndef COMMON_INTERCEPTOR_UNPOISON_PARAM
#define COMMON_INTERCEPTOR_UNPOISON_PARAM(count) {}
#endif

#ifndef COMMON_INTERCEPTOR_FD_ACCESS
#define COMMON_INTERCEPTOR_FD_ACCESS(ctx, fd) {}
#endif

#ifndef COMMON_INTERCEPTOR_MUTEX_PRE_LOCK
#define COMMON_INTERCEPTOR_MUTEX_PRE_LOCK(ctx, m) {}
#endif

#ifndef COMMON_INTERCEPTOR_MUTEX_POST_LOCK
#define COMMON_INTERCEPTOR_MUTEX_POST_LOCK(ctx, m) {}
#endif

#ifndef COMMON_INTERCEPTOR_MUTEX_UNLOCK
#define COMMON_INTERCEPTOR_MUTEX_UNLOCK(ctx, m) {}
#endif

#ifndef COMMON_INTERCEPTOR_MUTEX_REPAIR
#define COMMON_INTERCEPTOR_MUTEX_REPAIR(ctx, m) {}
#endif

#ifndef COMMON_INTERCEPTOR_MUTEX_INVALID
#define COMMON_INTERCEPTOR_MUTEX_INVALID(ctx, m) {}
#endif

#ifndef COMMON_INTERCEPTOR_HANDLE_RECVMSG
#define COMMON_INTERCEPTOR_HANDLE_RECVMSG(ctx, msg) ((void)(msg))
#endif

#ifndef COMMON_INTERCEPTOR_FILE_OPEN
#define COMMON_INTERCEPTOR_FILE_OPEN(ctx, file, path) {}
#endif

#ifndef COMMON_INTERCEPTOR_FILE_CLOSE
#define COMMON_INTERCEPTOR_FILE_CLOSE(ctx, file) {}
#endif

#ifndef COMMON_INTERCEPTOR_LIBRARY_LOADED
#define COMMON_INTERCEPTOR_LIBRARY_LOADED(filename, handle) {}
#endif

#ifndef COMMON_INTERCEPTOR_LIBRARY_UNLOADED
#define COMMON_INTERCEPTOR_LIBRARY_UNLOADED() {}
#endif

#ifndef COMMON_INTERCEPTOR_ENTER_NOIGNORE
#define COMMON_INTERCEPTOR_ENTER_NOIGNORE(ctx, ...) \
  COMMON_INTERCEPTOR_ENTER(ctx, __VA_ARGS__)
#endif

#ifndef COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED
#define COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED (0)
#endif

#define COMMON_INTERCEPTOR_READ_STRING(ctx, s, n)                   \
    COMMON_INTERCEPTOR_READ_RANGE((ctx), (s),                       \
      common_flags()->strict_string_checks ? (REAL(strlen)(s)) + 1 : (n) )

#ifndef COMMON_INTERCEPTOR_ON_DLOPEN
#define COMMON_INTERCEPTOR_ON_DLOPEN(filename, flag) \
  CheckNoDeepBind(filename, flag);
#endif

#ifndef COMMON_INTERCEPTOR_GET_TLS_RANGE
#define COMMON_INTERCEPTOR_GET_TLS_RANGE(begin, end) *begin = *end = 0;
#endif

#ifndef COMMON_INTERCEPTOR_ACQUIRE
#define COMMON_INTERCEPTOR_ACQUIRE(ctx, u) {}
#endif

#ifndef COMMON_INTERCEPTOR_RELEASE
#define COMMON_INTERCEPTOR_RELEASE(ctx, u) {}
#endif

#ifndef COMMON_INTERCEPTOR_USER_CALLBACK_START
#define COMMON_INTERCEPTOR_USER_CALLBACK_START() {}
#endif

#ifndef COMMON_INTERCEPTOR_USER_CALLBACK_END
#define COMMON_INTERCEPTOR_USER_CALLBACK_END() {}
#endif

#ifdef SANITIZER_NLDBL_VERSION
#define COMMON_INTERCEPT_FUNCTION_LDBL(fn)                          \
    COMMON_INTERCEPT_FUNCTION_VER(fn, SANITIZER_NLDBL_VERSION)
#else
#define COMMON_INTERCEPT_FUNCTION_LDBL(fn)                          \
    COMMON_INTERCEPT_FUNCTION(fn)
#endif

#ifndef COMMON_INTERCEPTOR_MEMSET_IMPL
#define COMMON_INTERCEPTOR_MEMSET_IMPL(ctx, dst, v, size) \
  {                                                       \
    if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)        \
      return internal_memset(dst, v, size);               \
    COMMON_INTERCEPTOR_ENTER(ctx, memset, dst, v, size);  \
    if (common_flags()->intercept_intrin)                 \
      COMMON_INTERCEPTOR_WRITE_RANGE(ctx, dst, size);     \
    return REAL(memset)(dst, v, size);                    \
  }
#endif

#ifndef COMMON_INTERCEPTOR_MEMMOVE_IMPL
#define COMMON_INTERCEPTOR_MEMMOVE_IMPL(ctx, dst, src, size) \
  {                                                          \
    if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)           \
      return internal_memmove(dst, src, size);               \
    COMMON_INTERCEPTOR_ENTER(ctx, memmove, dst, src, size);  \
    if (common_flags()->intercept_intrin) {                  \
      COMMON_INTERCEPTOR_WRITE_RANGE(ctx, dst, size);        \
      COMMON_INTERCEPTOR_READ_RANGE(ctx, src, size);         \
    }                                                        \
    return REAL(memmove)(dst, src, size);                    \
  }
#endif

#ifndef COMMON_INTERCEPTOR_MEMCPY_IMPL
#define COMMON_INTERCEPTOR_MEMCPY_IMPL(ctx, dst, src, size) \
  {                                                         \
    if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED) {        \
      return internal_memmove(dst, src, size);              \
    }                                                       \
    COMMON_INTERCEPTOR_ENTER(ctx, memcpy, dst, src, size);  \
    if (common_flags()->intercept_intrin) {                 \
      COMMON_INTERCEPTOR_WRITE_RANGE(ctx, dst, size);       \
      COMMON_INTERCEPTOR_READ_RANGE(ctx, src, size);        \
    }                                                       \
    return REAL(memcpy)(dst, src, size);                    \
  }
#endif

#ifndef COMMON_INTERCEPTOR_COPY_STRING
#define COMMON_INTERCEPTOR_COPY_STRING(ctx, to, from, size) {}
#endif

#ifndef COMMON_INTERCEPTOR_STRNDUP_IMPL
#define COMMON_INTERCEPTOR_STRNDUP_IMPL(ctx, s, size)                         \
  COMMON_INTERCEPTOR_ENTER(ctx, strndup, s, size);                            \
  uptr copy_length = internal_strnlen(s, size);                               \
  char *new_mem = (char *)WRAP(malloc)(copy_length + 1);                      \
  if (common_flags()->intercept_strndup) {                                    \
    COMMON_INTERCEPTOR_READ_STRING(ctx, s, Min(size, copy_length + 1));       \
  }                                                                           \
  COMMON_INTERCEPTOR_COPY_STRING(ctx, new_mem, s, copy_length);               \
  internal_memcpy(new_mem, s, copy_length);                                   \
  new_mem[copy_length] = '\0';                                                \
  return new_mem;
#endif

struct FileMetadata {
  // For open_memstream().
  char **addr;
  SIZE_T *size;
};

struct CommonInterceptorMetadata {
  enum {
    CIMT_INVALID = 0,
    CIMT_FILE
  } type;
  union {
    FileMetadata file;
  };
};

typedef AddrHashMap<CommonInterceptorMetadata, 31051> MetadataHashMap;

static MetadataHashMap *interceptor_metadata_map;

#if SI_POSIX
UNUSED static void SetInterceptorMetadata(__sanitizer_FILE *addr,
                                          const FileMetadata &file) {
  MetadataHashMap::Handle h(interceptor_metadata_map, (uptr)addr);
  CHECK(h.created());
  h->type = CommonInterceptorMetadata::CIMT_FILE;
  h->file = file;
}

UNUSED static const FileMetadata *GetInterceptorMetadata(
    __sanitizer_FILE *addr) {
  MetadataHashMap::Handle h(interceptor_metadata_map, (uptr)addr,
                            /* remove */ false,
                            /* create */ false);
  if (h.exists()) {
    CHECK(!h.created());
    CHECK(h->type == CommonInterceptorMetadata::CIMT_FILE);
    return &h->file;
  } else {
    return 0;
  }
}

UNUSED static void DeleteInterceptorMetadata(void *addr) {
  MetadataHashMap::Handle h(interceptor_metadata_map, (uptr)addr, true);
  CHECK(h.exists());
}
#endif  // SI_POSIX

#if SANITIZER_INTERCEPT_STRLEN
INTERCEPTOR(SIZE_T, strlen, const char *s) {
  // Sometimes strlen is called prior to InitializeCommonInterceptors,
  // in which case the REAL(strlen) typically used in
  // COMMON_INTERCEPTOR_ENTER will fail.  We use internal_strlen here
  // to handle that.
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_strlen(s);
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strlen, s);
  SIZE_T result = REAL(strlen)(s);
  if (common_flags()->intercept_strlen)
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s, result + 1);
  return result;
}
#define INIT_STRLEN COMMON_INTERCEPT_FUNCTION(strlen)
#else
#define INIT_STRLEN
#endif

#if SANITIZER_INTERCEPT_STRNLEN
INTERCEPTOR(SIZE_T, strnlen, const char *s, SIZE_T maxlen) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strnlen, s, maxlen);
  SIZE_T length = REAL(strnlen)(s, maxlen);
  if (common_flags()->intercept_strlen)
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s, Min(length + 1, maxlen));
  return length;
}
#define INIT_STRNLEN COMMON_INTERCEPT_FUNCTION(strnlen)
#else
#define INIT_STRNLEN
#endif

#if SANITIZER_INTERCEPT_STRNDUP
INTERCEPTOR(char*, strndup, const char *s, uptr size) {
  void *ctx;
  COMMON_INTERCEPTOR_STRNDUP_IMPL(ctx, s, size);
}
#define INIT_STRNDUP COMMON_INTERCEPT_FUNCTION(strndup)
#else
#define INIT_STRNDUP
#endif // SANITIZER_INTERCEPT_STRNDUP

#if SANITIZER_INTERCEPT___STRNDUP
INTERCEPTOR(char*, __strndup, const char *s, uptr size) {
  void *ctx;
  COMMON_INTERCEPTOR_STRNDUP_IMPL(ctx, s, size);
}
#define INIT___STRNDUP COMMON_INTERCEPT_FUNCTION(__strndup)
#else
#define INIT___STRNDUP
#endif // SANITIZER_INTERCEPT___STRNDUP

#if SANITIZER_INTERCEPT_TEXTDOMAIN
INTERCEPTOR(char*, textdomain, const char *domainname) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, textdomain, domainname);
  if (domainname) COMMON_INTERCEPTOR_READ_STRING(ctx, domainname, 0);
  char *domain = REAL(textdomain)(domainname);
  if (domain) {
    COMMON_INTERCEPTOR_INITIALIZE_RANGE(domain, REAL(strlen)(domain) + 1);
  }
  return domain;
}
#define INIT_TEXTDOMAIN COMMON_INTERCEPT_FUNCTION(textdomain)
#else
#define INIT_TEXTDOMAIN
#endif

#if SANITIZER_INTERCEPT_STRCMP
static inline int CharCmpX(unsigned char c1, unsigned char c2) {
  return (c1 == c2) ? 0 : (c1 < c2) ? -1 : 1;
}

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strcmp, uptr called_pc,
                              const char *s1, const char *s2, int result)

INTERCEPTOR(int, strcmp, const char *s1, const char *s2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strcmp, s1, s2);
  unsigned char c1, c2;
  uptr i;
  for (i = 0;; i++) {
    c1 = (unsigned char)s1[i];
    c2 = (unsigned char)s2[i];
    if (c1 != c2 || c1 == '\0') break;
  }
  COMMON_INTERCEPTOR_READ_STRING(ctx, s1, i + 1);
  COMMON_INTERCEPTOR_READ_STRING(ctx, s2, i + 1);
  int result = CharCmpX(c1, c2);
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strcmp, GET_CALLER_PC(), s1,
                             s2, result);
  return result;
}

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strncmp, uptr called_pc,
                              const char *s1, const char *s2, uptr n,
                              int result)

INTERCEPTOR(int, strncmp, const char *s1, const char *s2, uptr size) {
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_strncmp(s1, s2, size);
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strncmp, s1, s2, size);
  unsigned char c1 = 0, c2 = 0;
  uptr i;
  for (i = 0; i < size; i++) {
    c1 = (unsigned char)s1[i];
    c2 = (unsigned char)s2[i];
    if (c1 != c2 || c1 == '\0') break;
  }
  uptr i1 = i;
  uptr i2 = i;
  if (common_flags()->strict_string_checks) {
    for (; i1 < size && s1[i1]; i1++) {}
    for (; i2 < size && s2[i2]; i2++) {}
  }
  COMMON_INTERCEPTOR_READ_RANGE((ctx), (s1), Min(i1 + 1, size));
  COMMON_INTERCEPTOR_READ_RANGE((ctx), (s2), Min(i2 + 1, size));
  int result = CharCmpX(c1, c2);
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strncmp, GET_CALLER_PC(), s1,
                             s2, size, result);
  return result;
}

#define INIT_STRCMP COMMON_INTERCEPT_FUNCTION(strcmp)
#define INIT_STRNCMP COMMON_INTERCEPT_FUNCTION(strncmp)
#else
#define INIT_STRCMP
#define INIT_STRNCMP
#endif

#if SANITIZER_INTERCEPT_STRCASECMP
static inline int CharCaseCmp(unsigned char c1, unsigned char c2) {
  int c1_low = ToLower(c1);
  int c2_low = ToLower(c2);
  return c1_low - c2_low;
}

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strcasecmp, uptr called_pc,
                              const char *s1, const char *s2, int result)

INTERCEPTOR(int, strcasecmp, const char *s1, const char *s2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strcasecmp, s1, s2);
  unsigned char c1 = 0, c2 = 0;
  uptr i;
  for (i = 0;; i++) {
    c1 = (unsigned char)s1[i];
    c2 = (unsigned char)s2[i];
    if (CharCaseCmp(c1, c2) != 0 || c1 == '\0') break;
  }
  COMMON_INTERCEPTOR_READ_STRING(ctx, s1, i + 1);
  COMMON_INTERCEPTOR_READ_STRING(ctx, s2, i + 1);
  int result = CharCaseCmp(c1, c2);
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strcasecmp, GET_CALLER_PC(),
                             s1, s2, result);
  return result;
}

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strncasecmp, uptr called_pc,
                              const char *s1, const char *s2, uptr size,
                              int result)

INTERCEPTOR(int, strncasecmp, const char *s1, const char *s2, SIZE_T size) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strncasecmp, s1, s2, size);
  unsigned char c1 = 0, c2 = 0;
  uptr i;
  for (i = 0; i < size; i++) {
    c1 = (unsigned char)s1[i];
    c2 = (unsigned char)s2[i];
    if (CharCaseCmp(c1, c2) != 0 || c1 == '\0') break;
  }
  uptr i1 = i;
  uptr i2 = i;
  if (common_flags()->strict_string_checks) {
    for (; i1 < size && s1[i1]; i1++) {}
    for (; i2 < size && s2[i2]; i2++) {}
  }
  COMMON_INTERCEPTOR_READ_RANGE((ctx), (s1), Min(i1 + 1, size));
  COMMON_INTERCEPTOR_READ_RANGE((ctx), (s2), Min(i2 + 1, size));
  int result = CharCaseCmp(c1, c2);
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strncasecmp, GET_CALLER_PC(),
                             s1, s2, size, result);
  return result;
}

#define INIT_STRCASECMP COMMON_INTERCEPT_FUNCTION(strcasecmp)
#define INIT_STRNCASECMP COMMON_INTERCEPT_FUNCTION(strncasecmp)
#else
#define INIT_STRCASECMP
#define INIT_STRNCASECMP
#endif

#if SANITIZER_INTERCEPT_STRSTR || SANITIZER_INTERCEPT_STRCASESTR
static inline void StrstrCheck(void *ctx, char *r, const char *s1,
                               const char *s2) {
    uptr len1 = REAL(strlen)(s1);
    uptr len2 = REAL(strlen)(s2);
    COMMON_INTERCEPTOR_READ_STRING(ctx, s1, r ? r - s1 + len2 : len1 + 1);
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s2, len2 + 1);
}
#endif

#if SANITIZER_INTERCEPT_STRSTR

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strstr, uptr called_pc,
                              const char *s1, const char *s2, char *result)

INTERCEPTOR(char*, strstr, const char *s1, const char *s2) {
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_strstr(s1, s2);
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strstr, s1, s2);
  char *r = REAL(strstr)(s1, s2);
  if (common_flags()->intercept_strstr)
    StrstrCheck(ctx, r, s1, s2);
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strstr, GET_CALLER_PC(), s1,
                             s2, r);
  return r;
}

#define INIT_STRSTR COMMON_INTERCEPT_FUNCTION(strstr);
#else
#define INIT_STRSTR
#endif

#if SANITIZER_INTERCEPT_STRCASESTR

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strcasestr, uptr called_pc,
                              const char *s1, const char *s2, char *result)

INTERCEPTOR(char*, strcasestr, const char *s1, const char *s2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strcasestr, s1, s2);
  char *r = REAL(strcasestr)(s1, s2);
  if (common_flags()->intercept_strstr)
    StrstrCheck(ctx, r, s1, s2);
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_strcasestr, GET_CALLER_PC(),
                             s1, s2, r);
  return r;
}

#define INIT_STRCASESTR COMMON_INTERCEPT_FUNCTION(strcasestr);
#else
#define INIT_STRCASESTR
#endif

#if SANITIZER_INTERCEPT_STRTOK

INTERCEPTOR(char*, strtok, char *str, const char *delimiters) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strtok, str, delimiters);
  if (!common_flags()->intercept_strtok) {
    return REAL(strtok)(str, delimiters);
  }
  if (common_flags()->strict_string_checks) {
    // If strict_string_checks is enabled, we check the whole first argument
    // string on the first call (strtok saves this string in a static buffer
    // for subsequent calls). We do not need to check strtok's result.
    // As the delimiters can change, we check them every call.
    if (str != nullptr) {
      COMMON_INTERCEPTOR_READ_RANGE(ctx, str, REAL(strlen)(str) + 1);
    }
    COMMON_INTERCEPTOR_READ_RANGE(ctx, delimiters,
                                  REAL(strlen)(delimiters) + 1);
    return REAL(strtok)(str, delimiters);
  } else {
    // However, when strict_string_checks is disabled we cannot check the
    // whole string on the first call. Instead, we check the result string
    // which is guaranteed to be a NULL-terminated substring of the first
    // argument. We also conservatively check one character of str and the
    // delimiters.
    if (str != nullptr) {
      COMMON_INTERCEPTOR_READ_STRING(ctx, str, 1);
    }
    COMMON_INTERCEPTOR_READ_RANGE(ctx, delimiters, 1);
    char *result = REAL(strtok)(str, delimiters);
    if (result != nullptr) {
      COMMON_INTERCEPTOR_READ_RANGE(ctx, result, REAL(strlen)(result) + 1);
    } else if (str != nullptr) {
      // No delimiter were found, it's safe to assume that the entire str was
      // scanned.
      COMMON_INTERCEPTOR_READ_RANGE(ctx, str, REAL(strlen)(str) + 1);
    }
    return result;
  }
}

#define INIT_STRTOK COMMON_INTERCEPT_FUNCTION(strtok)
#else
#define INIT_STRTOK
#endif

#if SANITIZER_INTERCEPT_MEMMEM
DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_memmem, uptr called_pc,
                              const void *s1, SIZE_T len1, const void *s2,
                              SIZE_T len2, void *result)

INTERCEPTOR(void*, memmem, const void *s1, SIZE_T len1, const void *s2,
            SIZE_T len2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, memmem, s1, len1, s2, len2);
  void *r = REAL(memmem)(s1, len1, s2, len2);
  if (common_flags()->intercept_memmem) {
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s1, len1);
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s2, len2);
  }
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_memmem, GET_CALLER_PC(),
                             s1, len1, s2, len2, r);
  return r;
}

#define INIT_MEMMEM COMMON_INTERCEPT_FUNCTION(memmem);
#else
#define INIT_MEMMEM
#endif  // SANITIZER_INTERCEPT_MEMMEM

#if SANITIZER_INTERCEPT_STRCHR
INTERCEPTOR(char*, strchr, const char *s, int c) {
  void *ctx;
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_strchr(s, c);
  COMMON_INTERCEPTOR_ENTER(ctx, strchr, s, c);
  char *result = REAL(strchr)(s, c);
  if (common_flags()->intercept_strchr) {
    // Keep strlen as macro argument, as macro may ignore it.
    COMMON_INTERCEPTOR_READ_STRING(ctx, s,
      (result ? result - s : REAL(strlen)(s)) + 1);
  }
  return result;
}
#define INIT_STRCHR COMMON_INTERCEPT_FUNCTION(strchr)
#else
#define INIT_STRCHR
#endif

#if SANITIZER_INTERCEPT_STRCHRNUL
INTERCEPTOR(char*, strchrnul, const char *s, int c) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strchrnul, s, c);
  char *result = REAL(strchrnul)(s, c);
  uptr len = result - s + 1;
  if (common_flags()->intercept_strchr)
    COMMON_INTERCEPTOR_READ_STRING(ctx, s, len);
  return result;
}
#define INIT_STRCHRNUL COMMON_INTERCEPT_FUNCTION(strchrnul)
#else
#define INIT_STRCHRNUL
#endif

#if SANITIZER_INTERCEPT_STRRCHR
INTERCEPTOR(char*, strrchr, const char *s, int c) {
  void *ctx;
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_strrchr(s, c);
  COMMON_INTERCEPTOR_ENTER(ctx, strrchr, s, c);
  if (common_flags()->intercept_strchr)
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s, REAL(strlen)(s) + 1);
  return REAL(strrchr)(s, c);
}
#define INIT_STRRCHR COMMON_INTERCEPT_FUNCTION(strrchr)
#else
#define INIT_STRRCHR
#endif

#if SANITIZER_INTERCEPT_STRSPN
INTERCEPTOR(SIZE_T, strspn, const char *s1, const char *s2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strspn, s1, s2);
  SIZE_T r = REAL(strspn)(s1, s2);
  if (common_flags()->intercept_strspn) {
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s2, REAL(strlen)(s2) + 1);
    COMMON_INTERCEPTOR_READ_STRING(ctx, s1, r + 1);
  }
  return r;
}

INTERCEPTOR(SIZE_T, strcspn, const char *s1, const char *s2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strcspn, s1, s2);
  SIZE_T r = REAL(strcspn)(s1, s2);
  if (common_flags()->intercept_strspn) {
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s2, REAL(strlen)(s2) + 1);
    COMMON_INTERCEPTOR_READ_STRING(ctx, s1, r + 1);
  }
  return r;
}

#define INIT_STRSPN \
  COMMON_INTERCEPT_FUNCTION(strspn); \
  COMMON_INTERCEPT_FUNCTION(strcspn);
#else
#define INIT_STRSPN
#endif

#if SANITIZER_INTERCEPT_STRPBRK
INTERCEPTOR(char *, strpbrk, const char *s1, const char *s2) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strpbrk, s1, s2);
  char *r = REAL(strpbrk)(s1, s2);
  if (common_flags()->intercept_strpbrk) {
    COMMON_INTERCEPTOR_READ_RANGE(ctx, s2, REAL(strlen)(s2) + 1);
    COMMON_INTERCEPTOR_READ_STRING(ctx, s1,
        r ? r - s1 + 1 : REAL(strlen)(s1) + 1);
  }
  return r;
}

#define INIT_STRPBRK COMMON_INTERCEPT_FUNCTION(strpbrk);
#else
#define INIT_STRPBRK
#endif

#if SANITIZER_INTERCEPT_MEMSET
INTERCEPTOR(void *, memset, void *dst, int v, uptr size) {
  void *ctx;
  COMMON_INTERCEPTOR_MEMSET_IMPL(ctx, dst, v, size);
}

#define INIT_MEMSET COMMON_INTERCEPT_FUNCTION(memset)
#else
#define INIT_MEMSET
#endif

#if SANITIZER_INTERCEPT_MEMMOVE
INTERCEPTOR(void *, memmove, void *dst, const void *src, uptr size) {
  void *ctx;
  COMMON_INTERCEPTOR_MEMMOVE_IMPL(ctx, dst, src, size);
}

#define INIT_MEMMOVE COMMON_INTERCEPT_FUNCTION(memmove)
#else
#define INIT_MEMMOVE
#endif

#if SANITIZER_INTERCEPT_MEMCPY
INTERCEPTOR(void *, memcpy, void *dst, const void *src, uptr size) {
  // On OS X, calling internal_memcpy here will cause memory corruptions,
  // because memcpy and memmove are actually aliases of the same
  // implementation.  We need to use internal_memmove here.
  // N.B.: If we switch this to internal_ we'll have to use internal_memmove
  // due to memcpy being an alias of memmove on OS X.
  void *ctx;
  if (PLATFORM_HAS_DIFFERENT_MEMCPY_AND_MEMMOVE) {
    COMMON_INTERCEPTOR_MEMCPY_IMPL(ctx, dst, src, size);
  } else {
    COMMON_INTERCEPTOR_MEMMOVE_IMPL(ctx, dst, src, size);
  }
}

#define INIT_MEMCPY                                  \
  do {                                               \
    if (PLATFORM_HAS_DIFFERENT_MEMCPY_AND_MEMMOVE) { \
      COMMON_INTERCEPT_FUNCTION(memcpy);             \
    } else {                                         \
      ASSIGN_REAL(memcpy, memmove);                  \
    }                                                \
    CHECK(REAL(memcpy));                             \
  } while (false)

#else
#define INIT_MEMCPY
#endif

#if SANITIZER_INTERCEPT_MEMCMP

DECLARE_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_memcmp, uptr called_pc,
                              const void *s1, const void *s2, uptr n,
                              int result)

INTERCEPTOR(int, memcmp, const void *a1, const void *a2, uptr size) {
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_memcmp(a1, a2, size);
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, memcmp, a1, a2, size);
  if (common_flags()->intercept_memcmp) {
    if (common_flags()->strict_memcmp) {
      // Check the entire regions even if the first bytes of the buffers are
      // different.
      COMMON_INTERCEPTOR_READ_RANGE(ctx, a1, size);
      COMMON_INTERCEPTOR_READ_RANGE(ctx, a2, size);
      // Fallthrough to REAL(memcmp) below.
    } else {
      unsigned char c1 = 0, c2 = 0;
      const unsigned char *s1 = (const unsigned char*)a1;
      const unsigned char *s2 = (const unsigned char*)a2;
      uptr i;
      for (i = 0; i < size; i++) {
        c1 = s1[i];
        c2 = s2[i];
        if (c1 != c2) break;
      }
      COMMON_INTERCEPTOR_READ_RANGE(ctx, s1, Min(i + 1, size));
      COMMON_INTERCEPTOR_READ_RANGE(ctx, s2, Min(i + 1, size));
      int r = CharCmpX(c1, c2);
      CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_memcmp, GET_CALLER_PC(),
                                 a1, a2, size, r);
      return r;
    }
  }
  int result = REAL(memcmp(a1, a2, size));
  CALL_WEAK_INTERCEPTOR_HOOK(__sanitizer_weak_hook_memcmp, GET_CALLER_PC(), a1,
                             a2, size, result);
  return result;
}

#define INIT_MEMCMP COMMON_INTERCEPT_FUNCTION(memcmp)
#else
#define INIT_MEMCMP
#endif

#if SANITIZER_INTERCEPT_MEMCHR
INTERCEPTOR(void*, memchr, const void *s, int c, SIZE_T n) {
  if (COMMON_INTERCEPTOR_NOTHING_IS_INITIALIZED)
    return internal_memchr(s, c, n);
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, memchr, s, c, n);
#if SANITIZER_WINDOWS
  void *res;
  if (REAL(memchr)) {
    res = REAL(memchr)(s, c, n);
  } else {
    res = internal_memchr(s, c, n);
  }
#else
  void *res = REAL(memchr)(s, c, n);
#endif
  uptr len = res ? (char *)res - (const char *)s + 1 : n;
  COMMON_INTERCEPTOR_READ_RANGE(ctx, s, len);
  return res;
}

#define INIT_MEMCHR COMMON_INTERCEPT_FUNCTION(memchr)
#else
#define INIT_MEMCHR
#endif

#if SANITIZER_INTERCEPT_MEMRCHR
INTERCEPTOR(void*, memrchr, const void *s, int c, SIZE_T n) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, memrchr, s, c, n);
  COMMON_INTERCEPTOR_READ_RANGE(ctx, s, n);
  return REAL(memrchr)(s, c, n);
}

#define INIT_MEMRCHR COMMON_INTERCEPT_FUNCTION(memrchr)
#else
#define INIT_MEMRCHR
#endif

UNUSED static inline void FixRealStrtolEndptr(const char *nptr, char **endptr) {
  CHECK(endptr);
  if (nptr == *endptr) {
    // No digits were found at strtol call, we need to find out the last
    // symbol accessed by strtoll on our own.
    // We get this symbol by skipping leading blanks and optional +/- sign.
    while (IsSpace(*nptr)) nptr++;
    if (*nptr == '+' || *nptr == '-') nptr++;
    *endptr = const_cast<char *>(nptr);
  }
  CHECK(*endptr >= nptr);
}

UNUSED static inline void StrtolFixAndCheck(void *ctx, const char *nptr,
                             char **endptr, char *real_endptr, int base) {
  if (endptr) {
    *endptr = real_endptr;
    COMMON_INTERCEPTOR_WRITE_RANGE(ctx, endptr, sizeof(*endptr));
  }
  // If base has unsupported value, strtol can exit with EINVAL
  // without reading any characters. So do additional checks only
  // if base is valid.
  bool is_valid_base = (base == 0) || (2 <= base && base <= 36);
  if (is_valid_base) {
    FixRealStrtolEndptr(nptr, &real_endptr);
  }
  COMMON_INTERCEPTOR_READ_STRING(ctx, nptr, is_valid_base ?
                                 (real_endptr - nptr) + 1 : 0);
}

#if SANITIZER_INTERCEPT_STRTOIMAX
INTERCEPTOR(INTMAX_T, strtoimax, const char *nptr, char **endptr, int base) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strtoimax, nptr, endptr, base);
  // FIXME: under ASan the call below may write to freed memory and corrupt
  // its metadata. See
  // https://github.com/google/sanitizers/issues/321.
  char *real_endptr;
  INTMAX_T res = REAL(strtoimax)(nptr, &real_endptr, base);
  StrtolFixAndCheck(ctx, nptr, endptr, real_endptr, base);
  return res;
}

INTERCEPTOR(INTMAX_T, strtoumax, const char *nptr, char **endptr, int base) {
  void *ctx;
  COMMON_INTERCEPTOR_ENTER(ctx, strtoumax, nptr, endptr, base);
  // FIXME: under ASan the call below may write to freed memory and corrupt
  // its metadata. See
  // https://github.com/google/sanitizers/issues/321.
  char *real_endptr;
  INTMAX_T res = REAL(strtoumax)(nptr, &real_endptr, base);
  StrtolFixAndCheck(ctx, nptr, endptr, real_endptr, base);
  return res;
}

#define INIT_STRTOIMAX                  \
  COMMON_INTERCEPT_FUNCTION(strtoimax); \
  COMMON_INTERCEPT_FUNCTION(strtoumax);
#else
#define INIT_STRTOIMAX
#endif


void InitializeCommonStringInterceptors() {
  static u64 metadata_mem[sizeof(MetadataHashMap) / sizeof(u64) + 1];
  interceptor_metadata_map = new((void *)&metadata_mem) MetadataHashMap();

  INIT_TEXTDOMAIN;
  INIT_STRLEN;
  INIT_STRNLEN;
  INIT_STRNDUP;
  INIT___STRNDUP;
  INIT_STRCMP;
  INIT_STRNCMP;
  INIT_STRCASECMP;
  INIT_STRNCASECMP;
  INIT_STRSTR;
  INIT_STRCASESTR;
  INIT_STRCHR;
  INIT_STRCHRNUL;
  INIT_STRRCHR;
  INIT_STRSPN;
  INIT_STRTOK;
  INIT_STRPBRK;
  INIT_MEMSET;
  INIT_MEMMOVE;
  INIT_MEMCPY;
  INIT_MEMCHR;
  INIT_MEMCMP;
  INIT_MEMRCHR;
  INIT_MEMMEM;
  INIT_STRTOIMAX;
}
