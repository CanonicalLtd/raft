AC_PREREQ(2.60)
AC_INIT([raft], [0.9.18])
AC_LANG([C])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([ac])

AM_INIT_AUTOMAKE([subdir-objects -Wall -Wno-portability foreign])
AM_SILENT_RULES([yes])
AC_USE_SYSTEM_EXTENSIONS # Defines _GNU_SOURCE and similar

LT_INIT

AC_CANONICAL_HOST

build_linux=no
build_windows=no
build_mac=no

#Detect target OS
AS_CASE([$host_os],[linux*],[build_linux=yes])
AS_CASE([$host_os],[mingw*],[build_windows=yes])

AM_CONDITIONAL([LINUX], [test "x$build_linux" = "xyes"])
AM_CONDITIONAL([WINDOWS], [test "x$build_windows" = "xyes"])

# The libuv raft_io implementation is built by default if libuv is found, unless
# explicitely disabled.
AC_ARG_ENABLE(uv, AS_HELP_STRING([--disable-uv], [do not build the libuv-based raft_io implementation]))
AS_IF([test "x$enable_uv" != "xno"],
      [
          AM_COND_IF([WINDOWS], 
          [
              UV_LIBS="-L$prefix/lib -luv -lpthread -lws2_32 -lpsapi -liphlpapi -lshell32 -luserenv -luser32 -lmswsock -ladvapi32"
              UV_CFLAGS="-I$prefix/include"
              AC_SUBST(UV_LIBS)
              AC_SUBST(UV_CFLAGS)
          ],
          [PKG_CHECK_MODULES(UV, [libuv >= 1.8.0], [have_uv=yes], [have_uv=no])])
        ],
      [have_uv=no])
AS_IF([test "x$enable_uv" = "xyes" -a "x$have_uv" = "xno"], [AC_MSG_ERROR([libuv required but not found])], [])
AM_CONDITIONAL(UV_ENABLED, test "x$have_uv" = "xyes")

# The fake I/O implementation and associated fixture is built by default, unless
# explicitely disabled.
AC_ARG_ENABLE(fixture, AS_HELP_STRING([--disable-fixture], [do not build the raft_fixture test helper]))
AM_CONDITIONAL(FIXTURE_ENABLED, test "x$enable_fixture" != "xno")

# The example program is optional.
AC_ARG_ENABLE(example, AS_HELP_STRING([--enable-example[=ARG]], [build the example program [default=no]]))
AS_IF([test "x$enable_example" = "xyes" -a "x$have_uv" = "xno"], [AC_MSG_ERROR([example program requires libuv])], [])
AM_CONDITIONAL(EXAMPLE_ENABLED, test "x$enable_example" = "xyes")

# The benchmark programs are optional.
AC_ARG_ENABLE(benchmark, AS_HELP_STRING([--enable-benchmark[=ARG]], [build the benchmark programs [default=no]]))
AM_CONDITIONAL(BENCHMARK_ENABLED, test "x$enable_benchmark" = "xyes")

# Whether to enable debugging code.
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug[=ARG]], [enable debugging [default=no]]))
AM_CONDITIONAL(DEBUG_ENABLED, test "x$enable_debug" = "xyes")

# Whether to enable memory sanitizer.
AC_ARG_ENABLE(sanitize, AS_HELP_STRING([--enable-sanitize[=ARG]], [enable code sanitizers [default=no]]))
AM_CONDITIONAL(SANITIZE_ENABLED, test x"$enable_sanitize" = x"yes")

# Whether to enable code coverage.
AX_CODE_COVERAGE

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h stdio.h assert.h unistd.h linux/io_uring.h linux/aio_abi.h])

# Check if zfs >= 0.8.0 is available (for direct I/O support).
AC_CHECK_PROG(have_zfs, zfs, yes)
AS_IF([test x"$have_zfs" = x"yes"],
   [AX_COMPARE_VERSION($(zfs version 2>/dev/null | cut -f 2 -d - | head -1), [ge], [0.8.0],
       [AC_DEFINE(RAFT_HAVE_ZFS_WITH_DIRECT_IO)], [])
   ],
   [])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions and definitions.
AC_CHECK_DECLS([UV_FS_O_CREAT], [], [], [[#include <uv.h>]])

# Enable large file support. This is mandatory in order to interoperate with
# libuv, which enables large file support by default, making the size of 'off_t'
# on 32-bit architecture be 8 bytes instead of the normal 4.
AC_SYS_LARGEFILE

CC_CHECK_CFLAGS_APPEND([-std=c11])
CC_CHECK_CFLAGS_APPEND([-g])
CC_CHECK_CFLAGS_APPEND([--mcet -fcf-protection])
CC_CHECK_CFLAGS_APPEND([--param=ssp-buffer-size=4])
CC_CHECK_CFLAGS_APPEND([-pipe])
CC_CHECK_CFLAGS_APPEND([-fno-strict-aliasing])
CC_CHECK_CFLAGS_APPEND([-fdiagnostics-color])
CC_CHECK_CFLAGS_APPEND([-fexceptions])
CC_CHECK_CFLAGS_APPEND([-fstack-clash-protection])
CC_CHECK_CFLAGS_APPEND([-fstack-protector-strong])
CC_CHECK_CFLAGS_APPEND([-fasynchronous-unwind-tables])
CC_CHECK_CFLAGS_APPEND([-fdiagnostics-show-option])
CC_CHECK_CFLAGS_APPEND([-Wall])
CC_CHECK_CFLAGS_APPEND([-Wextra])
CC_CHECK_CFLAGS_APPEND([-Wpedantic])
CC_CHECK_CFLAGS_APPEND([-Wimplicit-fallthrough=5])
CC_CHECK_CFLAGS_APPEND([-Wcast-align])
CC_CHECK_CFLAGS_APPEND([-Wstrict-prototypes])
CC_CHECK_CFLAGS_APPEND([-Wlogical-op])
CC_CHECK_CFLAGS_APPEND([-Wmissing-include-dirs])
CC_CHECK_CFLAGS_APPEND([-Wold-style-definition])
CC_CHECK_CFLAGS_APPEND([-Winit-self])
CC_CHECK_CFLAGS_APPEND([-Wfloat-equal])
CC_CHECK_CFLAGS_APPEND([-Wsuggest-attribute=noreturn])
CC_CHECK_CFLAGS_APPEND([-Wformat=2])
CC_CHECK_CFLAGS_APPEND([-Wshadow])
CC_CHECK_CFLAGS_APPEND([-Wendif-labels])
CC_CHECK_CFLAGS_APPEND([-Wdate-time])
CC_CHECK_CFLAGS_APPEND([-Wnested-externs])
CC_CHECK_CFLAGS_APPEND([-Wconversion])
AS_CASE([$host_os],[mingw*],[CC_CHECK_CFLAGS_APPEND([-D__USE_MINGW_ANSI_STDIO])])
AC_SUBST(AM_CFLAGS)

AC_CONFIG_FILES([raft.pc Makefile])
AC_OUTPUT
