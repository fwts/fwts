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

# Nb. the top-level Makefile builds source/compiler for us now...

#
# -DACPI_EXEC_APP is specific to making ACPICA for the fwts method test
#
AM_CPPFLAGS = 						\
	-D_LINUX -DNDEBUG -D_CONSOLE -DACPI_EXEC_APP	\
	-I$(top_srcdir)/src/lib/include			\
	-I$(top_srcdir)/src/acpica/source/include		\
	-I$(top_srcdir)/src/acpica/source/tools/acpiexec	\
	-Wall -fno-strict-aliasing			\
	-Wno-address-of-packed-member

#
#  We rename some of the functions so we can override them in fwts. This
#  is a hack, I feel dirty now.
#
osunixxf_munged.c: ../../src/acpica/source/os_specific/service_layers/osunixxf.c
	cat  $^ |							\
	sed 's/^AcpiOsPrintf/__AcpiOsPrintf/' |				\
	sed 's/^AcpiOsReadPort/__AcpiOsReadPort/' |			\
	sed 's/^AcpiOsReadPciConfiguration/__AcpiOsReadPciConfiguration/' | \
	sed 's/^AcpiOsSignalSemaphore/__AcpiOsSignalSemaphore/' |	\
	sed 's/^AcpiOsWaitSemaphore/__AcpiOsWaitSemaphore/' |		\
	sed 's/^AcpiOsCreateSemaphore/__AcpiOsCreateSemaphore/' |	\
	sed 's/^AcpiOsDeleteSemaphore/__AcpiOsDeleteSemaphore/' |	\
	sed 's/^AcpiOsVprintf/__AcpiOsVprintf/' |			\
	sed 's/^AcpiOsSignal/__AcpiOsSignal/' |				\
	sed 's/^AcpiOsSleep/__AcpiOsSleep/' |				\
	sed 's/^AcpiOsExecute/__AcpiOsExecute/'				\
	> $@
#
#  Force maximum loop iterations to be just 128 instead of 0xffff
#  because we really don't want to wait until the sun turns into
#  a lump of coal before we complete a blocked operation
#
dscontrol_munged.c: ../../src/acpica/source/components/dispatcher/dscontrol.c
	cat $^ |					\
	sed 's/ACPI_MAX_LOOP_ITERATIONS/0x0080/'	\
	> $@

BUILT_SOURCES = osunixxf_munged.c dscontrol_munged.c

#
#  Source files that are generated on-the fly and need cleaning
#
CLEANFILES = osunixxf_munged.c					\
	dscontrol_munged.c					\
	../src/acpica/source/compiler/aslcompiler.output	\
	../src/acpica/source/compiler/dtparser.output		\
	../src/acpica/source/compiler/dtparser.y.h		\
	../src/acpica/source/compiler/prparser.output		\
	../src/acpica/source/compiler/prparser.y.h		\
	../src/acpica/source/compiler/prparserlex.c		\
	../src/acpica/source/compiler/prparserparse.c

pkglib_LTLIBRARIES = libfwtsacpica.la

libfwtsacpica_la_LDFLAGS = -version-info 1:0:0
libfwtsacpica_la_LIBADD = -lpthread
libfwtsacpica_la_CPPFLAGS = $(AM_CPPFLAGS)

