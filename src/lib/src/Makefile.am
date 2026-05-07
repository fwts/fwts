#
# Copyright (C) 2010-2026 Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib/include 		\
	-I$(top_srcdir)/src/libfwtsiasl			\
	-I$(top_srcdir)/src/acpica/source/include	\
	-I$(top_srcdir)/src/acpica/source/compiler	\
	-I$(top_srcdir)/smccc_test                      \
	-DDATAROOTDIR=\"$(datarootdir)\"		\
	-Wall -Werror -Wextra				\
	-Wno-address-of-packed-member

pkglib_LTLIBRARIES = libfwts.la

libfwts_la_LDFLAGS = -version-info 1:0:0

if HAVE_LIBBSD
libfwts_LDADD_BSD = -lbsd
endif

libfwts_la_LIBADD = 				\
	-lm -lpthread					\
	$(libfwts_LDADD_BSD)				\
	@GIO_LIBS@					\
	@GLIB_LIBS@

libfwts_la_CPPFLAGS =					\
	$(AM_CPPFLAGS)					\
	@GIO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	-DACPI_DEBUG_OUTPUT

if HAVE_LIBFDT
dt_sources = \
	fwts_devicetree.c
endif

#
#  Components of the fwts core helper library libfwts
#
libfwts_la_SOURCES = 		\
	fwts_ac_adapter.c 	\
	fwts_acpi_object_eval.c \
	fwts_acpi_tables.c 	\
	fwts_acpi.c 		\
	fwts_acpid.c 		\
	fwts_alloc.c 		\
	fwts_arch.c 		\
	fwts_args.c 		\
	fwts_backtrace.c	\
	fwts_battery.c 		\
	fwts_binpaths.c 	\
	fwts_button.c 		\
	fwts_checkeuid.c 	\
	fwts_checksum.c 	\
	fwts_clog.c		\
	fwts_cmos.c 		\
	fwts_coreboot.c		\
	fwts_coreboot_cbmem.c	\
	fwts_cpu.c 		\
	fwts_dump.c 		\
	fwts_dump_data.c 	\
	fwts_ebda.c 		\
	fwts_efi_module.c	\
	fwts_fileio.c 		\
	fwts_firmware.c 	\
	fwts_formatting.c 	\
	fwts_framework.c 	\
	fwts_get.c 		\
	fwts_gpe.c 		\
	fwts_guid.c 		\
	fwts_hwinfo.c 		\
	fwts_iasl.c 		\
	fwts_interactive.c 	\
	fwts_ioport.c		\
	fwts_ipmi.c		\
	fwts_json.c		\
	fwts_kernel.c 		\
	fwts_keymap.c 		\
	fwts_klog.c 		\
	fwts_olog.c		\
	fwts_list.c 		\
	fwts_log.c 		\
	fwts_log_html.c 	\
	fwts_log_json.c 	\
	fwts_log_plaintext.c 	\
	fwts_log_scan.c		\
	fwts_log_xml.c 		\
	fwts_memorymap.c 	\
	fwts_mmap.c 		\
	fwts_modprobe.c		\
	fwts_multiproc.c 	\
	fwts_oops.c 		\
	fwts_pci.c		\
	fwts_pipeio.c 		\
	fwts_release.c		\
	fwts_scan_efi_systab.c 	\
	fwts_set.c 		\
	fwts_smbios.c 		\
	fwts_smccc.c 		\
	fwts_stringextras.c 	\
	fwts_summary.c 		\
	fwts_text_list.c 	\
	fwts_tpm.c		\
	fwts_tty.c 		\
	fwts_uefi.c 		\
	fwts_wakealarm.c 	\
	fwts_pm_method.c	\
	fwts_safe_mem.c		\
	fwts_pm_debug.c		\
	$(dt_sources)

-include $(top_srcdir)/git.mk
