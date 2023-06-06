/*
 * Copyright (C) 2010-2023 Canonical
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

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>

#include "fwts.h"

/* ACPICA specific headers */
#include "acpi.h"
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acdebug.h"
#include "actables.h"
#include "acinterp.h"
#include "acapps.h"
#include "amlresrc.h"
#include "aecommon.h"

#define ACPI_MAX_INIT_TABLES		(64)	/* Number of ACPI tables */

#define MAX_SEMAPHORES			(2048)	/* For semaphore tracking */
#define MAX_THREADS			(128)	/* For thread tracking */

#define MAX_WAIT_TIMEOUT		(20)	/* Seconds */

#define ACPI_ADR_SPACE_USER_DEFINED1	(0x80)
#define ACPI_ADR_SPACE_USER_DEFINED2	(0xE4)

/*
 *  Semaphore accounting info
 */
typedef struct {
	sem_t		sem;	/* Semaphore handle */
	int		count;	/* count > 0 if acquired */
	bool		used;	/* Semaphore being used flag */
} sem_info;

/*
 *  Used to account for threads used by AcpiOsExecute
 */
typedef struct {
	bool		used;	/* true if the thread accounting info in use by a thread */
	pthread_t	thread;	/* thread info */
} fwts_thread;

typedef void * (*pthread_callback)(void *);

BOOLEAN AcpiGbl_AbortLoopOnTimeout = FALSE;
BOOLEAN AcpiGbl_IgnoreErrors = FALSE;
BOOLEAN AcpiGbl_VerboseHandlers = FALSE;
UINT8   AcpiGbl_RegionFillValue = 0;
INIT_FILE_ENTRY *AcpiGbl_InitEntries = NULL;
UINT32  AcpiGbl_InitFileLineCount = 0;

static ACPI_TABLE_DESC		Tables[ACPI_MAX_INIT_TABLES];	/* ACPICA Table descriptors */
static bool			region_handler_called;		/* Region handler tracking */

static sem_info			sem_table[MAX_SEMAPHORES];	/* Semaphore accounting for AcpiOs*Semaphore() */
static pthread_mutex_t		mutex_lock_sem_table;		/* Semaphore accounting mutex */

static fwts_thread		threads[MAX_THREADS];		/* Thread accounting for AcpiOsExecute */
static pthread_mutex_t		mutex_thread_info;		/* Thread accounting mutex */

/*
 *  Static copies of ACPI tables used by ACPICA execution engine
 */
static ACPI_TABLE_XSDT 		*fwts_acpica_XSDT;
static ACPI_TABLE_RSDT          *fwts_acpica_RSDT;
static ACPI_TABLE_RSDP		*fwts_acpica_RSDP;
static ACPI_TABLE_FADT		*fwts_acpica_FADT;
static void 			*fwts_acpica_DSDT;

static fwts_framework		*fwts_acpica_fw;		/* acpica context copy of fw */
static bool			fwts_acpica_init_called;	/* > 0, ACPICA initialised */

/* Semaphore Tracking */

/*
 *  fwts_acpica_sem_count_clear()
 *	clear refs to semaphores and per semaphore count
 */
void fwts_acpica_sem_count_clear(void)
{
	int i;

	pthread_mutex_lock(&mutex_lock_sem_table);

	for (i = 0; i < MAX_SEMAPHORES; i++)
		sem_table[i].count = 0;

	pthread_mutex_unlock(&mutex_lock_sem_table);
}

/*
 *  fwts_acpica_sem_count_get()
 *	collect up number of semaphores that were acquire and release
 *	during a method evaluation
 */
void fwts_acpica_sem_count_get(int *acquired, int *released)
{
	int i;
	*acquired = 0;
	*released = 0;

	/* Wait for any pending threads to complete */

	for (i = 0; i < MAX_THREADS; i++) {
		pthread_mutex_lock(&mutex_thread_info);
		if (threads[i].used) {
			pthread_mutex_unlock(&mutex_thread_info);

			/* Wait for thread to complete */
			pthread_join(threads[i].thread, NULL);

			pthread_mutex_lock(&mutex_thread_info);
			threads[i].used = false;
		}
		pthread_mutex_unlock(&mutex_thread_info);
	}

	/*
	 * All threads (such as Notify() calls now complete, so
	 * we can now do the semaphore accounting calculations
	 */
	pthread_mutex_lock(&mutex_lock_sem_table);
	for (i = 0; i < MAX_SEMAPHORES; i++) {
		if (sem_table[i].used) {
			(*acquired)++;
			if (sem_table[i].count == 0)
				(*released)++;
		}
	}
	pthread_mutex_unlock(&mutex_lock_sem_table);
}

/* ACPICA Handlers */

/*
 *  fwts_notify_handler()
 *	Notify handler
 */
static void fwts_notify_handler(ACPI_HANDLE Device, UINT32 Value, void *Context)
{
	(void)AcpiEvaluateObject(Device, "_NOT", NULL, NULL);
}

static void fwts_device_notify_handler(ACPI_HANDLE Device, UINT32 Value, void *Context)
{
	(void)AcpiEvaluateObject(Device, "_NOT", NULL, NULL);
}

static UINT32 fwts_interface_handler(ACPI_STRING InterfaceName, UINT32 Supported)
{
	return Supported;
}

static ACPI_STATUS fwts_table_handler(UINT32 Event, void *Table, void *Context)
{
	return AE_OK;
}

static UINT32 fwts_event_handler(void *Context)
{
	return 0;
}

