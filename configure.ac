AC_INIT(cw, 0.1.0, [mcrapet@gmail.com], [cw], [https://github.com/mcrapet/cw])
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
AC_CHECK_HEADERS([sys/select.h poll.h])
AC_CHECK_FUNCS([pselect ppoll dup2 strerror])

AC_ARG_WITH([iowait],
    [AS_HELP_STRING(
        [--with-iowait=<method>],
        [choose I/O wait method (select or poll) @<:@default=autodetect@:>@])],
    [
        if test "x$withval" = "xselect"; then
            if test "${ac_cv_func_pselect}" != "yes"; then
                AC_MSG_ERROR([--with-iowait=${withval} is not available on your system])
            fi
            AC_DEFINE(FORCE_IOWAIT, 0x53454C45,[Use select I/O multiplexing])
        elif test "x$withval" = "xpoll"; then
            if test "${ac_cv_func_ppoll}" != "yes"; then
                AC_MSG_ERROR([--with-iowait=${withval} is not available on your system])
            fi
            AC_DEFINE(FORCE_IOWAIT, 0x504F4C4C,[Use poll I/O multiplexing])
        else
            AC_MSG_ERROR([--with-iowait invalid argument, available values: select, poll (default)])
        fi
    ],
    [])

dnl Output the makefile
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT
