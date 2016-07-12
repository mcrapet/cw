AC_INIT(cw, 0.1.0, [mcrapet@gmail.com])
AC_PREREQ(2.59)

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects
	tar-pax no-dist-gzip dist-bzip2])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Checks for programs.
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S

dnl More warnings
CFLAGS="-Wall -Wextra $CFLAGS"

dnl Checks for header files.
AC_HEADER_STDC
AC_C_INLINE
AC_FUNC_FORK
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

dnl Checks for functions
AC_CHECK_FUNCS([pselect dup2 strerror])

dnl Output the makefile
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