static ACPI_STATUS fwts_exception_handler(
	ACPI_STATUS AmlStatus,
	ACPI_NAME Name,
	UINT16 Opcode,
	UINT32 AmlOffset,
	void *Context)
{
	char *exception = (char*)AcpiFormatException(AmlStatus);

	if (Name)
		fwts_log_info(fwts_acpica_fw,
			"ACPICA Exception %s during execution of method %4.4s",
			exception, (char*)&Name);
	else
		fwts_log_info(fwts_acpica_fw,
			"ACPICA Exception %s during execution at module level (table load)",
			exception);

	if (AcpiGbl_IgnoreErrors) {
		if (AmlStatus != AE_OK) {
			fwts_log_info(fwts_acpica_fw,
				"ACPICA Exception override, forcing AE_OK for exception %s",
				exception);
			AmlStatus = AE_OK;
		}
	}

	return AmlStatus;
}

static void fwts_attached_data_handler(ACPI_HANDLE obj, void *data)
{
}

static ACPI_STATUS fwts_region_init(
	ACPI_HANDLE RegionHandle,
	UINT32 Function,
	void *HandlerContext,
	void **RegionContext)
{
	*RegionContext = RegionHandle;
	return AE_OK;
}

void fwts_acpi_region_handler_called_set(const bool val)
{
	region_handler_called = val;
}

bool fwts_acpi_region_handler_called_get(void)
{
	return region_handler_called;
}

