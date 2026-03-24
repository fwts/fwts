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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <linux/input.h>
#include <fcntl.h>

#define AT_KEYBOARD	"AT Translated Set 2 keyboard"

static fwts_list *hotkeys_list;
static char *hotkey_dev;
static char *hotkey_keymap;

static int hotkey_check_key(
	fwts_framework *fw,
	const struct input_event *ev,
	const fwts_list *hotkeys)
{
	static int scancode = 0;

	if ((ev->code == MSC_SCAN) &&
	    (ev->type == EV_MSC))
		scancode = ev->value;

	if ((ev->type == EV_KEY) &&
	    (ev->value != 0)) {
		fwts_list_link *item;
		int found = 0;

		fwts_list_foreach(item, hotkeys) {
			const fwts_keycode *keycode =
				fwts_list_data(const fwts_keycode *, item);
			if (keycode->scancode == scancode) {
				fwts_printf(fw, "Scancode: 0x%2.2x Eventcode 0x%3.3x (%s) '%s'\n",
					scancode, ev->code,
					keycode->keyname, keycode->keytext);
				found = 1;
				break;
			}
		}
		if (!found)
			fwts_printf(fw, "Scancode: 0x%2.2x Eventcode 0x%3.3x\n",
				scancode, ev->code);
	}
	return FWTS_OK;
}


static int hotkey_test(
	fwts_framework *fw,
	const char *dev,
	const fwts_list *hotkeys)
{
	struct input_event ev;
	char path[PATH_MAX];
	int fd;

	snprintf(path, sizeof(path), "/dev/%s", dev);
	if ((fd = open(path, O_RDWR)) < 0) {
		fwts_log_error(fw, "Cannot open device %s.", path);
		return FWTS_ERROR;
	}

	fwts_printf(fw, "Now press all the hotkeys to see if they can be identified.\n");
	fwts_printf(fw, "Press <ESC> to finish.\n");

	if (ioctl(fd, EVIOCGRAB, (void*)1)) {	/* Get focus */
		fwts_log_error(fw, "Cannot grab device %s.", path);
		(void)close(fd);
		return FWTS_ERROR;
	}

	for (;;) {
		const ssize_t ret = read(fd, &ev, sizeof(ev));

		if (ret < (ssize_t)sizeof(ev))
			break;
		if ((ev.type == EV_KEY) &&
		    (ev.code == KEY_ESC) &&
		    (ev.value == 0)) {
			break;
		} else {
			hotkey_check_key(fw, &ev, hotkeys);
		}
	}

	if (ioctl(fd, EVIOCGRAB, (void*)0)) {	/* Release */
		fwts_log_error(fw, "Cannot un-grab device %s.", path);
		(void)close(fd);
		return FWTS_ERROR;
	}
	(void)close(fd);

	return FWTS_OK;
}

static char *hotkey_device(const char *path)
{
	DIR *scan;
	struct dirent *scan_entry;
	char *dev = NULL;

	if ((scan = opendir(path)) == NULL)
		return NULL;

	while ((scan_entry = readdir(scan)) != NULL) {
		if (strncmp("event", scan_entry->d_name, 5) == 0) {
			char filename[PATH_MAX];

			snprintf(filename, sizeof(filename),
				"input/%s", scan_entry->d_name);
			dev = strdup(filename);
			break;
		}
	}
	(void)closedir(scan);

	return dev;
}

static char *hotkey_find_keyboard(const char *path)
{
	DIR *dir;
	struct dirent *entry;
	struct stat statbuf;
	char filename[PATH_MAX];
	char *dev = NULL;
	char *data;

	if ((dir = opendir(path)) == NULL)
		return NULL;

	while ((entry = readdir(dir)) != NULL) {
		if (strlen(entry->d_name) > 3) {
			snprintf(filename, sizeof(filename), "%s/%s",
				path, entry->d_name);
			if (lstat(filename, &statbuf) == 0) {
				if (S_ISDIR(statbuf.st_mode)) {
					if (!S_ISLNK(statbuf.st_mode))
						if ((dev = hotkey_find_keyboard(filename)) != NULL)
							break;
				} else {
					if ((data = fwts_get(filename)) != NULL) {
						if (strncmp(data, AT_KEYBOARD, sizeof(AT_KEYBOARD)-1) == 0) {
							dev = hotkey_device(path);
							free(data);
							break;
						}
						free(data);
					}
				}
			}
		}
	}

	(void)closedir(dir);

	return dev;
}

static char *hotkey_find_keymap(const char *device)
{
	fwts_list *output;
	fwts_list_link *item;

	char buffer[1024];
	char *keymap = NULL;
	int status;

	snprintf(buffer, sizeof(buffer), "udevadm test /class/%s 2>&1", device);
	if (fwts_pipe_exec(buffer, &output, &status) != FWTS_OK)
		return NULL;

	snprintf(buffer, sizeof(buffer), "keymap %s", device);
	fwts_list_foreach(item, output) {
		char *text = fwts_text_list_text(item);

		if ((text = strstr(text, buffer)) != NULL) {
			char *ptr;

			text += strlen(buffer) + 1;
			if ((ptr = strstr(text, "'")) != NULL)
				*ptr = '\0';
			keymap = strdup(text);
			break;
		}
	}
	fwts_list_free(output, free);

	return keymap;
}

static int hotkey_init(fwts_framework *fw)
{
	if ((hotkey_dev = hotkey_find_keyboard("/sys/devices/platform")) == NULL) {
		fwts_log_error(fw, "Cannot find keyboard for this machine.");
		return FWTS_ERROR;
	}
	if ((hotkey_keymap = hotkey_find_keymap(hotkey_dev)) == NULL) {
		fwts_log_error(fw, "Cannot determine keymap for this machine.");
		return FWTS_ERROR;
	}
	if ((hotkeys_list = fwts_keymap_load(hotkey_keymap)) == NULL) {
		fwts_log_error(fw, "Cannot load keymap for this machine.");
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

static int hotkey_deinit(fwts_framework *fw)
{
	FWTS_UNUSED(fw);

	fwts_keymap_free(hotkeys_list);
	free(hotkey_dev);
	free(hotkey_keymap);
	return FWTS_OK;
}

static int hotkey_test1(fwts_framework *fw)
{
	fwts_log_info(fw,
		"This interactive test looks up key presses in the "
		"keymap and returns any known keymap names for the "
		"keys.");
	fwts_log_nl(fw);
	fwts_log_info(fw, "Using %s keymap and %s input device.", hotkey_keymap, hotkey_dev);
	hotkey_test(fw, hotkey_dev, hotkeys_list);

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test hotkey_tests[] = {
	{ hotkey_test1, "Hotkey keypress checks." },
	{ NULL, NULL }
};

static fwts_framework_ops hotkey_ops = {
	.description = "Hotkey scan code tests.",
	.init        = hotkey_init,
	.deinit      = hotkey_deinit,
	.minor_tests = hotkey_tests
};

FWTS_REGISTER("hotkey", &hotkey_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_INTERACTIVE | FWTS_FLAG_ROOT_PRIV);

#endif
