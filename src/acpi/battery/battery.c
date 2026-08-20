/*
 * Copyright (C) 2006, Intel Corporation
 * Copyright (C) 2010-2026 Canonical
 *
 * This file was originally part of the Linux-ready Firmware Developer Kit
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
#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

static void battery_discharge(fwts_framework *fw, const int secs)
{
	int i;
	fwts_cpu_consume_start();

	for (i = 0; i < secs; i++) {
		fwts_printf(fw, "Waiting %2.2d/%d\r", secs-i, secs);
		sleep(1);
	}

	fwts_cpu_consume_complete();
}

static uint32_t get_full(fwts_framework *fw, const uint32_t index)
{
        uint32_t capacity_mAh;
        uint32_t capacity_mWh;

	fwts_battery_get_capacity(fw, FWTS_BATTERY_REMAINING_CAPACITY, index, &capacity_mAh, &capacity_mWh);
	if (capacity_mAh != 0) {
		return capacity_mAh;
	}
	if (capacity_mWh != 0) {
		return capacity_mWh;
	}
	return 0;
}

static int wait_for_acpi_event(fwts_framework *fw, const char *name)
{
	int gpe_count = 0;
	int fd;
	int events = 0;
	int matching = 0;
	size_t len;
	int i;

	fwts_gpe *gpes_start;
	fwts_gpe *gpes_end;

	if ((gpe_count = fwts_gpe_read(&gpes_start)) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot read GPEs.");
		return FWTS_ERROR;
	}

	if ((fd = fwts_acpi_event_open()) < 0) {
		fwts_log_error(fw, "Cannot connect to acpid.");
		fwts_gpe_free(gpes_start, gpe_count);
		return FWTS_ERROR;
	}

	for (i = 0; i <= 20; i++) {
		char *buffer;

		if ((buffer = fwts_acpi_event_read(fd, &len, 1)) != NULL) {
			char *str;
			if ((str = strstr(buffer, "battery")) != NULL) {
				events++;
				if (strstr(str, name) != NULL) {
					matching++;
					free(buffer);
					break;
				}
			}
			free(buffer);
		}
		fwts_printf(fw, "Waiting %2.2d/20\r", 20-i);
	}
	fwts_acpi_event_close(fd);

	if ((gpe_count = fwts_gpe_read(&gpes_end)) == FWTS_ERROR) {
		fwts_log_error(fw, "Cannot read GPEs.");
		fwts_gpe_free(gpes_start, gpe_count);
		return FWTS_ERROR;
	}

	fwts_gpe_test(fw, gpes_start, gpes_end, gpe_count);
	fwts_gpe_free(gpes_start, gpe_count);
	fwts_gpe_free(gpes_end, gpe_count);

	if (events == 0)
		fwts_failed(fw, LOG_LEVEL_HIGH, "BatteryNoEvents",
			"Did not detect any ACPI battery events.");
	else {
		fwts_passed(fw, "Detected ACPI battery events.");
		if (matching == 0)
			fwts_failed(fw, LOG_LEVEL_HIGH, "BatteryNoEvents",
			"Could not detect ACPI events for battery %s.", name);
		else
			fwts_passed(fw, "Detected ACPI event for battery %s.",
				name);
	}

	return FWTS_OK;
}
static void check_charging(
	fwts_framework *fw,
	const uint32_t index,
	const char *name)
{
	uint32_t i;
	/* when we get here we KNOW the state is "charging" */
	uint32_t initial_value;

	fwts_printf(fw, "==== Waiting to see if battery '%s' charges ====\n", name);

	initial_value = get_full(fw, index);
	for (i = 0; i <= 120; i++) {
		uint32_t new_value = get_full(fw, index);

		if (new_value>initial_value) {
			fwts_passed(fw, "Battery %s charge is incrementing as expected.", name);
			return;
		}
		fwts_printf(fw, "Waiting %3.3d/120\r", 120-i);
		sleep(1);
	}
	fwts_failed(fw, LOG_LEVEL_MEDIUM, "BatteryNotCharging",
		"Battery %s claims it's charging but no charge is added", name);
}

static void check_discharging(
	fwts_framework *fw,
	const uint32_t index,
	const char *name)
{
	uint32_t i;
	/* when we get here we KNOW the state is "discharging" */
	uint32_t initial_value;

	fwts_printf(fw, "==== Waiting to see if battery '%s' discharges ====\n", name);
	fwts_cpu_consume_start();

	initial_value = get_full(fw, index);
	for (i = 0; i <= 120; i++) {
		uint32_t new_value = get_full(fw, index);

		if (new_value<initial_value) {
			fwts_passed(fw, "Battery %s charge is decrementing as expected.", name);
			fwts_cpu_consume_complete();
			return;
		}
		fwts_printf(fw, "Waiting %3.3d/120\r", 120-i);
		sleep(1);
	}
	fwts_cpu_consume_complete();
	fwts_failed(fw, LOG_LEVEL_MEDIUM, "BatteryNotDischarging",
		"Battery %s claims it is discharging but no charge is used.",
		name);
}

static void check_battery_cycle_count(
	fwts_framework *fw,
	const uint32_t index,
	const char *name)
{
	uint32_t cycle_count;

	fwts_printf(fw, "==== Checking cycle count of battery '%s' ====\n", name);
	if (fwts_battery_get_cycle_count(fw, index, &cycle_count) == FWTS_OK) {
		if (cycle_count == 0) {
			fwts_log_info(fw,
				"Please ignore this error with a new battery");
			fwts_failed(fw, LOG_LEVEL_LOW, "BatteryZeroCycleCount",
			"System firmware may not support cycle count interface "
			"or it reports it incorrectly for battery %s.",
			name);
		}
	}

}

static void check_battery_trip_point(
	fwts_framework *fw,
	const uint32_t index,
	const char *name)
{
	uint32_t trip_point;
	uint32_t trip_point_org;

	fwts_printf(fw, "==== Checking trip point of battery '%s' ====\n", name);

	if (!fwts_battery_check_trip_point_support(fw, index)) {
		fwts_printf(fw, "==== Not supported - skip test ====\n");
		return;
	}

	if (fwts_battery_get_trip_point(fw, index, &trip_point) == FWTS_OK)
		trip_point_org = trip_point;
	else
		trip_point_org = 0;

	fwts_log_info(fw, "Test battery '%s' downward trip point.", name);
	fwts_printf(fw, "==== Please now UNPLUG the AC power of the machine ====\n");
	fwts_press_enter(fw);
	sleep(1);
	trip_point = get_full(fw, index) - 5;
	fwts_battery_set_trip_point(fw, index, trip_point);
	fwts_cpu_consume_start();
	wait_for_acpi_event(fw, name);
	fwts_cpu_consume_complete();

	fwts_log_info(fw, "Test battery '%s' upwards trip point.", name);
	fwts_printf(fw, "==== Please PLUG IN the AC power of the machine ====\n");
	fwts_press_enter(fw);
	sleep(1);
	trip_point = get_full(fw, index) + 3;
	fwts_battery_set_trip_point(fw, index, trip_point);
	wait_for_acpi_event(fw, name);

	if (trip_point_org != 0)
		fwts_battery_set_trip_point(fw, index, trip_point_org);

}

static void do_battery_test(fwts_framework *fw, const uint32_t index)
{
	char name[PATH_MAX];
	char state[1024];

	*state = '\0';

	if (fwts_battery_get_name(fw, index, name, sizeof(name)) != FWTS_OK)
		fwts_log_info(fw, "Cannot find batter name: cannot test.");

	fwts_log_info(fw, "Test battery '%s'.", name);

	fwts_printf(fw, "==== Please PLUG IN the AC power of the machine ====\n");
	fwts_press_enter(fw);

	fwts_printf(fw, "==== Please now UNPLUG the AC power of the machine ====\n");
	wait_for_acpi_event(fw, name);
	check_discharging(fw, index, name);
	fwts_printf(fw, "==== Please wait 30 seconds while the battery is discharged a little ====\n");
	battery_discharge(fw, 30);
	fwts_printf(fw, "==== Please now PLUG IN the AC power of the machine ====\n");
	wait_for_acpi_event(fw, name);
	check_charging(fw, index, name);
	check_battery_cycle_count(fw, index, name);
	check_battery_trip_point(fw, index, name);
}

static int battery_test1(fwts_framework *fw)
{
	uint32_t count = 0;
	uint32_t i;

	fwts_log_info(fw,
	   "This test reports which (if any) batteries there are in the system. "
	   "In addition, for charging or discharging batteries, the test validates "
	   "that the reported 'current capacity' properly increments/decrements "
	   "in line with the charge/discharge state. "
	   "This test also stresses the battery state reporting codepath "
	   "in the ACPI BIOS, and any warnings given by the ACPI interpreter "
	   "will be reported.");

	if (fwts_battery_get_count(fw, &count) != FWTS_OK) {
		fwts_log_info(fw, "No battery information present: cannot test.");
		return FWTS_OK;
	}

	fwts_log_info(fw, "Found %" PRIu32 " batteries.", count);

	for (i = 0; i < count; i++)
		do_battery_test(fw, i);

	return FWTS_OK;
}

static fwts_framework_minor_test battery_tests[] = {
	{ battery_test1, "Battery test." },
	{ NULL, NULL }
};

static fwts_framework_ops battery_ops = {
	.description = "Battery tests.",
	.minor_tests = battery_tests
};

FWTS_REGISTER("battery", &battery_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_INTERACTIVE)

#endif