static ACPI_STATUS fwts_region_handler(
	UINT32                  function,
	ACPI_PHYSICAL_ADDRESS   address,
	UINT32                  bitwidth,
	UINT64                  *value,
	void                    *handlercontext,
	void                    *regioncontext)
{
	ACPI_OPERAND_OBJECT     *regionobject = ACPI_CAST_PTR(ACPI_OPERAND_OBJECT, regioncontext);
	UINT8                   *buffer = ACPI_CAST_PTR(UINT8, value);
	ACPI_SIZE               length;
	UINT32                  bytewidth;
	ACPI_CONNECTION_INFO    *context;
	int			i;

	if (!regionobject)
		return AE_BAD_PARAMETER;
	if (!value)
		return AE_BAD_PARAMETER;
	if (regionobject->Region.Type != ACPI_TYPE_REGION)
		return AE_OK;

	fwts_acpi_region_handler_called_set(true);

	context = ACPI_CAST_PTR (ACPI_CONNECTION_INFO, handlercontext);

	switch (regionobject->Region.SpaceId) {
	case ACPI_ADR_SPACE_SYSTEM_IO:
		switch (function & ACPI_IO_MASK) {
		case ACPI_READ:
			*value = 0;
			break;
		case ACPI_WRITE:
			break;
		default:
			return AE_BAD_PARAMETER;
			break;
		}
		break;

	case ACPI_ADR_SPACE_SMBUS:
	case ACPI_ADR_SPACE_GSBUS:  /* ACPI 5.0 */
		length = 0;

		switch (function & ACPI_IO_MASK) {
		case ACPI_READ:
			switch (function >> 16) {
			case AML_FIELD_ATTRIB_QUICK:
			case AML_FIELD_ATTRIB_SEND_RECEIVE:
			case AML_FIELD_ATTRIB_BYTE:
				length = 1;
				break;
			case AML_FIELD_ATTRIB_WORD:
			case AML_FIELD_ATTRIB_PROCESS_CALL:
				length = 2;
				break;
			case AML_FIELD_ATTRIB_BLOCK:
			case AML_FIELD_ATTRIB_BLOCK_PROCESS_CALL:
				length = 32;
				break;
			case AML_FIELD_ATTRIB_BYTES:
			case AML_FIELD_ATTRIB_RAW_BYTES:
			case AML_FIELD_ATTRIB_RAW_PROCESS_BYTES:
				break;
			default:
				break;
			}
			break;

		case ACPI_WRITE:
			switch (function >> 16) {
			case AML_FIELD_ATTRIB_QUICK:
			case AML_FIELD_ATTRIB_SEND_RECEIVE:
			case AML_FIELD_ATTRIB_BYTE:
			case AML_FIELD_ATTRIB_WORD:
			case AML_FIELD_ATTRIB_BLOCK:
				length = 0;
				break;
			case AML_FIELD_ATTRIB_PROCESS_CALL:
				length = 2;
				break;
			case AML_FIELD_ATTRIB_BLOCK_PROCESS_CALL:
				length = 32;
				break;
			case AML_FIELD_ATTRIB_BYTES:
			case AML_FIELD_ATTRIB_RAW_BYTES:
			case AML_FIELD_ATTRIB_RAW_PROCESS_BYTES:
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		for (i = 0; i < length; i++)
			buffer[i+2] = (UINT8)(0xA0 + i);
		buffer[0] = 0x7A;
		buffer[1] = (UINT8)length;
		return AE_OK;

	case ACPI_ADR_SPACE_IPMI: /* ACPI 4.0 */
		buffer[0] = 0;       /* Status byte */
		buffer[1] = 64;      /* Return buffer data length */
		buffer[2] = 0;       /* Completion code */
		buffer[3] = 0;       /* Reserved */
		for (i = 4; i < 66; i++)
			buffer[i] = (UINT8) (i);
		return AE_OK;

	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
	case ACPI_ADR_SPACE_EC:
	case ACPI_ADR_SPACE_CMOS:
	case ACPI_ADR_SPACE_GPIO:
	case ACPI_ADR_SPACE_PCI_BAR_TARGET:
	case ACPI_ADR_SPACE_FIXED_HARDWARE:
	case ACPI_ADR_SPACE_USER_DEFINED1:
	case ACPI_ADR_SPACE_USER_DEFINED2:
	default:
		bytewidth = (bitwidth / 8);
		if (bitwidth % 8)
			bytewidth += 1;

		switch (function) {
		case ACPI_READ:
			/* Fake it, return all set */
			memset(value, 0x00, bytewidth);
		break;
		case ACPI_WRITE:
			/* Fake it, do nothing */
			break;
		default:
			return AE_BAD_PARAMETER;
		}
		return AE_OK;
	}

	return AE_OK;
}

void
MpSaveGpioInfo (
	ACPI_PARSE_OBJECT	*Op,
	AML_RESOURCE		*Resource,
	UINT32			PinCount,
	UINT16			*PinList,
	char			*DeviceName)
{
	FWTS_UNUSED(Op);
	FWTS_UNUSED(Resource);
	FWTS_UNUSED(PinCount);
	FWTS_UNUSED(PinList);
	FWTS_UNUSED(DeviceName);
}

void
MpSaveSerialInfo (
	ACPI_PARSE_OBJECT	*Op,
	AML_RESOURCE		*Resource,
	char			*DeviceName)
{
	FWTS_UNUSED(Op);
	FWTS_UNUSED(Resource);
	FWTS_UNUSED(DeviceName);
}


/*
 *  AcpiOsGetRootPointer()
 *	override ACPICA AeLocalGetRootPointer to return a local copy of the RSDP
 */
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
	return (ACPI_PHYSICAL_ADDRESS)(uintptr_t)fwts_acpica_RSDP;
}

/*
 *  fwts_acpica_vprintf()
 *	accumulate prints from ACPICA engine, emit them when we hit terminal '\n'
 */
void fwts_acpica_vprintf(const char *fmt, va_list args)
{
	static char *buffer;
	static size_t buffer_len;

	char *tmp;
	size_t tmp_len;

	if (fwts_acpica_fw == NULL)
		return;

	/* Only emit messages if in ACPICA debug mode */
	if (!(fwts_acpica_fw->flags & FWTS_FLAG_ACPICA_DEBUG))
		return;

	if (vasprintf(&tmp, fmt, args) < 0) {
		fwts_log_info(fwts_acpica_fw, "Out of memory allocating ACPICA printf buffer.");
		return;
	}

	tmp_len = strlen(tmp);

	if (buffer_len == 0) {
		buffer_len = tmp_len + 1;
		buffer = malloc(buffer_len);
		if (buffer)
			strcpy(buffer, tmp);
		else {
			buffer = NULL;
			buffer_len = 0;
		}
	} else {
		char *new_buf;

		buffer_len += tmp_len;
		new_buf = realloc(buffer, buffer_len);
		if (new_buf) {
			buffer = new_buf;
			strcat(buffer, tmp);
		} else {
			free(buffer);
			buffer = NULL;
			buffer_len = 0;
		}
	}

	if (buffer && index(buffer, '\n') != NULL) {
		fwts_log_info(fwts_acpica_fw, "%s", buffer);
		free(buffer);
		buffer_len = 0;
	}

	free(tmp);
}

/*
 *  AcpiOsPrintf()
 *	Override ACPICA AcpiOsPrintf to write printfs to the fwts log
 *	rather than to stdout
 */
void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf (const char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	AcpiOsVprintf(fmt, args);
	va_end(args);
}

/*
 *  AcpiOsVprintf()
 *	Override ACPICA AcpiOsVprintf to write printfs to the fwts log
 *	rather than to stdout
 */
void AcpiOsVprintf(const char *fmt, va_list args)
{
	fwts_acpica_vprintf(fmt, args);
}

/*
 *  AcpiOsCreateSemaphore()
 *	Override ACPICA AcpiOsCreateSemaphore
 */
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits,
	UINT32 InitialUnits,
	ACPI_HANDLE *OutHandle)
{
	sem_info *sem = NULL;
	ACPI_STATUS ret = AE_OK;
	int i;

	if (!OutHandle)
		return AE_BAD_PARAMETER;

	pthread_mutex_lock(&mutex_lock_sem_table);
	for (i = 0; i < MAX_SEMAPHORES; i++) {
		if (!sem_table[i].used) {
			sem = &sem_table[i];
			break;
		}
	}

	if (!sem) {
		pthread_mutex_unlock(&mutex_lock_sem_table);
		return AE_NO_MEMORY;
	}

	sem->used = true;
	sem->count = 0;

	if (sem_init(&sem->sem, 0, InitialUnits) == -1) {
		*OutHandle = NULL;
		ret = AE_NO_MEMORY;
	} else {
		*OutHandle = (ACPI_HANDLE)sem;
	}
	pthread_mutex_unlock(&mutex_lock_sem_table);

	return ret;
}

/*
 *  AcpiOsDeleteSemaphore()
 *	Override ACPICA AcpiOsDeleteSemaphore to fix semaphore memory freeing
 */
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_HANDLE Handle)
{
	sem_info *sem = (sem_info *)Handle;
	ACPI_STATUS ret = AE_OK;

	if (!sem)
		return AE_BAD_PARAMETER;

	pthread_mutex_lock(&mutex_lock_sem_table);
	if (sem_destroy(&sem->sem) == -1)
		ret = AE_BAD_PARAMETER;
	sem->used = false;

	pthread_mutex_unlock(&mutex_lock_sem_table);

	return ret;
}

static void sem_alarm(int dummy)
{
	/* Do nothing apart from interrupt sem_wait() */
}

