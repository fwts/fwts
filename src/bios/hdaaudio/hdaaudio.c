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
#include <sys/types.h>
#include <dirent.h>

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

typedef struct {
	uint16_t	pin;
	uint32_t	setting;
} hda_audio_pin_setting;

static int hda_audio_read_pins(const char *path, const char *file, fwts_list *settings)
{
	FILE 		*fp;
	uint16_t	pin;
	uint32_t	setting;
	char 		name[PATH_MAX*2];

	fwts_list_init(settings);

	snprintf(name, sizeof(name), "%s/%s", path, file);

	if ((fp = fopen(name, "r")) == NULL)
		return FWTS_ERROR;

	while (fscanf(fp, "0x%hx 0x%x\n", &pin, &setting) == 2) {
		hda_audio_pin_setting *pin_setting;

		pin_setting  = calloc(1, sizeof(hda_audio_pin_setting));
		if (pin_setting == NULL) {
			fwts_list_free_items(settings, free);
			(void)fclose(fp);
			return FWTS_ERROR;
		}
		pin_setting->pin = pin;
		pin_setting->setting = setting;
		fwts_list_append(settings, pin_setting);
	}
	(void)fclose(fp);

	return FWTS_OK;
}

static void hda_audio_dump_pins(fwts_framework *fw, const char *config,
	fwts_list *settings)
{
	if (fwts_list_len(settings) > 0) {
		fwts_list_link *item;

		fwts_log_info(fw, "%s:", config);

		fwts_log_info_verbatim(fw, "  Pin  Setting");
		fwts_list_foreach(item, settings) {
			hda_audio_pin_setting *pin_setting =
				fwts_list_data(hda_audio_pin_setting *, item);

			fwts_log_info_verbatim(fw, "  0x%4.4hx 0x%8.8x",
				pin_setting->pin, pin_setting->setting);
		}
	} else
		fwts_log_info(fw, "%s: None Defined.", config);
}

static void hda_audio_dev_info(fwts_framework *fw, const char *label,
	const char *path, const char *file)
{
	char *info;
	char fullpath[PATH_MAX*2];

	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, file);

	if ((info = fwts_get(fullpath)) != NULL) {
		fwts_chop_newline(info);
		if (*info)
			fwts_log_info_verbatim(fw, "%-15.15s: %s", label, info);
		free(info);
	}
}

static int hda_audio_check_pins(fwts_framework *fw, const char *path)
{
	fwts_list	init_pin_configs;
	fwts_list	driver_pin_configs;
	fwts_list	user_pin_configs;
	int		warn = 0;

	hda_audio_dev_info(fw, "Vendor Name", path, "vendor_name");
	hda_audio_dev_info(fw, "Vendor ID", path, "vendor_id");
	hda_audio_dev_info(fw, "Model Name", path, "modelname");
	hda_audio_dev_info(fw, "Chip Name", path, "chipname");
	hda_audio_dev_info(fw, "Subsystem ID", path, "subsystem_id");
	hda_audio_dev_info(fw, "Revision ID", path, "revision_id");

	(void)hda_audio_read_pins(path, "init_pin_configs", &init_pin_configs);
	(void)hda_audio_read_pins(path, "driver_pin_configs", &driver_pin_configs);
	(void)hda_audio_read_pins(path, "user_pin_configs", &user_pin_configs);

	if (fwts_list_len(&init_pin_configs) > 0)
		hda_audio_dump_pins(fw, "BIOS pin configurations",
			&init_pin_configs);

	if (fwts_list_len(&driver_pin_configs) > 0) {
		hda_audio_dump_pins(fw, "Driver defined pin configurations",
			&driver_pin_configs);
		warn++;
	}

	if (fwts_list_len(&user_pin_configs) > 0) {
		hda_audio_dump_pins(fw, "User defined pin configurations",
			&user_pin_configs);
		warn++;
	}

	if (warn) {
		fwts_log_warning(fw,
			"BIOS pin configurations required software override "
			"to make HDA audio work correctly.");
		fwts_log_advice(fw,
			"The driver or user provided overrides should be "
			"corrected in BIOS firmware.");
	} else
		fwts_passed(fw,
			"Default BIOS pin configurations did not have "
			"software override.");

	fwts_list_free_items(&user_pin_configs, free);
	fwts_list_free_items(&driver_pin_configs, free);
	fwts_list_free_items(&init_pin_configs, free);

	return FWTS_OK;
}

static int hda_audio_test1(fwts_framework *fw)
{
	DIR *dir;
	struct dirent *directory;
	bool checked = false;

	if ((dir = opendir("/sys/class/sound")) == NULL) {
		fwts_skipped(fw, "Cannot find any sound devices in /sys/class/sound.");
		return FWTS_OK;
	}

	while ((directory = readdir(dir)) != NULL)
		if (strncmp(directory->d_name, "hw", 2) == 0) {
			char path[PATH_MAX];

			checked = true;
			snprintf(path, sizeof(path), "/sys/class/sound/%s",
				directory->d_name);
			fwts_log_info(fw, "Checking '%s':", directory->d_name);
			hda_audio_check_pins(fw, path);
			fwts_log_nl(fw);
		}

	(void)closedir(dir);

	if (!checked)
		fwts_skipped(fw, "Cannot find any BIOS set audio pin configurations.");

	return FWTS_OK;
}

static fwts_framework_minor_test hda_audio_tests[] = {
	{ hda_audio_test1, "HDA Audio Pin Configuration test." },
	{ NULL, NULL }
};

static fwts_framework_ops hda_audio_ops = {
	.description = "HDA Audio Pin Configuration test.",
	.minor_tests = hda_audio_tests
};

FWTS_REGISTER("hda_audio", &hda_audio_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_BATCH)

#endif
