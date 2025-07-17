# Must build in this order:
#   1. acpica/source/compiler
#   2. lib
#   3. acpica
# ... because the libs in this bundled acpica/ depend on lib/ (libfwts.so),
# but libfwts.so depends on libraries produced by acpica/source/compiler.
SUBDIRS = libfwtsiasl lib libfwtsacpica

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib/include			\
	-I$(top_srcdir)/src/acpica/source/include	\
	-I$(top_srcdir)/src/acpica/source/compiler	\
	-I$(top_srcdir)/efi_runtime			\
	-I$(top_srcdir)/smccc_test			\
	-pthread					\
	-Wall -Werror -Wextra				\
	-Wno-address-of-packed-member			\
	-Wfloat-equal -Wmissing-declarations		\
	-Wno-long-long -Wredundant-decls -Wshadow	\
	-Wno-missing-field-initializers 		\
	-Wno-missing-braces -Wno-sign-compare 		\
	-Wno-multichar -Wcast-qual

bin_PROGRAMS = fwts

fwts_CPPFLAGS = 					\
	$(AM_CPPFLAGS)					\
	@GIO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	-DACPI_DEBUG_OUTPUT

if HAVE_LIBFDT
dt_tests = \
	devicetree/dt_base/dt_base.c		\
	devicetree/dt_sysinfo/dt_sysinfo.c
mem_tests = \
	opal/mem_info.c				\
	opal/reserv_mem.c
cpu_tests = \
	opal/cpu_info.c
power_mgmt_tests = \
	opal/power_mgmt_info.c
endif

if HAVE_LIBFDT
if HAVE_LIBPCI
pci_tests = \
	opal/pci_info.c
endif
endif

