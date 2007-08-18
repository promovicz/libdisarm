#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([disarm],[0.1],[jonlst@gmail.com])
AM_INIT_AUTOMAKE([dist-bzip2 -Wall])

AC_CONFIG_SRCDIR([src/disarm.cpp])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_HEADER_ASSERT
AC_CHECK_HEADERS([limits.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_SYS_LARGEFILE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memchr memset munmap strchr strdup strtol])

AC_CONFIG_FILES([
	Makefile
])
AC_OUTPUT


echo "
 $PACKAGE_NAME $VERSION

    prefix:		${prefix}
    compiler:		${CXX}
    cxxflags:		${CXXFLAGS}
"
