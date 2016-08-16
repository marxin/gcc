dnl Check whether the target supports __atomic_operations.
AC_DEFUN([LIBGCC_CHECK_ATOMIC_OPERATION], [
  AC_CACHE_CHECK([whether the target supports atomic operations for $1B],
		 libgcc_cv_have_atomic_operations_$1, [
  libgcc_cv_have_atomic_operations_$1=no

  AC_LANG_CONFTEST(
  [AC_LANG_PROGRAM([[int foovar = 0;]], [[__atomic_fetch_add_$1 (&foovar, 1, 0);
  __atomic_fetch_or_$1 (&foovar, 1, 0)]])])
  if AC_TRY_COMMAND(${CC-cc} -Werror -S -o conftest.s conftest.c 1>&AS_MESSAGE_LOG_FD); then
      if grep __atomic_fetch_add_$1 conftest.s > /dev/null; then
	:
      else
	libgcc_cv_have_atomic_operations_$1=yes
      fi
    fi
    rm -f conftest.*
    ])
  if test $libgcc_cv_have_atomic_operations_$1 = yes; then
    AC_DEFINE(HAVE_ATOMIC_OPERATIONS_$1, 1,
	      [Define to 1 if the target supports atomic operations for $1B])
  fi])