/*
 *  AcpiOsWaitSemaphore()
 *	Override ACPICA AcpiOsWaitSemaphore to keep track of semaphore acquires
 *	so that we can see if any methods are sloppy in their releases.
 */
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_HANDLE handle, UINT32 Units, UINT16 Timeout)
{
	sem_info *sem = (sem_info *)handle;
	struct timespec	tm;
	struct sigaction sa;

	if (!handle)
		return AE_BAD_PARAMETER;

	switch (Timeout) {
	case 0:
		if (sem_trywait(&sem->sem))
			return AE_TIME;
		break;

	case ACPI_WAIT_FOREVER:
		/*
		 *  The semantics are "wait forever", but
		 *  really we actually detect a lock up
		 *  after a long wait and break out rather
		 *  than waiting for the sun to turn into
		 *  a lump of coal.  This allows us to
		 *  handle controls such as _EJ0 that may
		 *  be waiting forever for a event to signal
		 *  control to continue.  It is evil.
		 */
		sa.sa_handler = sem_alarm;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGALRM, &sa, NULL);
		alarm(MAX_WAIT_TIMEOUT);

		if (sem_wait(&sem->sem)) {
			alarm(0);
			if (errno == EINTR)
				fwts_log_info(fwts_acpica_fw,
					"AML was blocked waiting for "
					"an external event, fwts detected "
					"this and forced a timeout after "
					"%d seconds on a Wait() that had "
					"an indefinite timeout.",
					MAX_WAIT_TIMEOUT);
			return AE_TIME;
		}
		alarm(0);
		break;
	default:
		tm.tv_sec = Timeout / 1000;
		tm.tv_nsec = (Timeout - (tm.tv_sec * 1000)) * 1000000;

		if (sem_timedwait(&sem->sem, &tm))
			return AE_TIME;
		break;
	}

	pthread_mutex_lock(&mutex_lock_sem_table);
	sem->count++;
	pthread_mutex_unlock(&mutex_lock_sem_table);

	return AE_OK;
}

/*
 *  AcpiOsSignalSemaphore()
 *	Override ACPICA AcpiOsSignalSemaphore to keep track of semaphore releases
 *	so that we can see if any methods are sloppy in their releases.
 */
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_HANDLE handle, UINT32 Units)
{
	sem_info *sem = (sem_info *)handle;

	if (!handle)
		return AE_BAD_PARAMETER;

	if (sem_post(&sem->sem) < 0)
		return AE_LIMIT;

	pthread_mutex_lock(&mutex_lock_sem_table);
	sem->count--;
	pthread_mutex_unlock(&mutex_lock_sem_table);

	return AE_OK;
}

/*
 *  AcpiOsReadPort()
 *	Override ACPICA AcpiOsReadPort to fake port reads
 */
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS addr, UINT32 *value, UINT32 width)
{
	switch (width) {
	case 8:
		*value = 0xFF;
		break;
	case 16:
		*value = 0xFFFF;
		break;
	case 32:
		*value = 0xFFFFFFFF;
		break;
	default:
		return AE_BAD_PARAMETER;
	}
	return AE_OK;
}

typedef struct {
	pthread_callback	func;
	void *			context;
	int			thread_index;
} fwts_func_wrapper_context;

/*
 *  fwts_pthread_func_wrapper()
 *	wrap the AcpiOsExecute function so we can mark the thread
 *	accounting free once the function has completed.
 */
void *fwts_pthread_func_wrapper(fwts_func_wrapper_context *ctx)
{
	void *ret;

	ret = ctx->func(ctx->context);

	pthread_mutex_lock(&mutex_thread_info);
	threads[ctx->thread_index].used = false;
	pthread_mutex_unlock(&mutex_thread_info);

	free(ctx);

	return ret;
}

ACPI_STATUS AcpiOsExecute(
	ACPI_EXECUTE_TYPE       type,
	ACPI_OSD_EXEC_CALLBACK  function,
	void                    *func_context)
{
	int	ret;
	int 	i;

	pthread_mutex_lock(&mutex_thread_info);

	/* Find a free slot to do per-thread join tracking */
	for (i = 0; i < MAX_THREADS; i++) {
		if (!threads[i].used) {
			fwts_func_wrapper_context *ctx;

			/* We need some context to pass through to the thread wrapper */
			if ((ctx = malloc(sizeof(fwts_func_wrapper_context))) == NULL) {
				pthread_mutex_unlock(&mutex_thread_info);
				return AE_NO_MEMORY;
			}

			ctx->func = (pthread_callback)function;
			ctx->context = func_context;
			ctx->thread_index = i;
			threads[i].used = true;

			ret = pthread_create(&threads[i].thread, NULL,
				(pthread_callback)fwts_pthread_func_wrapper, ctx);
			pthread_mutex_unlock(&mutex_thread_info);

			if (ret)
				return AE_ERROR;

			return AE_OK;
		}
	}

	/* No free slots, failed! */
	pthread_mutex_unlock(&mutex_thread_info);
	return AE_NO_MEMORY;
}

/*
 *  AcpiOsReadPciConfiguration()
 *	Override ACPICA AcpiOsReadPciConfiguration to fake PCI reads
 */
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *pciid, UINT32 reg, UINT64 *value, UINT32 width)
{
	switch (width) {
	case 8:
		*value = 0x00;
		break;
	case 16:
		*value = 0x0000;
		break;
	case 32:
		*value = 0x00000000;
		break;
	default:
		return AE_BAD_PARAMETER;
	}
	return AE_OK;
}

void AeTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
	if (strncmp(ExistingTable->Signature, ACPI_SIG_DSDT, 4) == 0)
		*NewTable = (ACPI_TABLE_HEADER*)fwts_acpica_DSDT;
}

