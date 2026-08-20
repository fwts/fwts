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


#
# Get fwts sources, strip out .git directory, add in necessary debian packaging
# files, build source package ready for upload.
#
REPO=https://github.com/fwts/fwts
FWTS=fwts

#
#  Clone the repo
#
get_source()
{
	echo "Getting source"
	pushd "$version" >& /dev/null
	git clone $REPO
	popd >& /dev/null
}

#
#  Checkout version
#
checkout_version()
{
	echo "Checking out version"
	pushd "$version/$FWTS" >& /dev/null
	git checkout -b latest $version
	popd >& /dev/null
}

#
#  Remove the source
#
rm_source()
{
	rm -rf "$version/$FWTS"
}

#
#  Create source package ready for upload and build
#
mk_tarball()
{
	pushd "$version/$FWTS" >& /dev/null
	git archive --format tar -o ../fwts-$version.tar $version
	gzip ../fwts-$version.tar
	popd >& /dev/null
}

#
# Create checksums
#

mk_checksums()
{
	pushd "$version" >& /dev/null
	sha256sum fwts-$version.tar.gz >> SHA256SUMS
	popd >& /dev/null
}

#
#  Here we go..
#
if [ $# -eq 1 ]; then
	version=$1
	mkdir $version
else
	echo "Please specify a version"
	exit
fi

rm_source
get_source

checkout_version

mk_tarball
mk_checksums

rm_source
