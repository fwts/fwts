#
# Copyright (C) 2011-2026 Canonical, Ltd.
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

#
# We need to build this as a separate shared library with the iasl functionality built in. We
# just export the fwts_* interface.
#
AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -Wstrict-prototypes			\
	      -fno-strict-aliasing 			\
	      -D_LINUX					\
	      -DACPI_ASL_COMPILER $(CFLAGS)		\
	      -I$(top_srcdir)/src/acpica/source/include	\
	      -I$(top_srcdir)/src/acpica/source/compiler

AM_YFLAGS = -d

#
# Case in-sensitive scanning
#
AM_LFLAGS = -i

ASL_PARSER = 						\
	../../src/acpica/source/compiler/aslcstyle.y	\
	../../src/acpica/source/compiler/aslhelpers.y	\
	../../src/acpica/source/compiler/aslparser.y	\
	../../src/acpica/source/compiler/aslprimaries.y	\
	../../src/acpica/source/compiler/aslresources.y	\
	../../src/acpica/source/compiler/aslrules.y	\
	../../src/acpica/source/compiler/aslsupport.y	\
	../../src/acpica/source/compiler/asltokens.y	\
	../../src/acpica/source/compiler/asltypes.y

ASL_LEXER =
	../../src/acpica/source/compiler/aslcompiler.l	\
	aslsupport.l					\
	aslcompiler.y.h

aslcompiler.y: $(ASL_PARSER)
	m4 -P -I$(top_srcdir)/src/acpica/source/compiler $(top_srcdir)/src/acpica/source/compiler/aslparser.y > aslcompiler.y

aslcompilerlex.c: $(ASL_LEXER)
	${LEX} ${AM_LFLAGS} -PAslCompiler -o$@ $(top_srcdir)/src/acpica/source/compiler/aslcompiler.l

aslcompiler.c: aslcompiler.y
	${YACC} ${AM_YFLAGS} -d -baslcompiler -pAslCompiler $^
	mv aslcompiler.tab.c aslcompiler.c
	cp aslcompiler.tab.h aslcompiler.y.h
aslcompiler.y.h: aslcompiler.c

dtcompilerparserlex.c: $(top_srcdir)/src/acpica/source/compiler/dtcompilerparser.l $(top_srcdir)/src/acpica/source/compiler/dtcompilerparser.y
	${LEX} ${AM_LFLAGS} -PDtCompilerParser -odtcompilerparserlex.c $<
	${YACC} ${AM_YFLAGS} -bdtcompilerparser -pDtCompilerParser $(top_srcdir)/src/acpica/source/compiler/dtcompilerparser.y
	mv dtcompilerparser.tab.c dtcompilerparser.c
	cp dtcompilerparser.tab.h dtcompilerparser.y.h
dtcompilerparser.c dtcompilerparser.y.h: dtcompilerparserlex.c

dtparserlex.c: $(top_srcdir)/src/acpica/source/compiler/dtparser.l $(top_srcdir)/src/acpica/source/compiler/dtparser.y
	${LEX} ${AM_LFLAGS} -PDtParser -odtparserlex.c $<
	${YACC} ${AM_YFLAGS} -bdtparser -pDtParser $(top_srcdir)/src/acpica/source/compiler/dtparser.y
	mv dtparser.tab.c dtparser.c
	cp dtparser.tab.h dtparser.y.h
dtparser.c dtparser.y.h: dtparserlex.c

prparserlex.c: $(top_srcdir)/src/acpica/source/compiler/prparser.l $(top_srcdir)/src/acpica/source/compiler/prparser.y
	${LEX} ${AM_LFLAGS} -PPrParser -oprparserlex.c $<
	${YACC} ${AM_YFLAGS} -bprparser -pPrParser $(top_srcdir)/src/acpica/source/compiler/prparser.y
	mv prparser.tab.c prparser.c
	cp prparser.tab.h prparser.y.h
prparser.c prparser.y.h: prparserlex.c

pkglib_LTLIBRARIES = libfwtsiasl.la

BUILT_SOURCES = aslcompiler.y		\
		aslcompiler.y.h		\
		aslcompilerlex.c	\
		aslcompiler.c		\
		dtcompilerparser.y.h	\
		dtcompilerparserlex.c 	\
		dtcompilerparser.c	\
		dtparser.y.h		\
		dtparserlex.c 		\
		dtparser.c 		\
		prparser.y.h		\
		prparserlex.c		\
		prparser.c