ACPI_STATUS AcpiOsSignal(UINT32 function, void *info)
{
	switch (function) {
	case ACPI_SIGNAL_BREAKPOINT:
		fwts_warning(fwts_acpica_fw,
			"Method contains an ACPICA breakpoint: %s\n",
			info ? (char*)info : "No Information");
		AcpiGbl_CmSingleStep = FALSE;
		break;
	case ACPI_SIGNAL_FATAL:
	default:
		break;
	}
	return AE_OK;
}

void AcpiOsSleep(UINT64 milliseconds)
{
}

static void fwtsOverrideRegionHandlers(fwts_framework *fw)
{
	int i;
	static const ACPI_ADR_SPACE_TYPE fwts_default_space_id_list[] = {
		ACPI_ADR_SPACE_SYSTEM_MEMORY,
		ACPI_ADR_SPACE_SYSTEM_IO,
		ACPI_ADR_SPACE_PCI_CONFIG,
		ACPI_ADR_SPACE_EC
	};

	for (i = 0; i < ACPI_ARRAY_LENGTH(fwts_default_space_id_list); i++) {
		/* Install handler at the root object */
		ACPI_STATUS status;

		status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
				fwts_default_space_id_list[i], fwts_region_handler,
				fwts_region_init, NULL);

		if ((status != AE_OK) && (status != AE_SAME_HANDLER)) {
			fwts_log_error(fw, "Failed to install an OpRegion handler for %s space(%u)",
				AcpiUtGetRegionName((UINT8)fwts_default_space_id_list[i]),
				fwts_default_space_id_list[i]);
		}
	}
}

static int fwtsInstallEarlyHandlers(fwts_framework *fw)
{
	ACPI_HANDLE	handle;

	if (AcpiInstallInterfaceHandler(fwts_interface_handler) != AE_OK) {
		fwts_log_error(fw, "Failed to install interface handler.");
		return FWTS_ERROR;
	}

	if (AcpiInstallTableHandler(fwts_table_handler, NULL) != AE_OK) {
		fwts_log_error(fw, "Failed to install table handler.");
		return FWTS_ERROR;
	}

	if (AcpiInstallExceptionHandler(fwts_exception_handler) != AE_OK) {
		fwts_log_error(fw, "Failed to install exception handler.");
		return FWTS_ERROR;
	}

	if (AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY, fwts_notify_handler, NULL) != AE_OK) {
		fwts_log_error(fw, "Failed to install system notify handler.");
		return FWTS_ERROR;
	}

	if (AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_DEVICE_NOTIFY, fwts_device_notify_handler, NULL) != AE_OK) {
		fwts_log_error(fw, "Failed to install device notify handler.");
		return FWTS_ERROR;
	}

	if (AcpiGetHandle(NULL, "\\_SB", &handle) == AE_OK) {
		if (AcpiInstallNotifyHandler(handle, ACPI_SYSTEM_NOTIFY, fwts_notify_handler, NULL) != AE_OK) {
			fwts_log_error(fw, "Failed to install notify handler.");
			return FWTS_ERROR;
		}

		if (AcpiRemoveNotifyHandler(handle, ACPI_SYSTEM_NOTIFY, fwts_notify_handler) != AE_OK) {
			fwts_log_error(fw, "Failed to remove notify handler.");
			return FWTS_ERROR;
		}

		if (AcpiInstallNotifyHandler(handle, ACPI_ALL_NOTIFY, fwts_notify_handler, NULL) != AE_OK) {
			fwts_log_error(fw, "Failed to install notify handler.");
			return FWTS_ERROR;
		}

		if (AcpiRemoveNotifyHandler(handle, ACPI_ALL_NOTIFY, fwts_notify_handler) != AE_OK) {
			fwts_log_error(fw, "Failed to remove notify handler.");
			return FWTS_ERROR;
		}

		if (AcpiInstallNotifyHandler(handle, ACPI_ALL_NOTIFY, fwts_notify_handler, NULL) != AE_OK) {
			fwts_log_error(fw, "Failed to install notify handler.");
			return FWTS_ERROR;
		}

		if (AcpiAttachData(handle, fwts_attached_data_handler, handle) != AE_OK) {
			fwts_log_error(fw, "Failed to attach data handler.");
			return FWTS_ERROR;
		}
		if (AcpiDetachData(handle, fwts_attached_data_handler) != AE_OK) {
			fwts_log_error(fw, "Failed to detach data handler.");
			return FWTS_ERROR;
		}

		if (AcpiAttachData(handle, fwts_attached_data_handler, handle) != AE_OK) {
			fwts_log_error(fw, "Failed to attach data handler.");
			return FWTS_ERROR;
		}
	}

	fwtsOverrideRegionHandlers(fw);

	return FWTS_OK;
}

