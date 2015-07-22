#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([ndd], [0.1], [jansen@atlas.cz])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([ndd.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([iniparser], [iniparser_getint], ,\
    [AC_MSG_ERROR([Could not find initparser library])])

CF_LIB_SOCKET
CF_LIB_NSL

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stdlib.h string.h \
    sys/socket.h syslog.h unistd.h])
AC_CHECK_HEADERS([iniparser.h dictionary.h], ,\
    [AC_MSG_ERROR([Could not find headers for iniParser]) ])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([atexit memset socket strerror])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
