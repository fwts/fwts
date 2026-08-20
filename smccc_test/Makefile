#
# Copyright (C) 2021-2026 Canonical, Ltd.
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

KVER ?= `uname -r`
KBUILD_MODPOST_WARN=y
obj-m += smccc_test.o
all:
	make -C /lib/modules/$(KVER)/build M=`pwd` modules

install:
	make -C /lib/modules/$(KVER)/build M=`pwd` modules_install

clean:
	make -C /lib/modules/$(KVER)/build M=`pwd` clean
