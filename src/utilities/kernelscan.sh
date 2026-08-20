#!/bin/bash
#
# Copyright (C) 2010-2026 Canonical
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
# along with this program; if not, see <https://www.gnu.org/licenses/>.
#

CONFIGS="-DCONFIG_ACPI_HOTPLUG_CPU -DCONFIG_ACPI_PROC_EVENT \
	 -DCONFIG_ACPI_PROCFS_POWER -DCONFIG_ACPI_SLEEP \
	 -DCONFIG_CPU_FREQ -DCONFIG_DMI -DCONFIG_HIBERNATION \
	 -DCONFIG_HOTPLUG_CPU -DCONFIG_KEXEC -DCONFIG_NET \
	 -DCONFIG_PM -DCONFIG_PM_SLEEP -DCONFIG_SMP \
	 -DCONFIG_SUSPEND -DCONFIG_X86 -DCONFIG_X86_IO_APIC"

KERNELSCAN=./kernelscan
TMP=/tmp/kernelscan_$$.txt

if [ $# -lt 1 ]; then
	echo "Usage: $0 path-to-kernel-source"
	exit 1
fi

KLOG=/usr/share/fwts/klog.json

while [[ $# -gt 0 ]]
do

case "$1" in
	-k)
		shift
		KLOG=$1
		shift
		;;
	*)
		src=$1
		shift
		;;
esac
done

if [ "$src" == "" ]; then
	echo "Please provide path to linux source"
	exit 1
fi

if [ ! -d "$src" ]; then
	echo "Path '$src' not found"
	exit 1
fi
if [ ! -e "${KLOG}" ]; then
	echo "Path '${KLOG}' not found"
	exit 1
fi

scan_source_file()
{
	if [ -f $1 ]; then
		$KERNELSCAN -k ${KLOG} < $1 > $TMP
		if [ $(stat -c%s $TMP) -gt 0 ]; then
			echo "Source: $1"
			cat $TMP
		fi
		rm $TMP	
	else
		echo "Source: $1 does not exist"
	fi
}

scan_source_tree()
{
	tree=$1

	echo "Scanning $tree"
	for I in $(find $tree -name "*.c")
	do
		scan_source_file $I
	done
}

scan_source_tree $src/drivers/acpi
scan_source_tree $src/arch/x86/kernel/acpi
scan_source_tree $src/arch/x86/platform/efi
scan_source_tree $src/drivers/pnp/pnpbios  # need to do, loads of errors


#
# Individual files
#
scan_source_file $src/arch/x86/pci/pcbios.c
#scan_source_file $src/drivers/cpufreq/powernow-k8.c
scan_source_file $src/arch/x86/kernel/cpu/mtrr/generic.c
scan_source_file $src/arch/ia64/kernel/acpi.c
scan_source_file $src/arch/x86/boot/video-bios.c
scan_source_file $src/arch/x86/boot/video-mode.c
scan_source_file $src/arch/x86/boot/video-vesa.c
scan_source_file $src/arch/x86/boot/video-vga.c
scan_source_file $src/arch/x86/boot/video.c
scan_source_file $src/arch/x86/kernel/apm_32.c
scan_source_file $src/arch/x86/kernel/cpu/amd.c
scan_source_file $src/arch/x86/kernel/cpu/common.c
scan_source_file $src/arch/x86/kernel/e820.c
scan_source_file $src/arch/x86/kernel/hpet.c
scan_source_file $src/arch/x86/kernel/setup.c
scan_source_file $src/arch/x86/kernel/smpboot.c
scan_source_file $src/kernel/time/timekeeping.c
scan_source_file $src/drivers/watchdog/iTCO_vendor_support.c
scan_source_file $src/drivers/watchdog/hpwdt.c
scan_source_file $src/drivers/usb/host/pci-quirks.c
scan_source_file $src/drivers/iommu/amd/init.c


#
# Various Platform drivers
#    some of these are commented out because they
#    break the CPP phase, this needs fixing
# 
scan_source_file $src/drivers/platform/x86/acerhdf.c
scan_source_file $src/drivers/platform/x86/acer-wmi.c
scan_source_file $src/drivers/platform/x86/asus-laptop.c
scan_source_file $src/drivers/platform/x86/asus-nb-wmi.c
scan_source_file $src/drivers/platform/x86/asus-wmi.c
scan_source_file $src/drivers/platform/x86/classmate-laptop.c
scan_source_file $src/drivers/platform/x86/compal-laptop.c
scan_source_file $src/drivers/platform/x86/dell/dell-laptop.c
scan_source_file $src/drivers/platform/x86/dell/dell-wmi-aio.c
scan_source_file $src/drivers/platform/x86/dell/dell-wmi-base.c
scan_source_file $src/drivers/platform/x86/eeepc-laptop.c
scan_source_file $src/drivers/platform/x86/eeepc-wmi.c
scan_source_file $src/drivers/platform/x86/fujitsu-laptop.c
scan_source_file $src/drivers/platform/x86/fujitsu-tablet.c
scan_source_file $src/drivers/platform/x86/hp/hp-wmi.c
scan_source_file $src/drivers/platform/x86/ideapad-laptop.c
scan_source_file $src/drivers/platform/x86/msi-laptop.c
scan_source_file $src/drivers/platform/x86/msi-wmi.c
scan_source_file $src/drivers/platform/x86/mxm-wmi.c
scan_source_file $src/drivers/platform/x86/panasonic-laptop.c
scan_source_file $src/drivers/platform/x86/samsung-laptop.c
scan_source_file $src/drivers/platform/x86/samsung-q10.c
#scan_source_file $src/drivers/platform/x86/sony-laptop.c
scan_source_file $src/drivers/platform/x86/hp/tc1100-wmi.c
#scan_source_file $src/drivers/platform/x86/thinkpad_acpi.c
scan_source_file $src/drivers/platform/x86/topstar-laptop.c
scan_source_file $src/drivers/platform/x86/toshiba_acpi.c
scan_source_file $src/drivers/platform/x86/toshiba_bluetooth.c
scan_source_file $src/drivers/platform/x86/wmi.c
scan_source_file $src/drivers/platform/x86/xo15-ebook.c
