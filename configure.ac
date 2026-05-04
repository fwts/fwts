	  AC_INIT([fwts],[0.1],[fwts-devel@lists.ubuntu.com])
          AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
          m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
	  AC_CANONICAL_HOST
	  AC_CONFIG_MACRO_DIR([m4])
          AC_PROG_CC
	  AC_PROG_LEX([noyywrap])
	  AC_PROG_YACC
	  LT_INIT
	  AC_C_INLINE
	  AM_PROG_CC_C_O
  	  AC_CHECK_FUNCS([localtime_r])
	  AC_CHECK_FUNCS([dup2])
	  AC_CHECK_FUNCS([getcwd])
	  AC_CHECK_FUNCS([getpagesize])
	  AC_CHECK_FUNCS([memset])
	  AC_CHECK_FUNCS([mkdir])
	  AC_CHECK_FUNCS([munmap])
	  AC_CHECK_FUNCS([select])
	  AC_CHECK_FUNCS([socket])
	  AC_CHECK_FUNCS([sqrt])
	  AC_CHECK_FUNCS([strchr])
	  AC_CHECK_FUNCS([strdup])
	  AC_CHECK_FUNCS([strerror])
	  AC_CHECK_FUNCS([strstr])
	  AC_CHECK_FUNCS([strtoul])
	  AC_CHECK_FUNCS([strtoull])
	  AC_CHECK_FUNCS([gettimeofday])
	  AC_CHECK_FUNCS([uname])
	  AC_CHECK_HEADERS([assert.h])
	  AC_CHECK_HEADERS([ctype.h])
	  AC_CHECK_HEADERS([errno.h])
	  AC_CHECK_HEADERS([fcntl.h])
	  AC_CHECK_HEADERS([getopt.h])
	  AC_CHECK_HEADERS([limits.h])
	  AC_CHECK_HEADERS([linux/input.h])
	  AC_CHECK_HEADERS([math.h])
	  AC_CHECK_HEADERS([paths.h])
	  AC_CHECK_HEADERS([pcre.h])
	  AC_CHECK_HEADERS([sched.h])
	  AC_CHECK_HEADERS([signal.h])
	  AC_CHECK_HEADERS([stdarg.h])
	  AC_CHECK_HEADERS([stdint.h])
	  AC_CHECK_HEADERS([stdio.h])
	  AC_CHECK_HEADERS([stdlib.h])
	  AC_CHECK_HEADERS([string.h])
	  AC_CHECK_HEADERS([bsd/string.h])
	  AC_CHECK_HEADERS([sys/io.h])
	  AC_CHECK_HEADERS([sys/klog.h])
	  AC_CHECK_HEADERS([sys/mman.h])
	  AC_CHECK_HEADERS([sys/param.h])
	  AC_CHECK_HEADERS([sys/select.h])
	  AC_CHECK_HEADERS([sys/socket.h])
	  AC_CHECK_HEADERS([sys/stat.h])
	  AC_CHECK_HEADERS([sys/type.h])
	  AC_CHECK_HEADERS([sys/un.h])
	  AC_CHECK_HEADERS([sys/wait.h])
	  AC_CHECK_HEADERS([termios.h])
	  AC_CHECK_HEADERS([time.h])
	  AC_CHECK_HEADERS([sys/ioctl.h])
	  AC_CHECK_HEADERS([sys/time.h])
	  AC_CHECK_HEADERS([glib.h])
	  AC_CHECK_HEADERS([gio/gio.h])
	  AC_CHECK_HEADERS([asm/opal-prd.h])
	  AC_CHECK_HEADERS([mtd/mtd-abi.h])
	  AC_CHECK_HEADERS([pci/pci.h])
	  PKG_CHECK_MODULES([GIO], [gio-2.0])
	  PKG_CHECK_MODULES([GLIB], [glib-2.0])
	  AC_SUBST([GIO_CFLAGS])
	  AC_SUBST([GIO_LIBS])
	  AC_SUBST([GLIB_CFLAGS])
	  AC_SUBST([GLIB_LIBS])
	  AM_CONDITIONAL([HAVE_ASM_OPAL_PRD_H],
	    [test "x$ac_cv_header_asm_opal_prd_h" = "xyes"])
	  AM_CONDITIONAL([HAVE_MTD_ABI_H],
	    [test "x$ac_cv_header_mtd_abi_h" = "xyes"])
	  AM_CONDITIONAL([HAVE_PCI_PCI_H],
	    [test "x$ac_cv_header_pci_pci_h" = "xyes"])
	  #AC_CHECK_LIB(pcre, pcre_compile)
	  AC_SEARCH_LIBS([fdt_check_header], [fdt], [
	    AC_DEFINE([HAVE_LIBFDT], [1], [Define if we have libfdt])])
	  AM_CONDITIONAL([HAVE_LIBFDT],
	    [test "x$ac_cv_search_fdt_check_header" != "xno"])
	  AC_SEARCH_LIBS([pci_alloc], [pci], [
	    AC_DEFINE([HAVE_LIBPCI], [1], [Define if we have libpci])])
	  AM_CONDITIONAL([HAVE_LIBPCI],
	    [test "x$ac_cv_search_pci_alloc" != "xno"])
	  AC_SEARCH_LIBS([strlcpy], [bsd],, AC_MSG_ERROR([strlcpy is not available]))
	  AM_CONDITIONAL([HAVE_LIBBSD],
	    [test "x$ac_cv_search_strlcpy" = "x-lbsd"])
	  AC_FUNC_MALLOC
	  AC_FUNC_FORK
	  AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
	  AC_FUNC_MMAP
	  AC_FUNC_REALLOC
	  AC_TYPE_PID_T
	  AC_TYPE_SIZE_T
	  AC_TYPE_OFF_T
	  AC_TYPE_UINT16_T
	  AC_TYPE_UINT32_T
	  AC_TYPE_UINT64_T
	  AC_TYPE_UINT8_T
	  case $host in
	  arm*) CFLAGS="$CFLAGS -marm" ;;
	  esac
          AC_CONFIG_HEADERS([config.h])
	  AC_CONFIG_FILES([
           Makefile
           src/Makefile
	   src/libfwtsiasl/Makefile
	   src/libfwtsacpica/Makefile
           src/lib/Makefile
           src/lib/src/Makefile
	   src/utilities/Makefile
           data/Makefile
           scripts/bash-completion/Makefile
          ])
	  AC_ARG_WITH([bashcompletiondir],
		      AS_HELP_STRING([--with-bashcompletiondir=DIR], [Bash completions directory]),
		      [],
		      [AS_IF([`pkg-config --exists bash-completion`], [
		       with_bashcompletiondir=`pkg-config --variable=completionsdir bash-completion`
		       ], [
			   with_bashcompletiondir=${datadir}/bash-completion/completions
			   ])
		      ])
	  AC_SUBST([bashcompletiondir], [$with_bashcompletiondir])
          AC_OUTPUT
