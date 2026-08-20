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
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

/*
 *  fwts_tty_width()
 *	try and find width of tty. If it cannot be determined
 *	then simply return the default_width
 */
int fwts_tty_width(const int fd, const int default_width)
{
#ifdef TIOCGWINSZ
	struct winsize ws;

	/* Zero'ing ws keeps static analyzers happy */
	(void)memset(&ws, 0, sizeof(ws));
	/* if tty and we can get a sane width, return it */
	if (isatty(fd) &&
	    (ioctl(fd, TIOCGWINSZ, &ws) != -1) &&
	    (0 < ws.ws_col) &&
	    (ws.ws_col == (size_t)ws.ws_col))
		return ws.ws_col;
	else
#endif
	/* not supported or failed to get, return default */
	return default_width;
}