#
#  fwts acpica library sources
#
libfwtsacpica_la_SOURCES =						\
	fwts_acpica.c							\
	osunixxf_munged.c						\
	dscontrol_munged.c						\
	../../src/acpica/source/components/debugger/dbcmds.c		\
	../../src/acpica/source/components/debugger/dbdisply.c		\
	../../src/acpica/source/components/debugger/dbexec.c		\
	../../src/acpica/source/components/debugger/dbfileio.c		\
	../../src/acpica/source/components/debugger/dbhistry.c		\
	../../src/acpica/source/components/debugger/dbinput.c		\
	../../src/acpica/source/components/debugger/dbstats.c		\
	../../src/acpica/source/components/debugger/dbobject.c		\
	../../src/acpica/source/components/debugger/dbutils.c		\
	../../src/acpica/source/components/debugger/dbxface.c		\
	../../src/acpica/source/components/debugger/dbmethod.c		\
	../../src/acpica/source/components/debugger/dbnames.c		\
	../../src/acpica/source/components/debugger/dbconvert.c		\
	../../src/acpica/source/components/debugger/dbtest.c		\
	../../src/acpica/source/components/disassembler/dmbuffer.c	\
	../../src/acpica/source/components/disassembler/dmcstyle.c	\
	../../src/acpica/source/components/disassembler/dmnames.c	\
	../../src/acpica/source/components/disassembler/dmopcode.c	\
	../../src/acpica/source/components/disassembler/dmresrc.c	\
	../../src/acpica/source/components/disassembler/dmresrcl.c	\
	../../src/acpica/source/components/disassembler/dmresrcs.c	\
	../../src/acpica/source/components/disassembler/dmutils.c	\
	../../src/acpica/source/components/disassembler/dmwalk.c	\
	../../src/acpica/source/components/disassembler/dmresrcl2.c	\
	../../src/acpica/source/components/disassembler/dmdeferred.c	\
	../../src/acpica/source/components/dispatcher/dsdebug.c		\
	../../src/acpica/source/components/dispatcher/dsfield.c		\
	../../src/acpica/source/components/dispatcher/dsinit.c		\
	../../src/acpica/source/components/dispatcher/dsmethod.c	\
	../../src/acpica/source/components/dispatcher/dsmthdat.c	\
	../../src/acpica/source/components/dispatcher/dsobject.c	\
	../../src/acpica/source/components/dispatcher/dspkginit.c	\
	../../src/acpica/source/components/dispatcher/dsutils.c		\
	../../src/acpica/source/components/dispatcher/dswexec.c		\
	../../src/acpica/source/components/dispatcher/dswload.c		\
	../../src/acpica/source/components/dispatcher/dswscope.c	\
	../../src/acpica/source/components/dispatcher/dswstate.c	\
	../../src/acpica/source/components/dispatcher/dsargs.c		\
	../../src/acpica/source/components/dispatcher/dswload2.c	\
	../../src/acpica/source/components/dispatcher/dsopcode.c	\
	../../src/acpica/source/components/events/evevent.c		\
	../../src/acpica/source/components/events/evgpe.c		\
	../../src/acpica/source/components/events/evgpeblk.c		\
	../../src/acpica/source/components/events/evgpeinit.c		\
	../../src/acpica/source/components/events/evgpeutil.c		\
	../../src/acpica/source/components/events/evxfgpe.c		\
	../../src/acpica/source/components/events/evmisc.c		\
	../../src/acpica/source/components/events/evregion.c		\
	../../src/acpica/source/components/events/evrgnini.c		\
	../../src/acpica/source/components/events/evsci.c		\
	../../src/acpica/source/components/events/evxface.c		\
	../../src/acpica/source/components/events/evxfevnt.c		\
	../../src/acpica/source/components/events/evxfregn.c		\
	../../src/acpica/source/components/events/evglock.c		\
	../../src/acpica/source/components/events/evhandler.c		\
	../../src/acpica/source/components/executer/exconcat.c		\
	../../src/acpica/source/components/executer/exfield.c		\
	../../src/acpica/source/components/executer/exfldio.c		\
	../../src/acpica/source/components/executer/exmisc.c		\
	../../src/acpica/source/components/executer/exmutex.c		\
	../../src/acpica/source/components/executer/exnames.c		\
	../../src/acpica/source/components/executer/exoparg1.c		\
	../../src/acpica/source/components/executer/exoparg2.c		\
	../../src/acpica/source/components/executer/exoparg3.c		\
	../../src/acpica/source/components/executer/exoparg6.c		\
	../../src/acpica/source/components/executer/exprep.c		\
	../../src/acpica/source/components/executer/exregion.c		\
	../../src/acpica/source/components/executer/exresnte.c		\
	../../src/acpica/source/components/executer/exresolv.c		\
	../../src/acpica/source/components/executer/exresop.c		\
	../../src/acpica/source/components/executer/exserial.c		\
	../../src/acpica/source/components/executer/exstore.c		\
	../../src/acpica/source/components/executer/exstoren.c		\
	../../src/acpica/source/components/executer/exstorob.c		\
	../../src/acpica/source/components/executer/exsystem.c		\
	../../src/acpica/source/components/executer/extrace.c		\
	../../src/acpica/source/components/executer/exutils.c		\
	../../src/acpica/source/components/executer/exconvrt.c		\
	../../src/acpica/source/components/executer/excreate.c		\
	../../src/acpica/source/components/executer/exdump.c		\
	../../src/acpica/source/components/executer/exdebug.c		\
	../../src/acpica/source/components/executer/exconfig.c		\
	../../src/acpica/source/components/hardware/hwacpi.c		\
	../../src/acpica/source/components/hardware/hwgpe.c		\
	../../src/acpica/source/components/hardware/hwpci.c		\
	../../src/acpica/source/components/hardware/hwregs.c		\
	../../src/acpica/source/components/hardware/hwsleep.c		\
	../../src/acpica/source/components/hardware/hwvalid.c		\
	../../src/acpica/source/components/hardware/hwxface.c		\
	../../src/acpica/source/components/hardware/hwxfsleep.c		\
	../../src/acpica/source/components/hardware/hwesleep.c		\
	../../src/acpica/source/components/namespace/nsaccess.c		\
	../../src/acpica/source/components/namespace/nsalloc.c		\
	../../src/acpica/source/components/namespace/nsdump.c		\
	../../src/acpica/source/components/namespace/nsdumpdv.c		\
	../../src/acpica/source/components/namespace/nseval.c		\
	../../src/acpica/source/components/namespace/nsinit.c		\
	../../src/acpica/source/components/namespace/nsload.c		\
	../../src/acpica/source/components/namespace/nsnames.c		\
	../../src/acpica/source/components/namespace/nsobject.c		\
	../../src/acpica/source/components/namespace/nsparse.c		\
	../../src/acpica/source/components/namespace/nspredef.c		\
	../../src/acpica/source/components/namespace/nsrepair.c		\
	../../src/acpica/source/components/namespace/nsrepair2.c	\
	../../src/acpica/source/components/namespace/nssearch.c		\
	../../src/acpica/source/components/namespace/nsutils.c		\
	../../src/acpica/source/components/namespace/nswalk.c		\
	../../src/acpica/source/components/namespace/nsxfeval.c		\
	../../src/acpica/source/components/namespace/nsxfname.c		\
	../../src/acpica/source/components/namespace/nsxfobj.c		\
	../../src/acpica/source/components/namespace/nsconvert.c	\
	../../src/acpica/source/components/namespace/nsprepkg.c		\
	../../src/acpica/source/components/namespace/nsarguments.c	\
	../../src/acpica/source/components/parser/psargs.c		\
	../../src/acpica/source/components/parser/psloop.c		\
	../../src/acpica/source/components/parser/psopcode.c		\
	../../src/acpica/source/components/parser/psparse.c		\
	../../src/acpica/source/components/parser/psscope.c		\
	../../src/acpica/source/components/parser/pstree.c		\
	../../src/acpica/source/components/parser/psutils.c		\
	../../src/acpica/source/components/parser/pswalk.c		\
	../../src/acpica/source/components/parser/psxface.c		\
	../../src/acpica/source/components/parser/psopinfo.c		\
	../../src/acpica/source/components/parser/psobject.c		\
	../../src/acpica/source/components/resources/rsaddr.c		\
	../../src/acpica/source/components/resources/rscalc.c		\
	../../src/acpica/source/components/resources/rscreate.c		\
	../../src/acpica/source/components/resources/rsdump.c		\
	../../src/acpica/source/components/resources/rsio.c		\
	../../src/acpica/source/components/resources/rsinfo.c		\
	../../src/acpica/source/components/resources/rsirq.c		\
	../../src/acpica/source/components/resources/rslist.c		\
	../../src/acpica/source/components/resources/rsmemory.c		\
	../../src/acpica/source/components/resources/rsmisc.c		\
	../../src/acpica/source/components/resources/rsutils.c		\
	../../src/acpica/source/components/resources/rsxface.c		\
	../../src/acpica/source/components/resources/rsserial.c		\
	../../src/acpica/source/components/resources/rsdumpinfo.c	\
	../../src/acpica/source/components/tables/tbdata.c		\
	../../src/acpica/source/components/tables/tbfadt.c		\
	../../src/acpica/source/components/tables/tbfind.c		\
	../../src/acpica/source/components/tables/tbinstal.c		\
	../../src/acpica/source/components/tables/tbutils.c		\
	../../src/acpica/source/components/tables/tbxface.c		\
	../../src/acpica/source/components/tables/tbxfroot.c		\
	../../src/acpica/source/components/tables/tbxfload.c		\
	../../src/acpica/source/components/tables/tbprint.c		\
	../../src/acpica/source/components/utilities/utaddress.c	\
	../../src/acpica/source/components/utilities/utalloc.c		\
	../../src/acpica/source/components/utilities/utascii.c		\
	../../src/acpica/source/components/utilities/utcache.c		\
	../../src/acpica/source/components/utilities/utcksum.c		\
	../../src/acpica/source/components/utilities/utcopy.c		\
	../../src/acpica/source/components/utilities/utdebug.c		\
	../../src/acpica/source/components/utilities/utdelete.c		\
	../../src/acpica/source/components/utilities/uteval.c		\
	../../src/acpica/source/components/utilities/utglobal.c		\
	../../src/acpica/source/components/utilities/uthex.c		\
	../../src/acpica/source/components/utilities/utids.c		\
	../../src/acpica/source/components/utilities/utinit.c		\
	../../src/acpica/source/components/utilities/utlock.c		\
	../../src/acpica/source/components/utilities/utmath.c		\
	../../src/acpica/source/components/utilities/utmisc.c		\
	../../src/acpica/source/components/utilities/utmutex.c		\
	../../src/acpica/source/components/utilities/utnonansi.c	\
	../../src/acpica/source/components/utilities/utobject.c		\
	../../src/acpica/source/components/utilities/utresdecode.c	\
	../../src/acpica/source/components/utilities/utresrc.c		\
	../../src/acpica/source/components/utilities/utstate.c		\
	../../src/acpica/source/components/utilities/utstrtoul64.c	\
	../../src/acpica/source/components/utilities/uttrack.c		\
	../../src/acpica/source/components/utilities/utosi.c		\
	../../src/acpica/source/components/utilities/utxferror.c	\
	../../src/acpica/source/components/utilities/utxface.c		\
	../../src/acpica/source/components/utilities/utdecode.c		\
	../../src/acpica/source/components/utilities/utexcep.c		\
	../../src/acpica/source/components/utilities/utpredef.c		\
	../../src/acpica/source/components/utilities/utstring.c		\
	../../src/acpica/source/components/utilities/utstrsuppt.c	\
	../../src/acpica/source/components/utilities/utownerid.c	\
	../../src/acpica/source/components/utilities/utxfinit.c		\
	../../src/acpica/source/components/utilities/uterror.c		\
	../../src/acpica/source/components/utilities/utbuffer.c 	\
	../../src/acpica/source/components/utilities/utuuid.c		\
	../../src/acpica/source/common/acfileio.c			\
	../../src/acpica/source/common/acgetline.c            		\
	../../src/acpica/source/common/ahids.c				\
	../../src/acpica/source/common/cmfsize.c            		\
	../../src/acpica/source/common/ahtable.c			\
	../../src/acpica/source/common/ahuuids.c			\
	../../src/acpica/source/tools/acpiexec/aeinitfile.c		\
	../../src/acpica/source/tools/acpiexec/aehandlers.c		\
	../../src/acpica/source/tools/acpiexec/aeexception.c		\
	../../src/acpica/source/tools/acpiexec/aeregion.c		\
	../../src/acpica/source/tools/acpiexec/aeinstall.c		\
	../../src/acpica/source/os_specific/service_layers/osgendbg.c


-include ../../git.mk