static int fwtsInstallLateHandlers(fwts_framework *fw)
{
	int i;
	ACPI_STATUS status;
	static ACPI_ADR_SPACE_TYPE fwts_space_id_list[] =
	{
		ACPI_ADR_SPACE_SYSTEM_MEMORY,
		ACPI_ADR_SPACE_SYSTEM_IO,
		ACPI_ADR_SPACE_EC,
		ACPI_ADR_SPACE_SMBUS,
		ACPI_ADR_SPACE_CMOS,
		ACPI_ADR_SPACE_GSBUS,
		ACPI_ADR_SPACE_GPIO,
		ACPI_ADR_SPACE_PCI_BAR_TARGET,
		ACPI_ADR_SPACE_IPMI,
		ACPI_ADR_SPACE_FIXED_HARDWARE,
		ACPI_ADR_SPACE_USER_DEFINED1,
		ACPI_ADR_SPACE_USER_DEFINED2
	};

	for (i = 0; i < ACPI_ARRAY_LENGTH(fwts_space_id_list); i++) {
		status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		    fwts_space_id_list[i], fwts_region_handler, fwts_region_init, NULL);
		if ((status != AE_OK) && (status != AE_SAME_HANDLER)) {
			fwts_log_error(fw,
				"Failed to install an OpRegion handler for %s space(%u)",
				AcpiUtGetRegionName((UINT8)fwts_space_id_list[i]),
				fwts_space_id_list[i]);
			return FWTS_ERROR;
		}
	}

	if (!AcpiGbl_ReducedHardware) {
		status = AcpiInstallFixedEventHandler(ACPI_EVENT_GLOBAL, fwts_event_handler, NULL);
		if ((status != AE_OK) && (status != AE_SAME_HANDLER)) {
			fwts_log_error(fw, "Failed to install global event handler.");
			return FWTS_ERROR;
		}
		status = AcpiInstallFixedEventHandler(ACPI_EVENT_RTC, fwts_event_handler, NULL);
		if ((status != AE_OK) && (status != AE_SAME_HANDLER)) {
			fwts_log_error(fw, "Failed to install RTC event handler.");
			return FWTS_ERROR;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_acpica_verify_table_get()
 *	fetch a named table with some sanity checks, helper
 *	function for fwts_acpica_verify_facp_table_pointers().
 *	Note: table is never NULL
 */
static int fwts_acpica_verify_table_get(
	fwts_framework *fw,
	const char *name,
	fwts_acpi_table_info **table)
{
	if (fwts_acpi_find_table(fw, name, 0, table) != FWTS_OK) {
		fwts_log_error(fw, "Failed to find ACPI table '%s'.", name);
		return FWTS_ERROR;
	}
	if (!(*table)) {
		/* The load may have failed to find this table */
		fwts_log_error(fw, "Missing ACPI table '%s' and  "
			"the FACP points to it.", name);
		return FWTS_ERROR;
	}
	if ((*table)->data == NULL) {
		/* This really should not happen */
		fwts_log_error(fw, "ACPI table %s had a NULL address "
			"which is unexpected.", name);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  fwts_acpica_verify_facp_table_pointers()
 *	verify facp points to tables that actually exist
 *	Note: if they don't exist then ACPICA segfaults
 */
static int fwts_acpica_verify_facp_table_pointers(fwts_framework *fw)
{
	fwts_acpi_table_info *table;
	fwts_acpi_table_fadt *fadt;

	if (fwts_acpica_verify_table_get(fw, "FACP", &table) != FWTS_OK)
		return FWTS_ERROR;
	fadt = (fwts_acpi_table_fadt *)table->data;

	if (fadt->dsdt || fadt->x_dsdt) {
		if (fwts_acpica_verify_table_get(fw, "DSDT", &table) != FWTS_OK)
			return FWTS_ERROR;
		if (((uint64_t)fadt->dsdt != table->addr) &&
		    (fadt->x_dsdt != table->addr)) {
			fwts_log_error(fw, "FACP points to non-existent DSDT.");
			return FWTS_ERROR;
		}
	}
	if (fadt->firmware_control || fadt->x_firmware_ctrl) {
		if (fwts_acpica_verify_table_get(fw, "FACS", &table) != FWTS_OK)
			return FWTS_ERROR;
		if (((uint64_t)fadt->firmware_control != table->addr) &&
		    (fadt->x_firmware_ctrl != table->addr)) {
			fwts_log_error(fw, "FACP points to non-existent FACS.");
			return FWTS_ERROR;
		}
	}
	return FWTS_OK;
}

/*
 *  fwts_acpica_set_fwts_framework()
 *	set fwts_acpica_fw ptr
 */
void fwts_acpica_set_fwts_framework(fwts_framework *fw)
{
	fwts_acpica_fw = fw;
}

#define FWTS_ACPICA_MODE(fw, mode)	\
	(((fw->acpica_mode & mode) == mode) ? 1 : 0)

/*
 *  fwts_acpica_init()
 *	Initialise ACPICA core engine
 */
int fwts_acpica_init(fwts_framework *fw)
{
	int i;
	int n;
	UINT32 init_flags = ACPI_FULL_INITIALIZATION;
	fwts_acpi_table_info *table;

	/* Abort if already initialised */
	if (fwts_acpica_init_called)
		return FWTS_ERROR;

	AcpiGbl_AutoSerializeMethods =
		FWTS_ACPICA_MODE(fw, FWTS_ACPICA_MODE_SERIALIZED);
	AcpiGbl_EnableInterpreterSlack =
		FWTS_ACPICA_MODE(fw, FWTS_ACPICA_MODE_SLACK);
	AcpiGbl_IgnoreErrors =
		FWTS_ACPICA_MODE(fw, FWTS_ACPICA_MODE_IGNORE_ERRORS);
	AcpiGbl_DisableAutoRepair =
		FWTS_ACPICA_MODE(fw, FWTS_ACPICA_MODE_DISABLE_AUTO_REPAIR);
	AcpiGbl_CstyleDisassembly = FALSE;

	pthread_mutex_init(&mutex_lock_sem_table, NULL);
	pthread_mutex_init(&mutex_thread_info, NULL);

	fwts_acpica_set_fwts_framework(fw);

	AcpiDbgLevel = ACPI_NORMAL_DEFAULT;
	AcpiDbgLayer = 0x00000000;

	AcpiOsRedirectOutput(stderr);

	/* Clone FADT, make sure it points to a DSDT in user space */
	if (fwts_acpi_find_table(fw, "FACP", 0, &table) != FWTS_OK)
		return FWTS_ERROR;
	if (table != NULL) {
		fwts_acpi_table_info *tbl;

		fwts_acpica_FADT = fwts_low_calloc(1, table->length);
		if (fwts_acpica_FADT == NULL) {
			fwts_log_error(fw, "Out of memory allocating FADT.");
			return FWTS_ERROR;
		}
		memcpy(fwts_acpica_FADT, table->data, table->length);

		if (fwts_acpi_find_table(fw, "DSDT", 0, &tbl) != FWTS_OK)
			return FWTS_ERROR;
		if (tbl) {
			fwts_acpica_DSDT = (void *)tbl->data;

			fwts_acpica_FADT->Dsdt = ACPI_PTR_TO_PHYSADDR(tbl->data);
			if (table->length >= 148)
				fwts_acpica_FADT->XDsdt = ACPI_PTR_TO_PHYSADDR(tbl->data);
		} else {
			fwts_acpica_DSDT = NULL;
			fwts_acpica_FADT->Dsdt = 0;
			fwts_acpica_FADT->XDsdt = 0;
		}

		if (fwts_acpi_find_table(fw, "FACS", 0, &tbl) != FWTS_OK)
			return FWTS_ERROR;
		if (tbl) {
			fwts_acpica_FADT->Facs = ACPI_PTR_TO_PHYSADDR(tbl->data);
			if (table->length >= 140)
				fwts_acpica_FADT->XFacs = ACPI_PTR_TO_PHYSADDR(tbl->data);
		} else {
			fwts_acpica_FADT->Facs = 0;
			fwts_acpica_FADT->XFacs = 0;
		}

		fwts_acpica_FADT->Header.Checksum = 0;
		fwts_acpica_FADT->Header.Checksum = (UINT8)-AcpiUtChecksum ((void*)fwts_acpica_FADT, table->length);
	} else {
		fwts_acpica_FADT = NULL;
	}

	if (fwts_acpica_verify_facp_table_pointers(fw) != FWTS_OK)
		return FWTS_ERROR;

	/* Clone XSDT, make it point to tables in user address space */
	if (fwts_acpi_find_table(fw, "XSDT", 0, &table) != FWTS_OK)
		return FWTS_ERROR;
	if (table != NULL) {
		uint64_t *entries;
		int j;

		fwts_acpica_XSDT = fwts_low_calloc(1, table->length);
		if (fwts_acpica_XSDT == NULL) {
			fwts_log_error(fw, "Out of memory allocating XSDT.");
			return FWTS_ERROR;
		}
		memcpy(fwts_acpica_XSDT, table->data, sizeof(ACPI_TABLE_HEADER));

		n = (table->length - sizeof(ACPI_TABLE_HEADER)) / sizeof(uint64_t);
		entries = (uint64_t*)(table->data + sizeof(ACPI_TABLE_HEADER));
		for (i = 0, j = 0; i < n; i++) {
			fwts_acpi_table_info *tbl;

			if (fwts_acpi_find_table_by_addr(fw, entries[i], &tbl) != FWTS_OK)
				return FWTS_ERROR;
			if (tbl) {
				if (strncmp(tbl->name, "RSDP", 4) == 0)
					continue;
				if (strncmp(tbl->name, "FACP", 4) == 0) {
					fwts_acpica_XSDT->TableOffsetEntry[j++] = ACPI_PTR_TO_PHYSADDR(fwts_acpica_FADT);
				} else {
					fwts_acpica_XSDT->TableOffsetEntry[j++] = ACPI_PTR_TO_PHYSADDR(tbl->data);
				}
			}
		}
		fwts_acpica_XSDT->Header.Checksum = 0;
		fwts_acpica_XSDT->Header.Checksum = (UINT8)-AcpiUtChecksum ((void*)fwts_acpica_XSDT, table->length);
	} else {
		fwts_acpica_XSDT = NULL;
	}

	/* Clone RSDT, make it point to tables in user address space */
	if (fwts_acpi_find_table(fw, "RSDT", 0, &table) != FWTS_OK)
		return FWTS_ERROR;
	if (table) {
		uint32_t *entries;
		int j;

		fwts_acpica_RSDT = fwts_low_calloc(1, table->length);
		if (fwts_acpica_RSDT == NULL) {
			fwts_log_error(fw, "Out of memory allocating RSDT.");
			return FWTS_ERROR;
		}
		memcpy(fwts_acpica_RSDT, table->data, sizeof(ACPI_TABLE_HEADER));

		n = (table->length - sizeof(ACPI_TABLE_HEADER)) / sizeof(uint32_t);
		entries = (uint32_t*)(table->data + sizeof(ACPI_TABLE_HEADER));
		for (i = 0, j = 0; i < n; i++) {
			fwts_acpi_table_info *tbl;
			if (fwts_acpi_find_table_by_addr(fw, entries[i], &tbl) != FWTS_OK)
				return FWTS_ERROR;
			if (tbl) {
				if (strncmp(tbl->name, "RSDP", 4) == 0)
					continue;
				if (strncmp(tbl->name, "FACP", 4) == 0) {
					fwts_acpica_RSDT->TableOffsetEntry[j++] = ACPI_PTR_TO_PHYSADDR(fwts_acpica_FADT);
				} else {
					fwts_acpica_RSDT->TableOffsetEntry[j++] = ACPI_PTR_TO_PHYSADDR(tbl->data);
				}
			}
		}
		fwts_acpica_RSDT->Header.Checksum = 0;
		fwts_acpica_RSDT->Header.Checksum = (UINT8)-AcpiUtChecksum ((void*)fwts_acpica_RSDT, table->length);
	} else {
		fwts_acpica_RSDT = NULL;
	}

	/* Clone RSDP, make it point to tables in user address space */
	if (fwts_acpi_find_table(fw, "RSDP", 0, &table) != FWTS_OK)
		return FWTS_ERROR;
	if (table) {
		fwts_acpica_RSDP = fwts_low_calloc(1, table->length);
		if (fwts_acpica_RSDP == NULL) {
			fwts_log_error(fw, "Out of memory allocating RSDP.");
			return FWTS_ERROR;
		}
		memcpy(fwts_acpica_RSDP, table->data, table->length);

		if (table->length > 20)
			fwts_acpica_RSDP->XsdtPhysicalAddress = ACPI_PTR_TO_PHYSADDR(fwts_acpica_XSDT);
		fwts_acpica_RSDP->RsdtPhysicalAddress = ACPI_PTR_TO_PHYSADDR(fwts_acpica_RSDT);
		fwts_acpica_RSDP->Checksum = (UINT8)-AcpiUtChecksum ((void*)fwts_acpica_RSDP, ACPI_RSDP_CHECKSUM_LENGTH);
	} else {
		fwts_acpica_RSDP = NULL;
	}

	if (ACPI_FAILURE(AcpiInitializeSubsystem())) {
		fwts_log_error(fw, "Failed to initialise ACPICA subsystem.");
		goto failed;
	}
#if 0
	if (ACPI_FAILURE(AcpiInitializeDebugger())) {
		fwts_log_error(fw, "Failed to initialise ACPICA subsystem.");
		goto failed;
	}
#endif
	if (AcpiInitializeTables(Tables, ACPI_MAX_INIT_TABLES, TRUE) != AE_OK) {
		fwts_log_error(fw, "Failed to initialise tables.");
		goto failed;
	}
	if (fwtsInstallEarlyHandlers(fw) != FWTS_OK) {
		fwts_log_error(fw, "Failed to install early handlers.");
		goto failed;
	}
	if (ACPI_FAILURE(AcpiReallocateRootTable())) {
		fwts_log_error(fw, "Failed to reallocate root table.");
		goto failed;
	}

	init_flags = (ACPI_NO_HANDLER_INIT | ACPI_NO_ACPI_ENABLE |
		      ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT);
	AcpiGbl_MaxLoopIterations = 0x10;

	if (ACPI_FAILURE(AcpiEnableSubsystem(init_flags))) {
		fwts_log_error(fw, "Failed to enable ACPI subsystem.");
		goto failed;
	}
	if (ACPI_FAILURE(AcpiLoadTables() != AE_OK)) {
		fwts_log_error(fw, "Failed to load tables.");
		goto failed;
	}

	if (fwtsInstallLateHandlers(fw) != FWTS_OK) {
		fwts_log_error(fw, "Failed to install late handlers.");
		goto failed;
	}
	if (ACPI_FAILURE(AcpiInitializeObjects(init_flags))) {
		fwts_log_error(fw, "Failed to initialize ACPI objects.");
		goto failed;
	}

	fwts_acpica_init_called = true;
	return FWTS_OK;

failed:
	AcpiTerminate();
	return FWTS_ERROR;
}

#define FWTS_ACPICA_FREE(x)	\
	{ fwts_low_free(x); x = NULL; }

/*
 *  fwts_acpica_deinit()
 *	De-initialise ACPICA core engine
 */
int fwts_acpica_deinit(void)
{
	if (!fwts_acpica_init_called)
		return FWTS_ERROR;

	AcpiTerminate();
	pthread_mutex_destroy(&mutex_lock_sem_table);
	pthread_mutex_destroy(&mutex_thread_info);

	FWTS_ACPICA_FREE(fwts_acpica_XSDT);
	FWTS_ACPICA_FREE(fwts_acpica_RSDT);
	FWTS_ACPICA_FREE(fwts_acpica_RSDP);
	FWTS_ACPICA_FREE(fwts_acpica_FADT);

	fwts_acpica_init_called = false;

	return FWTS_OK;
}

/*
 *  fwts_acpi_walk_for_object_names()
 *  	append to list (passed in context) objects names that match a given
 * 	type, (callback from fwts_acpica_get_object_names())
 *
 */
static ACPI_STATUS fwts_acpi_walk_for_object_names(
	ACPI_HANDLE	objHandle,
	UINT32		nestingLevel,
	void		*context,
	void		**ret)
{
	fwts_list *list = (fwts_list*)context;

	ACPI_BUFFER	buffer;
	char tmpbuf[1024];

	buffer.Pointer = tmpbuf;
	buffer.Length  = sizeof(tmpbuf);

	if (!ACPI_FAILURE(AcpiNsHandleToPathname(objHandle, &buffer, FALSE)))
		fwts_list_append(list, strdup((char *)buffer.Pointer));

	return AE_OK;
}

/*
 *  fwts_acpica_get_object_names()
 *	fetch a list of object names that match a specified type
 */
fwts_list *fwts_acpica_get_object_names(const int type)
{
	fwts_list *list;

	if ((list = fwts_list_new()) != NULL)
		AcpiWalkNamespace(type, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
			fwts_acpi_walk_for_object_names, NULL, list, NULL);

	return list;
}
