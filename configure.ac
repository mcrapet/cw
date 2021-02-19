AC_INIT(cw, 0.1.1, [mcrapet@gmail.com], [cw], [https://github.com/mcrapet/cw])
AC_PREREQ(2.64)

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects
	tar-pax no-dist-gzip dist-bzip2])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Checks for programs
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S

dnl More warnings
CFLAGS="-Wall -Wextra $CFLAGS"

dnl Checks for header files
AC_HEADER_STDC
AC_C_INLINE
AC_FUNC_FORK
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

dnl Checks for functions
AC_CHECK_HEADERS([sys/select.h poll.h sys/epoll.h])
AC_CHECK_FUNCS([pselect ppoll epoll_ctl dup2 strerror])


AC_ARG_WITH([iowait],
    [AS_HELP_STRING(
        [--with-iowait=<method>],
        [choose I/O wait method (select, ppoll or epoll) @<:@default=autodetect@:>@])],
    [
        if test "x$withval" = "xselect"; then
            if test "${ac_cv_func_pselect}" != "yes"; then
                AC_MSG_ERROR([--with-iowait=${withval} is not available on your system])
            fi
            AC_DEFINE(FORCE_IOWAIT, 0x53454C45)
        elif test "x$withval" = "xppoll"; then
            if test "${ac_cv_func_ppoll}" != "yes"; then
                AC_MSG_ERROR([--with-iowait=${withval} is not available on your system])
            fi
            AC_DEFINE(FORCE_IOWAIT, 0x504F4C4C)
        elif test "x$withval" = "xepoll"; then
            if test "${ac_cv_func_epoll}" != "yes"; then
                AC_MSG_ERROR([--with-iowait=${withval} is not available on your system])
            fi
            AC_DEFINE(FORCE_IOWAIT, 0x45504F4C)
        else
            AC_MSG_ERROR([--with-iowait invalid argument, available values: select, ppoll, epoll (default)])
        fi
    ],
    [])

AH_TEMPLATE([FORCE_IOWAIT], [Define I/O multiplexing method])

dnl Output the makefile
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT
