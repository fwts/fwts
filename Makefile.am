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
# along with this program; if not, see <https://www.gnu.org/licenses/>.
#

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_GETTEXT) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)

# results.log: created by `src/fwts` when executed with no `-r` option.
MOSTLYCLEANFILES = \
	results.log

GITIGNOREFILES = \
	debian/*.debhelper \
	debian/*.debhelper.log \
	debian/*.substvars \
	`cat debian/control |$(GREP) ^Package|$(AWK) '{print "debian/" $$2}'` \
	debian/autoreconf.after \
	debian/autoreconf.before \
	debian/files \
	debian/tmp \
	fedora/fwts.spec \
	fedora/rpm

SUBDIRS = src data src/utilities scripts/bash-completion

ACLOCAL_AMFLAGS = -I m4

TESTS_ENVIRONMENT = export LC_ALL=C;
TESTS_ENVIRONMENT += export FWTS='$(srcdir)/src/fwts';
TESTS_ENVIRONMENT += export FWTSTESTDIR='$(srcdir)/fwts-test';
TESTS_ENVIRONMENT += export TMP=/tmp;
TESTS_ENVIRONMENT += export FAILURE_LOG=/tmp/failure.log;

TESTS = fwts-test/acpidump-0001/test-0001.sh \
	fwts-test/acpitables-0001/test-0001.sh \
	fwts-test/aest-0001/test-0001.sh \
	fwts-test/aest-0001/test-0002.sh \
	fwts-test/apicinstance-0001/test-0001.sh \
	fwts-test/apicinstance-0001/test-0002.sh \
	fwts-test/apmt-0001/test-0001.sh \
	fwts-test/apmt-0001/test-0002.sh \
	fwts-test/arg-help-0001/test-0001.sh \
	fwts-test/arg-help-0001/test-0002.sh \
	fwts-test/arg-json-0001/test-0001.sh \
	fwts-test/arg-json-0001/test-0002.sh \
	fwts-test/arg-json-0001/test-0003.sh \
	fwts-test/arg-json-0001/test-0004.sh \
	fwts-test/arg-klog-0001/test-0001.sh \
	fwts-test/arg-klog-0001/test-0002.sh \
	fwts-test/arg-log-fields-0001/test-0001.sh \
	fwts-test/arg-log-format-0001/test-0001.sh \
	fwts-test/arg-log-format-0001/test-0002.sh \
	fwts-test/arg-log-format-0001/test-0003.sh \
	fwts-test/arg-log-format-0001/test-0004.sh \
	fwts-test/arg-quiet-0001/test-0001.sh \
	fwts-test/arg-quiet-0001/test-0002.sh \
	fwts-test/arg-results-0001/test-0001.sh \
	fwts-test/arg-results-0001/test-0002.sh \
	fwts-test/arg-results-0001/test-0003.sh \
	fwts-test/arg-results-0001/test-0004.sh \
	fwts-test/arg-results-0001/test-0005.sh \
	fwts-test/arg-results-0001/test-0006.sh \
	fwts-test/arg-results-no-separators-0001/test-0001.sh \
	fwts-test/arg-show-progress-dialog-0001/test-0001.sh \
	fwts-test/arg-show-progress-dialog-0001/test-0002.sh \
	fwts-test/arg-table-path-0001/test-0001.sh \
	fwts-test/arg-table-path-0001/test-0002.sh \
	fwts-test/arg-width-0001/test-0001.sh \
	fwts-test/arg-width-0001/test-0002.sh \
	fwts-test/arg-width-0001/test-0003.sh \
	fwts-test/arg-width-0001/test-0004.sh \
	fwts-test/arg-width-0001/test-0005.sh \
	fwts-test/arg-width-0001/test-0006.sh \
	fwts-test/arg-width-0001/test-0007.sh \
	fwts-test/arg-width-0001/test-0008.sh \
	fwts-test/asf-0001/test-0001.sh \
	fwts-test/asf-0001/test-0002.sh \
	fwts-test/aspt-0001/test-0001.sh \
	fwts-test/aspt-0001/test-0002.sh \
	fwts-test/aspt-0001/test-0003.sh \
	fwts-test/aspt-0001/test-0004.sh \
	fwts-test/bert-0001/test-0001.sh \
	fwts-test/bert-0001/test-0002.sh \
	fwts-test/bgrt-0001/test-0001.sh \
	fwts-test/bgrt-0001/test-0002.sh \
	fwts-test/boot-0001/test-0001.sh \
	fwts-test/boot-0001/test-0002.sh \
	fwts-test/ccel-0001/test-0001.sh \
        fwts-test/ccel-0001/test-0002.sh \
	fwts-test/cedt-0001/test-0001.sh \
	fwts-test/cedt-0001/test-0002.sh \
	fwts-test/checksum-0001/test-0001.sh \
	fwts-test/checksum-0001/test-0003.sh \
	fwts-test/checksum-0001/test-0004.sh \
	fwts-test/cpep-0001/test-0001.sh \
	fwts-test/cpep-0001/test-0002.sh \
	fwts-test/csrt-0001/test-0001.sh \
	fwts-test/csrt-0001/test-0002.sh \
	fwts-test/dbg2-0001/test-0001.sh \
	fwts-test/dbg2-0001/test-0002.sh \
	fwts-test/dbgp-0001/test-0001.sh \
	fwts-test/dbgp-0001/test-0002.sh \
	fwts-test/disassemble-0001/test-0001.sh \
	fwts-test/drtm-0001/test-0001.sh \
	fwts-test/drtm-0001/test-0002.sh \
	fwts-test/erst-0001/test-0001.sh \
	fwts-test/erst-0001/test-0002.sh \
	fwts-test/facs-0001/test-0001.sh \
	fwts-test/facs-0001/test-0002.sh \
	fwts-test/fpdt-0001/test-0001.sh \
	fwts-test/fpdt-0001/test-0002.sh \
	fwts-test/hest-0001/test-0001.sh \
	fwts-test/hest-0001/test-0002.sh \
	fwts-test/hmat-0001/test-0001.sh \
	fwts-test/hmat-0001/test-0002.sh \
	fwts-test/ibft-0001/test-0001.sh \
	fwts-test/ibft-0001/test-0002.sh \
	fwts-test/iort-0001/test-0001.sh \
	fwts-test/iort-0001/test-0002.sh \
	fwts-test/ivrs-0001/test-0001.sh \
	fwts-test/ivrs-0001/test-0002.sh \
	fwts-test/klog-0001/test-0001.sh \
	fwts-test/klog-0001/test-0002.sh \
	fwts-test/klog-0001/test-0003.sh \
	fwts-test/klog-0002/test-0001.sh \
	fwts-test/klog-0002/test-0002.sh \
	fwts-test/klog-0002/test-0003.sh \
	fwts-test/lpit-0001/test-0001.sh \
	fwts-test/lpit-0001/test-0002.sh \
	fwts-test/madt-0001/test-0001.sh \
	fwts-test/madt-0001/test-0002.sh \
	fwts-test/mchi-0001/test-0001.sh \
	fwts-test/mchi-0001/test-0002.sh \
        fwts-test/misc-0001/test-0001.sh \
        fwts-test/misc-0001/test-0002.sh \
	fwts-test/mpam-0001/test-0001.sh \
	fwts-test/mpam-0001/test-0002.sh \
        fwts-test/mpst-0001/test-0001.sh \
        fwts-test/mpst-0001/test-0002.sh \
        fwts-test/msct-0001/test-0001.sh \
        fwts-test/msct-0001/test-0002.sh \
	fwts-test/msdm-0001/test-0001.sh \
	fwts-test/msdm-0001/test-0002.sh \
	fwts-test/method-0001/test-0001.sh \
        fwts-test/nhlt-0001/test-0001.sh \
        fwts-test/nhlt-0001/test-0002.sh \
	fwts-test/nfit-0001/test-0001.sh \
	fwts-test/nfit-0001/test-0002.sh \
	fwts-test/oops-0001/test-0001.sh \
	fwts-test/oops-0001/test-0002.sh \
	fwts-test/oops-0001/test-0003.sh \
	fwts-test/osilinux-0001/test-0001.sh \
	fwts-test/osilinux-0001/test-0002.sh \
	fwts-test/pcct-0001/test-0001.sh \
	fwts-test/pcct-0001/test-0002.sh \
	fwts-test/pdtt-0001/test-0001.sh \
	fwts-test/pdtt-0001/test-0002.sh \
	fwts-test/phat-0001/test-0001.sh \
	fwts-test/phat-0001/test-0002.sh \
	fwts-test/phat-0001/test-0003.sh \
	fwts-test/pmtt-0001/test-0001.sh \
	fwts-test/pmtt-0001/test-0002.sh \
	fwts-test/pptt-0001/test-0001.sh \
	fwts-test/pptt-0001/test-0002.sh \
	fwts-test/ras2-0001/test-0001.sh \
	fwts-test/ras2-0001/test-0002.sh \
	fwts-test/rasf-0001/test-0001.sh \
	fwts-test/rasf-0001/test-0002.sh \
	fwts-test/rgrt-0001/test-0001.sh \
	fwts-test/rgrt-0001/test-0002.sh \
	fwts-test/rhct-0001/test-0001.sh \
	fwts-test/rhct-0001/test-0002.sh \
	fwts-test/rsdp-0001/test-0001.sh \
	fwts-test/rsdt-0001/test-0001.sh \
	fwts-test/s0idle-0001/test-0001.sh \
	fwts-test/s0idle-0001/test-0002.sh \
	fwts-test/sbst-0001/test-0001.sh \
	fwts-test/sbst-0001/test-0002.sh \
        fwts-test/sdei-0001/test-0001.sh \
	fwts-test/sdei-0001/test-0002.sh \
	fwts-test/sdev-0001/test-0001.sh \
	fwts-test/sdev-0001/test-0002.sh \
        fwts-test/skvl-0001/test-0001.sh \
        fwts-test/skvl-0001/test-0002.sh \
	fwts-test/slic-0001/test-0001.sh \
	fwts-test/slic-0001/test-0002.sh \
	fwts-test/slit-0001/test-0001.sh \
	fwts-test/slit-0001/test-0002.sh \
	fwts-test/spcr-0001/test-0001.sh \
	fwts-test/spcr-0001/test-0002.sh \
	fwts-test/srat-0001/test-0001.sh \
	fwts-test/srat-0001/test-0002.sh \
	fwts-test/stao-0001/test-0001.sh \
	fwts-test/stao-0001/test-0002.sh \
	fwts-test/svkl-0001/test-0001.sh \
	fwts-test/svkl-0001/test-0002.sh \
	fwts-test/syntaxcheck-0001/test-0001.sh \
	fwts-test/tcpa-0001/test-0001.sh \
	fwts-test/tcpa-0001/test-0002.sh \
	fwts-test/tpm2-0001/test-0001.sh \
	fwts-test/tpm2-0001/test-0002.sh \
	fwts-test/uefi-0001/test-0001.sh \
	fwts-test/uefi-0001/test-0002.sh \
        fwts-test/viot-0001/test-0001.sh \
        fwts-test/viot-0001/test-0002.sh \
	fwts-test/waet-0001/test-0001.sh \
	fwts-test/waet-0001/test-0002.sh \
	fwts-test/waet-0001/test-0003.sh \
	fwts-test/wdat-0001/test-0001.sh \
	fwts-test/wdat-0001/test-0002.sh \
	fwts-test/wmi-0001/test-0001.sh \
	fwts-test/wpbt-0001/test-0001.sh \
	fwts-test/wpbt-0001/test-0002.sh \
	fwts-test/wsmt-0001/test-0001.sh \
	fwts-test/wsmt-0001/test-0002.sh \
	fwts-test/wmi-0001/test-0002.sh \
	fwts-test/wmi-0001/test-0003.sh \
	fwts-test/xenv-0001/test-0001.sh \
	fwts-test/xenv-0001/test-0002.sh \
	fwts-test/xsdt-0001/test-0001.sh

-include $(top_srcdir)/git.mk