#
# Just export fwts specific API so we don't clash with core ACPICA library
#
libfwtsiasl_la_LDFLAGS = -export-symbols-regex "fwts_.*" -version-info 1:0:0
libfwtsiasl_la_LIBADD = -lpthread
CLEANFILES = $(BUILT_SOURCES)

libfwtsiasl_la_CPPFLAGS = $(AM_CPPFLAGS)
libfwtsiasl_la_SOURCES = 						\
	fwts_iasl_interface.c 						\
	aslcompilerlex.c						\
	aslcompiler.c							\
	dtcompilerparserlex.c						\
	dtcompilerparser.c						\
	dtparserlex.c							\
	dtparser.c							\
	prparserlex.c							\
	prparser.c							\
	../../src/acpica/source/common/adisasm.c			\
	../../src/acpica/source/common/acfileio.c			\
	../../src/acpica/source/common/adfile.c				\
	../../src/acpica/source/common/adwalk.c				\
	../../src/acpica/source/common/ahids.c				\
	../../src/acpica/source/common/ahpredef.c			\
	../../src/acpica/source/common/ahtable.c			\
	../../src/acpica/source/common/ahuuids.c			\
	../../src/acpica/source/compiler/aslallocate.c			\
	../../src/acpica/source/compiler/aslanalyze.c			\
	../../src/acpica/source/compiler/aslascii.c			\
	../../src/acpica/source/compiler/aslbtypes.c			\
	../../src/acpica/source/compiler/aslcache.c			\
	../../src/acpica/source/compiler/aslcodegen.c			\
	../../src/acpica/source/compiler/aslcompile.c			\
	../../src/acpica/source/compiler/asldebug.c			\
	../../src/acpica/source/compiler/aslerror.c			\
	../../src/acpica/source/compiler/aslexternal.c			\
	../../src/acpica/source/compiler/aslfiles.c			\
	../../src/acpica/source/compiler/aslfileio.c			\
	../../src/acpica/source/compiler/aslfold.c			\
	../../src/acpica/source/compiler/aslhelp.c			\
	../../src/acpica/source/compiler/aslhex.c			\
	../../src/acpica/source/compiler/asllength.c			\
	../../src/acpica/source/compiler/asllisting.c			\
	../../src/acpica/source/compiler/asllistsup.c			\
	../../src/acpica/source/compiler/aslload.c			\
	../../src/acpica/source/compiler/asllookup.c			\
	../../src/acpica/source/compiler/aslmain.c			\
	../../src/acpica/source/compiler/aslmap.c			\
	../../src/acpica/source/compiler/aslmapenter.c			\
	../../src/acpica/source/compiler/aslmapoutput.c			\
	../../src/acpica/source/compiler/aslmaputils.c			\
	../../src/acpica/source/compiler/aslmessages.c			\
	../../src/acpica/source/compiler/aslmethod.c			\
	../../src/acpica/source/compiler/aslnamesp.c			\
	../../src/acpica/source/compiler/asloffset.c			\
	../../src/acpica/source/compiler/aslopcodes.c			\
	../../src/acpica/source/compiler/asloperands.c			\
	../../src/acpica/source/compiler/aslopt.c			\
	../../src/acpica/source/compiler/asloptions.c			\
	../../src/acpica/source/compiler/aslparseop.c			\
	../../src/acpica/source/compiler/aslpredef.c			\
	../../src/acpica/source/compiler/aslprepkg.c			\
	../../src/acpica/source/compiler/aslprintf.c			\
	../../src/acpica/source/compiler/aslprune.c			\
	../../src/acpica/source/compiler/aslresource.c			\
	../../src/acpica/source/compiler/aslrestype1.c			\
	../../src/acpica/source/compiler/aslrestype1i.c			\
	../../src/acpica/source/compiler/aslrestype2.c			\
	../../src/acpica/source/compiler/aslrestype2d.c			\
	../../src/acpica/source/compiler/aslrestype2e.c			\
	../../src/acpica/source/compiler/aslrestype2q.c			\
	../../src/acpica/source/compiler/aslrestype2s.c			\
	../../src/acpica/source/compiler/aslrestype2w.c			\
	../../src/acpica/source/compiler/aslstartup.c			\
	../../src/acpica/source/compiler/aslstubs.c			\
	../../src/acpica/source/compiler/aslpld.c			\
	../../src/acpica/source/compiler/asltransform.c			\
	../../src/acpica/source/compiler/asltree.c			\
	../../src/acpica/source/compiler/aslutils.c			\
	../../src/acpica/source/compiler/asluuid.c			\
	../../src/acpica/source/compiler/aslwalks.c			\
	../../src/acpica/source/compiler/aslxref.c			\
	../../src/acpica/source/compiler/aslxrefout.c			\
	../../src/acpica/source/compiler/cvcompiler.c			\
	../../src/acpica/source/compiler/cvdisasm.c			\
	../../src/acpica/source/compiler/cvparser.c			\
	../../src/acpica/source/common/cmfsize.c			\
	../../src/acpica/source/components/debugger/dbfileio.c		\
	../../src/acpica/source/components/disassembler/dmbuffer.c	\
	../../src/acpica/source/components/disassembler/dmcstyle.c	\
	../../src/acpica/source/components/disassembler/dmdeferred.c	\
	../../src/acpica/source/common/dmextern.c			\
	../../src/acpica/source/components/disassembler/dmnames.c	\
	../../src/acpica/source/components/disassembler/dmopcode.c	\
	../../src/acpica/source/components/disassembler/dmresrc.c	\
	../../src/acpica/source/components/disassembler/dmresrcl.c	\
	../../src/acpica/source/components/disassembler/dmresrcl2.c	\
	../../src/acpica/source/components/disassembler/dmresrcs.c	\
	../../src/acpica/source/common/dmrestag.c			\
	../../src/acpica/source/common/dmswitch.c			\
	../../src/acpica/source/common/dmtable.c			\
	../../src/acpica/source/common/dmtables.c			\
	../../src/acpica/source/common/dmtbdump.c			\
	../../src/acpica/source/common/dmtbdump1.c			\
	../../src/acpica/source/common/dmtbdump2.c			\
	../../src/acpica/source/common/dmtbdump3.c			\
	../../src/acpica/source/common/dmtbinfo.c			\
	../../src/acpica/source/common/dmtbinfo1.c			\
	../../src/acpica/source/common/dmtbinfo2.c			\
	../../src/acpica/source/common/dmtbinfo3.c			\
	../../src/acpica/source/components/disassembler/dmutils.c	\
	../../src/acpica/source/components/disassembler/dmwalk.c	\
	../../src/acpica/source/components/dispatcher/dsargs.c		\
	../../src/acpica/source/components/dispatcher/dscontrol.c	\
	../../src/acpica/source/components/dispatcher/dsfield.c		\
	../../src/acpica/source/components/dispatcher/dsobject.c	\
	../../src/acpica/source/components/dispatcher/dsopcode.c	\
	../../src/acpica/source/components/dispatcher/dspkginit.c	\
	../../src/acpica/source/components/dispatcher/dsutils.c		\
	../../src/acpica/source/components/dispatcher/dswexec.c		\
	../../src/acpica/source/components/dispatcher/dswload.c		\
	../../src/acpica/source/components/dispatcher/dswload2.c	\
	../../src/acpica/source/components/dispatcher/dswscope.c	\
	../../src/acpica/source/components/dispatcher/dswstate.c	\
	../../src/acpica/source/compiler/dtcompile.c			\
	../../src/acpica/source/compiler/dtexpress.c			\
	../../src/acpica/source/compiler/dtfield.c			\
	../../src/acpica/source/compiler/dtio.c				\
	../../src/acpica/source/compiler/dtsubtable.c			\
	../../src/acpica/source/compiler/dttable.c			\
	../../src/acpica/source/compiler/dttable1.c			\
	../../src/acpica/source/compiler/dttable2.c			\
	../../src/acpica/source/compiler/dttemplate.c			\
	../../src/acpica/source/compiler/dtutils.c			\
	../../src/acpica/source/components/executer/exconcat.c		\
	../../src/acpica/source/components/executer/exconvrt.c		\
	../../src/acpica/source/components/executer/excreate.c		\
	../../src/acpica/source/components/executer/exdump.c		\
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
	../../src/acpica/source/components/executer/exstore.c		\
	../../src/acpica/source/components/executer/exstoren.c		\
	../../src/acpica/source/components/executer/exstorob.c		\
	../../src/acpica/source/components/executer/exsystem.c		\
	../../src/acpica/source/components/executer/exutils.c		\
	../../src/acpica/source/common/getopt.c				\
	../../src/acpica/source/components/namespace/nsaccess.c		\
	../../src/acpica/source/components/namespace/nsalloc.c		\
	../../src/acpica/source/components/namespace/nsdump.c		\
	../../src/acpica/source/components/namespace/nsnames.c		\
	../../src/acpica/source/components/namespace/nsobject.c		\
	../../src/acpica/source/components/namespace/nsparse.c		\
	../../src/acpica/source/components/namespace/nssearch.c		\
	../../src/acpica/source/components/namespace/nsutils.c		\
	../../src/acpica/source/components/namespace/nswalk.c		\
	../../src/acpica/source/components/namespace/nsxfobj.c		\
	../../src/acpica/source/os_specific/service_layers/osunixxf.c	\
	../../src/acpica/source/compiler/prexpress.c			\
	../../src/acpica/source/compiler/prmacros.c			\
	../../src/acpica/source/compiler/prscan.c			\
	../../src/acpica/source/compiler/prutils.c			\
	../../src/acpica/source/components/parser/psargs.c		\
	../../src/acpica/source/components/parser/psloop.c		\
	../../src/acpica/source/components/parser/psobject.c		\
	../../src/acpica/source/components/parser/psopcode.c		\
	../../src/acpica/source/components/parser/psopinfo.c		\
	../../src/acpica/source/components/parser/psparse.c		\
	../../src/acpica/source/components/parser/psscope.c		\
	../../src/acpica/source/components/parser/pstree.c		\
	../../src/acpica/source/components/parser/psutils.c		\
	../../src/acpica/source/components/parser/pswalk.c		\
	../../src/acpica/source/components/tables/tbdata.c		\
	../../src/acpica/source/components/tables/tbfadt.c		\
	../../src/acpica/source/components/tables/tbinstal.c		\
	../../src/acpica/source/components/tables/tbprint.c		\
	../../src/acpica/source/components/tables/tbutils.c		\
	../../src/acpica/source/components/tables/tbxface.c		\
	../../src/acpica/source/components/tables/tbxfload.c		\
	../../src/acpica/source/components/utilities/utaddress.c	\
	../../src/acpica/source/components/utilities/utalloc.c		\
	../../src/acpica/source/components/utilities/utascii.c		\
	../../src/acpica/source/components/utilities/utbuffer.c		\
	../../src/acpica/source/components/utilities/utcache.c		\
	../../src/acpica/source/components/utilities/utcopy.c		\
	../../src/acpica/source/components/utilities/utdebug.c		\
	../../src/acpica/source/components/utilities/utdecode.c		\
	../../src/acpica/source/components/utilities/utdelete.c		\
	../../src/acpica/source/components/utilities/uterror.c		\
	../../src/acpica/source/components/utilities/utexcep.c		\
	../../src/acpica/source/components/utilities/utglobal.c		\
	../../src/acpica/source/components/utilities/uthex.c		\
	../../src/acpica/source/components/utilities/utinit.c		\
	../../src/acpica/source/components/utilities/utlock.c		\
	../../src/acpica/source/components/utilities/utmath.c		\
	../../src/acpica/source/components/utilities/utmisc.c		\
	../../src/acpica/source/components/utilities/utmutex.c		\
	../../src/acpica/source/components/utilities/utnonansi.c	\
	../../src/acpica/source/components/utilities/utobject.c		\
	../../src/acpica/source/components/utilities/utownerid.c	\
	../../src/acpica/source/components/utilities/utpredef.c		\
	../../src/acpica/source/components/utilities/utresdecode.c	\
	../../src/acpica/source/components/utilities/utresrc.c		\
	../../src/acpica/source/components/utilities/utstate.c		\
	../../src/acpica/source/components/utilities/utstrtoul64.c	\
	../../src/acpica/source/components/utilities/utstrsuppt.c	\
	../../src/acpica/source/components/utilities/utstring.c		\
	../../src/acpica/source/components/utilities/utuuid.c		\
	../../src/acpica/source/components/utilities/utxface.c		\
	../../src/acpica/source/components/utilities/utxferror.c	\
	../../src/acpica/source/components/utilities/utcksum.c

-include ../../git.mk
