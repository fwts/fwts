#!/bin/bash
if [ $# -ne 1 ];
then
	echo "You need to specify the version number"
	echo "The last version was" `git tag | tail -1`
	exit 1
fi
version=$1
cat << EOF > src/lib/include/fwts_version.h
/*
 * Copyright (C) 2010-2026 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */
EOF
# Update fwts_version
echo '#define FWTS_VERSION "'$version'"' >> src/lib/include/fwts_version.h
echo '#define FWTS_DATE    "'`date --utc "+%F %T"`'"' >> src/lib/include/fwts_version.h
# Update snapcraft version
oldsnap_version=$(grep "version: " snapcraft.yaml | cut -d' ' -f2)
sed -i "s/version: ${oldsnap_version}/version: ${version}/" snapcraft.yaml
# Update git repo
git add src/lib/include/fwts_version.h
git add snapcraft.yaml
git commit -s -m"lib: fwts_version.h - update to $version"
git tag -m'"Version '$1'"' $1
