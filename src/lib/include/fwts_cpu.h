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

#ifndef __FWTS_CPU_H__
#define __FWTS_CPU_H__

#include "fwts_framework.h"

#include <stdbool.h>

typedef struct cpuinfo_x86 {
	char *vendor_id;	/* Vendor ID */
	int x86;		/* CPU family */
	int x86_model;		/* Model */
	char *model_name;	/* Model name */
	int stepping;		/* Stepping */
	char *flags;		/* String containing flags */
} fwts_cpuinfo_x86;

/* PowerPC Processor specific bits */
/* PVR definitions */
#define PVR_TYPE_P7     0x003f
#define PVR_TYPE_P7P    0x004a
#define PVR_TYPE_P8E    0x004b /* Murano */
#define PVR_TYPE_P8     0x004d /* Venice */
#define PVR_TYPE_P8NVL  0x004c /* Naples */
#define PVR_TYPE_P9     0x004e

/* Processor generation */
typedef enum proc_gen {
	proc_gen_unknown,
	proc_gen_p7,            /* P7 and P7+ */
	proc_gen_p8,
	proc_gen_p9,
} proc_gen_t;
extern proc_gen_t proc_gen;

typedef struct cpu_benchmark_result {
	bool		cycles_valid;
	uint64_t	loops;
	uint64_t	cycles;
} fwts_cpu_benchmark_result;

int fwts_cpu_readmsr(fwts_framework *fw, const int cpu, const uint32_t reg, uint64_t *val);

int fwts_cpu_is_Intel(bool *is_intel);
int fwts_cpu_is_AMD(bool *is_amd);
int fwts_cpu_is_Hygon(bool *is_hygon);

fwts_bool fwts_cpu_has_c1e(void);
fwts_cpuinfo_x86 *fwts_cpu_get_info(const int which_cpu);
void fwts_cpu_free_info(fwts_cpuinfo_x86 *cpu);

int fwts_cpu_enumerate(void);
int fwts_cpu_consume(const int seconds);
int fwts_cpu_consume_start(void);
void fwts_cpu_consume_complete(void);
int fwts_cpu_benchmark(fwts_framework *fw, const int cpu,
		fwts_cpu_benchmark_result *result);

uint64_t fwts_cpu_benchmark_best_result(fwts_cpu_benchmark_result *res);

#endif
