#!/bin/sh
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

#
# Generate a Debian orig tarball from latest tagged release
#
TAG=$(git tag | tail -1 | sed s/V//)
git reset --hard V${TAG}
git clean -fd
echo "Making debian orig tarball for fwts V${TAG}"
rm -f m4/* ../fwts_*
git archive V${TAG} -o ../fwts_${TAG}.orig.tar
gzip -9 ../fwts_${TAG}.orig.tar 
