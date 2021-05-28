AC_PREREQ(2.60)
AC_INIT([raft], [0.10.1])
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
AS_CASE([$host_os],[darwin*],[build_mac=yes])

AM_CONDITIONAL([LINUX], [test "x$build_linux" = "xyes"])
AM_CONDITIONAL([WINDOWS], [test "x$build_windows" = "xyes"])
AM_CONDITIONAL([MAC], [test "x$build_mac" = "xyes"])

# The libuv raft_io implementation is built by default if libuv is found, unless
# explicitly disabled.
AC_ARG_ENABLE(uv, AS_HELP_STRING([--disable-uv], [do not build the libuv-based raft_io implementation]))
AS_IF([test "x$enable_uv" != "xno"],
      [PKG_CHECK_MODULES(UV, [libuv >= 1.18.0], [have_uv=yes], [have_uv=no])],
      [have_uv=no])
AS_IF([test "x$enable_uv" = "xyes" -a "x$have_uv" = "xno"], [AC_MSG_ERROR([libuv required but not found])], [])
AM_CONDITIONAL(UV_ENABLED, test "x$have_uv" = "xyes")

# The libuv raft_io implementation is built by default to compress snapshots if liblz4 is found, unless
# explicitly disabled.
AC_ARG_ENABLE(lz4, AS_HELP_STRING([--disable-lz4], [do not use lz4 compression]))

# Thanks to the OpenVPN configure.ac file for this part.
AS_IF([test "x$enable_lz4" != "xno"],
      # If this fails, we will do another test next.
      # We also add set LZ4_LIBS otherwise
      # linker will not know about the lz4 library
      [PKG_CHECK_MODULES(LZ4, [liblz4 >= 1.7.1], [have_lz4="yes"], [LZ4_LIBS="-llz4"])])
if test "${have_lz4}" != "yes" -a "${enable_lz4}" != "no"; then
    AC_CHECK_HEADERS([lz4.h],
                     [have_lz4h="yes"],
                     [])
    if test "${have_lz4h}" = "yes" ; then
        AC_MSG_CHECKING([additionally if system LZ4 version >= 1.7.1])
        AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[
#include <lz4.h>
                         ]],
                         [[
/* Version encoding: MMNNPP (Major miNor Patch) - see lz4.h for details */
#if LZ4_VERSION_NUMBER < 10701L
#error LZ4 is too old
#endif
                         ]]
                        )],
         [
             AC_MSG_RESULT([ok])
             have_lz4="yes"
         ],
         [
             AC_MSG_RESULT([system LZ4 library is too old])
             have_lz4="no"
         ]
        )
    fi
fi

AS_IF([test "x$enable_lz4" != "xno" -a "x$have_lz4" = "xno"],
      [AC_MSG_ERROR([liblz4 required but not found])], [])
AM_CONDITIONAL(LZ4_AVAILABLE, test "x$have_lz4" = "xyes")
AM_CONDITIONAL(LZ4_ENABLED, test "x$enable_lz4" != "xno")

# The fake I/O implementation and associated fixture is built by default, unless
# explicitly disabled.
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
   [AX_COMPARE_VERSION($(cat /sys/module/zfs/version | cut -f 1 -d -), [ge], [0.8.0],
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

CC_CHECK_FLAGS_APPEND([AM_CFLAGS],[CFLAGS],[ \
  -std=c11 \
  -g \
  --mcet \
  -fcf-protection \
  --param=ssp-buffer-size=4 \
  -pipe \
  -fno-strict-aliasing \
  -fdiagnostics-color \
  -fexceptions \
  -fstack-clash-protection \
  -fstack-protector-strong \
  -fasynchronous-unwind-tables \
  -fdiagnostics-show-option \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Wimplicit-fallthrough=5 \
  -Wcast-align \
  -Wstrict-prototypes \
  -Wlogical-op \
  -Wmissing-include-dirs \
  -Wold-style-definition \
  -Winit-self \
  -Wfloat-equal \
  -Wsuggest-attribute=noreturn \
  -Wformat=2 \
  -Wendif-labels \
  -Wdate-time \
  -Wnested-externs \
  -Wconversion \
  -Werror=implicit-function-declaration \
  -Wunused-but-set-variable \
  -Werror=return-type \
  -Werror=incompatible-pointer-types \
  -Wshadow \
  -Werror=overflow \
  -Werror=shift-count-overflow \
  -Werror=shift-overflow=2 \
  -Warray-bounds \
  -Wrestrict \
  -Wreturn-local-addr \
  -Wstringop-overflow \
])
AC_SUBST(AM_CFLAGS)

CC_CHECK_FLAGS_APPEND([AM_LDFLAGS],[LDFLAGS],[ \
  -z relro \
  -z now \
  -fstack-protector-strong \
  --param=ssp-buffer-size=4 \
])
AC_SUBST(AM_LDLAGS)

CC_CHECK_FLAGS_APPEND([AM_LDFLAGS],[LDFLAGS],[ \
  -z relro \
  -z now \
  -fstack-protector-strong \
  --param=ssp-buffer-size=4 \
])
AC_SUBST(AM_LDLAGS)

case $host in
  *mingw*)
  # -D__USE_MINGW_ANSI_STDIO is deprecated it seems.
  CC_CHECK_FLAGS_APPEND([AM_CFLAGS],[CFLAGS],[-D_POSIX])
  ;;
esac

AC_CONFIG_FILES([raft.pc Makefile])
AC_OUTPUT