#
#  fwts main + tests
#
fwts_SOURCES = main.c 				\
	acpi/ac_adapter/ac_adapter.c 		\
	acpi/acpidump/acpidump.c 		\
	acpi/acpiinfo/acpiinfo.c 		\
        acpi/acpipld/acpipld.c                  \
        acpi/acpitables/acpitables.c 		\
	sbbr/acpitables/acpitables.c 		\
	acpi/aest/aest.c				\
	acpi/apicinstance/apicinstance.c 	\
	acpi/apmt/apmt.c		 	\
	acpi/asf/asf.c				\
	acpi/aspt/aspt.c			\
	acpi/battery/battery.c 			\
	acpi/bert/bert.c			\
	acpi/bgrt/bgrt.c			\
	acpi/boot/boot.c			\
	acpi/brightness/brightness-helper.c	\
	acpi/brightness/brightness.c 		\
	acpi/brightness/autobrightness.c 	\
        acpi/ccel/ccel.c                        \
	acpi/cedt/cedt.c			\
	acpi/checksum/checksum.c 		\
	acpi/cpep/cpep.c			\
	acpi/crsdump/crsdump.c			\
	acpi/crsdump/prsdump.c			\
	acpi/csrt/csrt.c			\
	cpu/cpuidle/cpuidle.c 			\
	acpi/dbgp/dbgp.c 			\
	acpi/dbg2/dbg2.c 			\
	acpi/devices/ac_adapter/ac.c		\
	acpi/devices/battery/battery.c		\
	acpi/devices/battery/smart_battery.c	\
	acpi/devices/ec/ec.c			\
	acpi/devices/buttons/power_button.c	\
	acpi/devices/buttons/sleep_button.c	\
	acpi/devices/lid/lid.c			\
	acpi/devices/nvdimm/nvdimm.c 			\
	acpi/devices/sensor/als.c		\
	acpi/devices/time/time.c		\
	acpi/devices/wpc/wpc.c			\
	acpi/dmar/dmar.c 			\
	acpi/dppt/dppt.c 			\
	acpi/drtm/drtm.c 			\
	acpi/dsddump/dsddump.c			\
	acpi/ecdt/ecdt.c			\
	acpi/einj/einj.c			\
	acpi/erst/erst.c			\
	acpi/facs/facs.c 			\
	acpi/fadt/fadt.c 			\
	sbbr/fadt/fadt.c 			\
	acpi/fan/fan.c 				\
	acpi/fpdt/fpdt.c 			\
	acpi/gpedump/gpedump.c			\
	acpi/gtdt/gtdt.c			\
	acpi/hest/hest.c			\
	acpi/hpet/hpet.c 			\
	acpi/hmat/hmat.c 			\
	acpi/iort/iort.c			\
	acpi/ibft/ibft.c			\
	acpi/ivrs/ivrs.c			\
	acpi/lid/lid.c 				\
	acpi/lpit/lpit.c 			\
	acpi/madt/madt.c			\
	acpi/mcfg/mcfg.c 			\
	acpi/mchi/mchi.c 			\
        acpi/misc/misc.c                        \
	acpi/mpam/mpam.c 			\
	acpi/mpst/mpst.c 			\
	acpi/msct/msct.c 			\
	acpi/msdm/msdm.c 			\
	acpi/method/method.c 			\
	acpi/nfit/nfit.c 			\
	acpi/nhltdump/nhltdump.c 		\
	acpi/osilinux/osilinux.c 		\
	acpi/pcc/pcc.c 				\
	acpi/pcct/pcct.c			\
	acpi/pdtt/pdtt.c			\
	acpi/powerbutton/powerbutton.c 		\
	acpi/plddump/plddump.c			\
	acpi/phat/phat.c 			\
	acpi/pmtt/pmtt.c 			\
	acpi/pptt/pptt.c 			\
	acpi/rasf/rasf.c			\
	acpi/ras2/ras2.c			\
	acpi/rgrt/rgrt.c			\
	acpi/rsdp/rsdp.c			\
	sbbr/rsdp/rsdp.c			\
	acpi/rsdt/rsdt.c			\
	acpi/s3/s3.c 				\
        acpi/s3pt/s3pt.c                        \
	acpi/s3power/s3power.c 			\
	acpi/s4/s4.c 				\
	acpi/sbst/sbst.c			\
	acpi/sdei/sdei.c			\
	acpi/sdev/sdev.c			\
        acpi/skvl/skvl.c                        \
        acpi/slic/slic.c                        \
	acpi/slit/slit.c 			\
	acpi/spcr/spcr.c 			\
	acpi/spmi/spmi.c 			\
	acpi/srat/srat.c 			\
	acpi/stao/stao.c			\
	acpi/svkl/svkl.c			\
	acpi/syntaxcheck/syntaxcheck.c 		\
	acpi/tcpa/tcpa.c 			\
	acpi/tpm2/tpm2.c 			\
	acpi/uefi/uefi.c			\
	acpi/uniqueid/uniqueid.c		\
	acpi/viot/viot.c			\
	acpi/waet/waet.c			\
	acpi/wakealarm/wakealarm.c 		\
	acpi/wdat/wdat.c			\
	acpi/wmi/wmi.c 				\
	acpi/wpbt/wpbt.c			\
	acpi/wsmt/wsmt.c			\
	acpi/xsdt/xsdt.c			\
	acpi/xenv/xenv.c			\
	apic/apicedge/apicedge.c 		\
	bios/bios_info/bios_info.c 		\
	bios/bios32/bios32.c 			\
	bios/ebda_region/ebda_region.c 		\
	bios/ebdadump/ebdadump.c 		\
	bios/hdaaudio/hdaaudio.c 		\
	bios/interrupt/interrupt.c		\
	bios/memmapdump/memmapdump.c 		\
	bios/mtrr/mtrr.c 			\
	bios/multiproc/mpcheck.c 		\
	bios/multiproc/mpdump.c 		\
	bios/pciirq/pciirq.c 			\
	bios/pnp/pnp.c 				\
	bios/romdump/romdump.c 			\
	bios/s0idle/s0idle.c 			\
	bios/smm/smm.c				\
	cmos/cmosdump/cmosdump.c 		\
	coreboot/clog/clog.c			\
	cpu/virt/virt.c 			\
	cpu/virt/virt_svm.c 			\
	cpu/virt/virt_vmx.c			\
	cpu/maxfreq/maxfreq.c 			\
	cpu/cpufreq/cpufreq.c 			\
	cpu/nx/nx.c 				\
	cpu/msr/msr.c 				\
	cpu/microcode/microcode.c 		\
	dmi/dmicheck/dmicheck.c 		\
	hotkey/hotkey/hotkey.c 			\
	ipmi/bmc/bmc_info.c			\
	kernel/klog/klog.c 			\
	kernel/olog/olog.c			\
	kernel/oops/oops.c 			\
	kernel/version/version.c 		\
	opal/mtd_info.c				\
	opal/prd_info.c				\
	pci/aspm/aspm.c 			\
	pci/crs/crs.c 				\
	pci/maxreadreq/maxreadreq.c 		\
	pci/smccc/smccc.c			\
	tpm/tpmevlog/tpmevlog.c			\
	tpm/tpmevlogdump/tpmevlogdump.c		\
	uefi/csm/csm.c 				\
	uefi/uefidump/uefidump.c 		\
	uefi/uefirttime/uefirttime.c		\
	uefi/uefirtvariable/uefirtvariable.c	\
	uefi/uefirtmisc/uefirtmisc.c		\
	uefi/securebootcert/securebootcert.c	\
	uefi/uefivarinfo/uefivarinfo.c		\
	uefi/uefibootpath/uefibootpath.c	\
	uefi/uefirtauthvar/uefirtauthvar.c	\
	uefi/esrtdump/esrtdump.c		\
	uefi/esrt/esrt.c			\
	$(pci_tests)				\
	$(mem_tests)				\
	$(cpu_tests)				\
	$(power_mgmt_tests)			\
	$(dt_tests)

fwts_LDFLAGS = -no-undefined

fwts_LDADD = \
	-lbsd \
	$(top_builddir)/src/lib/src/libfwts.la \
	$(top_builddir)/src/libfwtsiasl/libfwtsiasl.la \
	$(top_builddir)/src/libfwtsacpica/libfwtsacpica.la

man_MANS = ../doc/fwts.1 ../doc/fwts-collect.1 ../doc/fwts-frontend-text.1

-include $(top_srcdir)/git.mk
