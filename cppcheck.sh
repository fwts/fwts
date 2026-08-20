#!/bin/bash
#
# Copyright (C) 2016-2026 Canonical
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

CPPCHECK_DIR=/tmp/cppcheck
CPPCHECK=${CPPCHECK_DIR}/cppcheck
CPPCHECK_REPO=https://github.com/danmar/cppcheck
CPPCHECK_LOG=cppcheck.log
DEPENDENCIES="git build-essential"
JOBS=$(nproc)
HERE=$(pwd)

#
#  Install any packages we depend on to build cppcheck
#
cppcheck_install_dependencies()
{
	install=""

	echo "Checking for dependencies for cppcheck.."

	for d in ${DEPENDENCIES}
	do
		if [ "$(dpkg -l | grep $d)" == "" ]; then
			install="$install $d"
		fi
	done
	if [ "$install" != "" ]; then
		echo "Need to install:$install"
		sudo apt-get install $install
		if [ $? -ne 0 ]; then
			echo "Installation of packages failed"
			exit 1
		fi
	fi
}

#
#  Get an up to date version of cppcheck
#
cppcheck_get()
{
	if [ -d ${CPPCHECK_DIR} ]; then
		echo "Getting latest version of cppcheck.."
		mkdir -p ${CPPCHECK_DIR}
		cd ${CPPCHECK_DIR}
		git checkout -f master >& /dev/null
		git fetch origin >& /dev/null
		git fetch origin master >& /dev/null
		git reset --hard FETCH_HEAD >& /dev/null
		cd ${HERE}
	else
		echo "Getting cppcheck.."
		git clone ${CPPCHECK_REPO} ${CPPCHECK_DIR}
	fi
}

#
#  Build cppcheck
#
cppcheck_build()
{
	cd ${CPPCHECK_DIR}
	echo "cppcheck: make clean.."
	make clean >& /dev/null
	echo "cppcheck: make.."
	nice make -j $JOBS > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "Build of cppcheck succeeded"
	else
		echo "Build  of cppcheckfailed"
		exit 1
	fi
	cd ${HERE}
}

#
#  Build fwts using cppcheck
#
cppcheck_fwts()
{
	echo "cppchecking fwts.."
	rm -f ${CPPCHECK_LOG}
	nice ${CPPCHECK} --force -j $JOBS --enable=all . 2>&1 | tee ${CPPCHECK_LOG}
}

#
#  Check for errors
#
cppcheck_errors()
{
	errors=$(grep "(error" ${CPPCHECK_LOG} | wc -l)
	warnings=$(grep "(warning" ${CPPCHECK_LOG} | wc -l)
	echo " "
	echo "cppcheck found $errors errors and $warnings warnings, see ${CPPCHECK_LOG} for more details."
}

cppcheck_install_dependencies
cppcheck_get
cppcheck_build
cppcheck_fwts
cppcheck_errors
