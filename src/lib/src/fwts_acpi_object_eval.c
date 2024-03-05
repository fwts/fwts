/*
 * Copyright (C) 2010-2024 Canonical
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

/* acpica headers */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "acpi.h"
#pragma GCC diagnostic error "-Wunused-parameter"
#include "fwts_acpi_object_eval.h"

typedef struct {
	const ACPI_STATUS status;
	const fwts_log_level level;
	const char *error_type;
	const char *error_text;
	const char *advice;
} acpi_eval_error;

static const acpi_eval_error errors[] = {
	/* ACPI_STATUS			fwts_log_level		error_type		error_text */
	{ AE_ERROR,			LOG_LEVEL_HIGH,		"AEError",		"Unspecified error",
		"An internal unspecified ACPICA execution error has occurred."
	},
	{ AE_NO_ACPI_TABLES,		LOG_LEVEL_HIGH,		"AENoACPITables",	"No ACPI tables",
		"No ACPI tables could be found.  Normally this indicates the tables could not be loaded "
		"from firmware or a provided firmware dump file."
	},
	{ AE_NO_NAMESPACE,		LOG_LEVEL_HIGH,		"AENoNamespace",	"No namespace",
		"An ACPI namespace was not loaded. This happens if an ACPI root node could not be found."
	},
	{ AE_NO_MEMORY,			LOG_LEVEL_CRITICAL,	"AENoMemory",		"Out of memory",
		"No more memory could be allocated from the heap."
	},
	{ AE_NOT_FOUND,			LOG_LEVEL_CRITICAL,	"AENotFound",		"Not found",
		"An ACPI object or entity was requested but could not be found. Running fwts with the "
		"--acpica=slack option may work around this issue."
	},
	{ AE_NOT_EXIST,			LOG_LEVEL_CRITICAL,	"AENotExist",		"Not exist",
		"An ACPI object or entity was requested but does not exist.",
	},
	{ AE_ALREADY_EXISTS,		LOG_LEVEL_HIGH,		"AEAlreadyExists",	"Already exists",
		"An ACPI object or entity already exists.  Sometimes this occurs when code is not "
		"serialized and it has been created a second time by another thread. ACPICA can detect "
		"these errors and enable auto serialization to try to work around this error. "
	},
	{ AE_TYPE,			LOG_LEVEL_CRITICAL,	"AETtype",		"Type",
		"The type of an object is incorrect and does not match the expected type. ACPI will "
		"generally abort execution of the AML op-code that causes this error."
	},
	{ AE_NULL_OBJECT,		LOG_LEVEL_CRITICAL,	"AENullObject",		"Null object",
		"An ACPI object is NULL when it was expected to be non-NULL."
	},
	{ AE_NULL_ENTRY,		LOG_LEVEL_CRITICAL,	"AENullEntry",		"Null entry",
		"The requested ACPI object did not exist.  This can occur when a root entry is "
		"NULL."
	},
	{ AE_BUFFER_OVERFLOW,		LOG_LEVEL_CRITICAL,	"AEBufferOverflow",	"Buffer overflow",
		"An ACPI buffer that was provided was too small to contain the data to be copied "
		"into it. This generally indicates an AML bug."
	},
	{ AE_STACK_OVERFLOW,		LOG_LEVEL_CRITICAL,	"AEStackOverflow",	"Stack overflow",
		"An ACPI method call failed because the call nesting was too deep and the "
		"internal APCI execution stack overflowed. This generally occurs with AML that "
		"may be recursing too deeply."
	},
	{ AE_STACK_UNDERFLOW,		LOG_LEVEL_CRITICAL,	"AEStackUnderflow",	"Stack underflow",
		"An ACPI method call failed because the internal ACPI execution stack underflowed. "
		"This occurred while walking the object stack and popping off an object from an empty stack."
	},
	{ AE_NOT_IMPLEMENTED,		LOG_LEVEL_HIGH,		"AENotImplemented",	"Not implemented",
		"The ACPICA execution engine has not implemented a feature."
	},
	{ AE_SUPPORT,			LOG_LEVEL_HIGH,		"AESupport",		"Support",
		"Normally this occurs when AML attempts to perform an action that is not supported, "
		"such as converting objects in an unsupported way or attempting to access memory using "
		"an unsupported address space type."
	},
	{ AE_LIMIT,			LOG_LEVEL_CRITICAL,	"AELimit",		"Limit",
		"A predefined limit was reached, for example, attempting to access an I/O port above the "
		"allowed 64K limit or a semaphore handle is out of range."
	},
	{ AE_TIME,			LOG_LEVEL_HIGH,		"AETime",		"Timeout",
		"A timeout has been reached, for example a semaphore has timed out."
	},
	{ AE_ACQUIRE_DEADLOCK,		LOG_LEVEL_CRITICAL,	"AEAcqDeadlock",	"Acquire deadlock",
		"A mutex acquire was aborted because a potential deadlock was detected."
	},
	{ AE_RELEASE_DEADLOCK,		LOG_LEVEL_CRITICAL,	"AERelDeadlock",	"Release deadlock",
		"A mutex release was aborted because a potential deadlock was detected."
	},
	{ AE_NOT_ACQUIRED,		LOG_LEVEL_CRITICAL,	"AENotAcq",		"Not acquired",
		"An attempt to release a mutex or an ACPI global lock failed because it "
		"had not previously acquired. This normally indicates an AML programming bug."
	},
	{ AE_ALREADY_ACQUIRED,		LOG_LEVEL_CRITICAL,	"AEAlreadyAcq",		"Already acquired",
		"An attempt to re-acquire mutex or an ACPI global lock failed because it "
		"had not previously acquired. This normally indicates an AML programming bug."
	},
	{ AE_NO_HARDWARE_RESPONSE,	LOG_LEVEL_HIGH,		"AENoHWResponse",	"No hardware response",
		"ACPI gave up waiting for hardware to respond. For example, a SMI_CMD failed or "
		"the platform does not have an ACPI global lock defined or an ACPI event could "
		"not be enabled."
	},
	{ AE_NO_GLOBAL_LOCK,		LOG_LEVEL_HIGH,		"AENoGlobalLock",	"No global lock",
		"The platform does not define an ACPI global lock in the FACS."
	},
	{ AE_ABORT_METHOD,		LOG_LEVEL_CRITICAL,	"AEAbortMethod",	"Abort method",
		"Execution of an ACPI method was aborted because the internal ACPICA AcpiGbl_AbortMethod "
		"flag was set to true.  This should not occur."
	},
	{ AE_SAME_HANDLER,		LOG_LEVEL_HIGH,		"AESameHandler",	"Same handler",
		"An attempt was made to install the same handler that is already installed."
	},
	{ AE_NO_HANDLER,		LOG_LEVEL_CRITICAL,	"AENoHandler",		"No handler",
		"A handler for a General Purpose Event operation was not installed."
	},
	{ AE_OWNER_ID_LIMIT,		LOG_LEVEL_HIGH,		"AEOwnerIDLimit",	"Owner ID limit",
		"A maximum of 255 OwnerIds have been used. There are no more Owner IDs available for "
		"ACPI tables or control methods."
	},

	{ AE_BAD_PARAMETER,		LOG_LEVEL_HIGH,		"AEBadParam",		"Args: Bad parameter",
		"A parameter passed is out of range or invalid."
	},
	{ AE_BAD_CHARACTER,		LOG_LEVEL_HIGH,		"AEBadChar",		"Args: Bad character",
		"An invalid character was found in an ACPI name."
	},
	{ AE_BAD_PATHNAME,		LOG_LEVEL_HIGH,		"AEBadPathname",	"Args: Bad pathname",
		"An internal AML namestring could not be built because of an invalid pathname."
	},
	{ AE_BAD_DATA,			LOG_LEVEL_HIGH,		"AEBadData",		"Args: Bad data",
		"A buffer or package contained some incorrect data."
	},
	{ AE_BAD_HEX_CONSTANT,		LOG_LEVEL_HIGH,		"AEBadHexConst",	"Args: Bad hex constant",
		"An invalid character was found in a hexadecimal constant."
	},
	{ AE_BAD_OCTAL_CONSTANT,	LOG_LEVEL_HIGH,		"AEBadOctConst",	"Args: Bad octal constant",
		"An invalid character was found in an octal constant."
	},
	{ AE_BAD_DECIMAL_CONSTANT,	LOG_LEVEL_HIGH,		"AEBadDecConst",	"Args: Bad decimal constant",
		"An invalid character was found in a decimal constant."
	},
	{ AE_MISSING_ARGUMENTS,		LOG_LEVEL_CRITICAL,	"AEMissingArgs",	"Args: Missing arguments",
		"Too few arguments were passed into an ACPI control method."
	},
	{ AE_BAD_ADDRESS,		LOG_LEVEL_CRITICAL,	"AEBadAddr",		"Args: Bad address",
		"An illegal null I/O address was referenced by the AML."
	},

	{ AE_AML_BAD_OPCODE, 		LOG_LEVEL_CRITICAL,	"AEAMLBadOpCode",	"Bad AML opcode",
		"An invalid AML opcode encountered during AML execution."
	},
	{ AE_AML_NO_OPERAND, 		LOG_LEVEL_HIGH,		"AEAMLNoOperand",	"Missing operand",
		"An AML opcode was missing a required operand."
	},
	{ AE_AML_OPERAND_TYPE, 		LOG_LEVEL_HIGH,		"AEAMLOperandType",	"Incorrect operand type",
		"An AML opcode had an operand of an incorrect type."
	},
	{ AE_AML_OPERAND_VALUE, 	LOG_LEVEL_HIGH,		"AEAMLOperandValue",	"Incorrect operand value",
		"An AML opcode has an operand of an inappropriate or invalid value.",
	},
	{ AE_AML_UNINITIALIZED_LOCAL, 	LOG_LEVEL_HIGH,		"AEAMLUninitLocal",	"Uninitialized local variable",
		"A method attempted to use a local variable that was not initialized."
	},
	{ AE_AML_UNINITIALIZED_ARG, 	LOG_LEVEL_HIGH,		"AEAMLUninitArg",	"Uninitialized argument",
		"A method attempted to use an uninitialized argument."
	},
	{ AE_AML_UNINITIALIZED_ELEMENT, LOG_LEVEL_HIGH,		"AEAMLUninitElement",	"Uninitialized element",
		"A method attempted to use an empty element in an ACPI package."
	},
	{ AE_AML_NUMERIC_OVERFLOW, 	LOG_LEVEL_HIGH,		"AEAMLNumericOverflow",	"Numeric overflow",
		"A numeric overflow occurred, for example when converting a BCD value."
	},
	{ AE_AML_REGION_LIMIT, 		LOG_LEVEL_CRITICAL,	"AEAMLRegionLimit",	"Region limit",
		"An attempt to access beyond the end of an ACPI Operation Region occurred."
	},
	{ AE_AML_BUFFER_LIMIT, 		LOG_LEVEL_CRITICAL,	"AEAMLBufferLimit",	"Buffer limit",
		"An attempt to access beyond the end of an ACPI buffer occurred."
	},
	{ AE_AML_PACKAGE_LIMIT, 	LOG_LEVEL_CRITICAL,	"AEAMLPackgeLimit",	"Package limit",
		"An attempt to access beyond the endof an ACPI package occurred."
	},
	{ AE_AML_DIVIDE_BY_ZERO, 	LOG_LEVEL_CRITICAL,	"AEAMLDivideByZero",	"Division by zero",
		"A division by zero error occurred while executing the AML Divide op-code"
	},
	{ AE_AML_NAME_NOT_FOUND, 	LOG_LEVEL_HIGH,		"AEAMLNameNotFound",	"Name not found",
		"An ACPI named reference could not be resolved."
	},
	{ AE_AML_INTERNAL, 		LOG_LEVEL_HIGH,		"AEAMLInternal",	"Internal ACPICA execution engine error",
		"An internal error occurred in the ACPICA execution interpreter.  A bug report should "
		"be filed against fwts so that this can be resolved."
	},
	{ AE_AML_INVALID_SPACE_ID, 	LOG_LEVEL_HIGH,		"AEAMLInvalidSpaceID",	"Invalid space ID error",
		"An invalid ACPI Operation Region Space ID has been encountered."
	},
	{ AE_AML_STRING_LIMIT, 		LOG_LEVEL_HIGH,		"AEAMLStringLimit",	"String limit",
		"A string more than 200 characters long has been used."
	},
	{ AE_AML_NO_RETURN_VALUE, 	LOG_LEVEL_HIGH,		"AEAMLNoReturnValue",	"No return value",
		"A method did not return the required value that was expected."
	},
	{ AE_AML_METHOD_LIMIT, 		LOG_LEVEL_HIGH,		"AEAMLMethodLimit",	"Method limit",
		"A control method reached a limit of 255 concurrent thread executions and hit the ACPICA "
		"re-entrancy limit."
	},
	{ AE_AML_NOT_OWNER, 		LOG_LEVEL_HIGH,		"AEAMLNotOwner",	"Not owner",
		"A thread attempted to release a mutex that it did not own.",
	},
	{ AE_AML_MUTEX_ORDER, 		LOG_LEVEL_HIGH,		"AEAMLMutexOrder",	"Mutex order",
		"Deadlock prevention has detected that the current mutex sync level is too large."
	},
	{ AE_AML_MUTEX_NOT_ACQUIRED, 	LOG_LEVEL_HIGH,		"AEAMLMutexNotAcq",	"Mutex not acquired",
		"An attempt was made to release a mutex that had not been acquired."
	},
	{ AE_AML_INVALID_RESOURCE_TYPE, LOG_LEVEL_HIGH,		"AEAMLInvResourceType", "Invalid resource type",
		"An ACPI resource list contained an invalid resource type."
	},
	{ AE_AML_INVALID_INDEX, 	LOG_LEVEL_HIGH,		"AEAMLInvIndex",	"Invalid index",
		"An attempt to get a node with an index that was too large occurred, for example an ACPI "
		"ArgN or LocalN were N was out of range."
	},
	{ AE_AML_REGISTER_LIMIT, 	LOG_LEVEL_HIGH,		"AEAMLRegisterLimit",	"Register limit",
		"An attempt to use a bank value that is beyond the capacity of a register occurred."
	},
	{ AE_AML_NO_WHILE, 		LOG_LEVEL_CRITICAL,	"AEAMLNoWhile",		"No while",
		"A Break or Continue op-code was reached without a matching While op-code."
	},
	{ AE_AML_ALIGNMENT, 		LOG_LEVEL_CRITICAL,	"AEAMLAlignment",	"Misaligmnent",
		"A non-aligned memory transfer was attempted on a machine that does not "
		"support it."
	},
	{ AE_AML_NO_RESOURCE_END_TAG, 	LOG_LEVEL_HIGH,		"AEAMLNoResEndTag",	"No resource end tag",
		"An ACPI resource list had a missing End Tag."
	},
	{ AE_AML_BAD_RESOURCE_VALUE, 	LOG_LEVEL_HIGH,		"AEAMLBadResValue",	"Bad resource value",
		"An ACPI resource element had an invalid value."
	},
	{ AE_AML_CIRCULAR_REFERENCE,	LOG_LEVEL_CRITICAL,	"AEAMLCircularRef",	"Circular reference",
		"Two references refer to each other, which is not allowed."
	},
	{ AE_AML_BAD_RESOURCE_LENGTH,	LOG_LEVEL_HIGH,		"AEAMLBadResLength",	"Bad resource length",
		"The length of an ACPI Resource Descriptor was incorrect."
	},
	{ AE_AML_ILLEGAL_ADDRESS, 	LOG_LEVEL_CRITICAL,	"AEAMLIllegalAddr",	"Illegal address",
		"A memory, PCI configuration or I/O address was encountered with an illegal address."
	},
	/* { AE_AML_INFINITE_LOOP, 	LOG_LEVEL_HIGH,		"AEAMLInfiniteLoop",	"Infinite loop", NULL },
	{ AE_AML_INFINITE_LOOP, 	LOG_LEVEL_HIGH,		"AEAMLInfiniteLoop",	"Infinite loop",
		"An AML While loop exceeded the maximum execution time."
	}, */
	{ AE_AML_UNINITIALIZED_NODE, 	LOG_LEVEL_HIGH,		"AEAMLUninitNode",	"Uninitialized node",
		"A namespace node is uninitialized or unresolved."
	},
	{ AE_AML_TARGET_TYPE,		LOG_LEVEL_HIGH,		"AEAMLTargetType",	"Target type",
		"A target operand of an incorrect type was encountered."
	},
	{ AE_AML_PROTOCOL,		LOG_LEVEL_HIGH,		"AEAMLViolateProtocol",	"Violation protocol",
		"Violation of a fixed ACPI protocol."
	},
	{ AE_AML_BUFFER_LENGTH, 	LOG_LEVEL_HIGH,		"AEAMLBadBufLength",	"Bad buffer length",
		"The length of the buffer is invalid/incorrect."
	},
	{ 0,				0,			NULL,			NULL , 		NULL}
};

static fwts_list *fwts_object_names;
static bool fwts_acpi_initialized = false;

/*
 *  fwts_acpi_init()
 *	Initialise ACPIA engine and collect method namespace
 */
int fwts_acpi_init(fwts_framework *fw)
{
	if (fwts_acpica_init(fw) != FWTS_OK)
		return FWTS_ERROR;

	/* Gather all object names */
	fwts_object_names = fwts_acpica_get_object_names(0);
	fwts_acpi_initialized = true;

	return FWTS_OK;
}

/*
 *  fwts_acpi_deinit()
 *	Close ACPIA engine and free method namespace
 */
int fwts_acpi_deinit(fwts_framework *fw)
{
	int ret = FWTS_ERROR;

	FWTS_UNUSED(fw);

	if (fwts_acpi_initialized) {
		fwts_list_free(fwts_object_names, free);
		fwts_object_names = NULL;
		ret = fwts_acpica_deinit();

		fwts_acpi_initialized = false;
	}

	return ret;
}

/*
 *  fwts_acpi_object_get_names()
 *	return list of object names
 */
fwts_list *fwts_acpi_object_get_names(void)
{
	return fwts_object_names;
}

/*
 *  fwts_acpi_object_exists()
 *	return first matching name
 */
char *fwts_acpi_object_exists(const char *name)
{
	size_t name_len = strlen(name);
	fwts_list_link	*item;

	fwts_list_foreach(item, fwts_object_names) {
		char *method_name = fwts_list_data(char*, item);
		size_t len = strlen(method_name);

		if (strncmp(name, method_name + len - name_len, name_len) == 0)
			return method_name;
	}
	return NULL;
}


/*
 *   fwts_acpi_object_dump_recursive()
 *	dump out an object, minimal form
 */
static void fwts_acpi_object_dump_recursive(
	fwts_framework *fw,
	const ACPI_OBJECT *obj,
	const int depth,
	const int index)
{
	uint32_t i;
	char index_buf[6];
	ACPI_BUFFER buffer;
	ACPI_STATUS status;
	char full_name[128];

	buffer.Length = sizeof(full_name);
	buffer.Pointer = full_name;

	if (index > -1)
		snprintf(index_buf, sizeof(index_buf), "%2.2d: ", (uint8_t) index);
	else
		index_buf[0] = '\0';

	switch (obj->Type) {
	case ACPI_TYPE_INTEGER:
		fwts_log_info_verbatim(fw, "%*s%sINTEGER: 0x%8.8llx", depth * 2, "",
			index_buf, (unsigned long long)obj->Integer.Value);
		break;
	case ACPI_TYPE_STRING:
		fwts_log_info_verbatim(fw, "%*s%sSTRING:  %s", depth * 2, "",
			index_buf, obj->String.Pointer);
		break;
	case ACPI_TYPE_BUFFER:
		fwts_log_info_verbatim(fw, "%*s%sBUFFER:  (%d bytes)", depth * 2, "",
			index_buf, obj->Buffer.Length);
		break;
	case ACPI_TYPE_PACKAGE:
		fwts_log_info_verbatim(fw, "%*s%sPackage has %d elements:",depth * 2, "",
			index_buf, obj->Package.Count);
		for (i = 0; i < obj->Package.Count; i++) {
			ACPI_OBJECT *element = &obj->Package.Elements[i];
			fwts_acpi_object_dump_recursive(fw, element, depth + 1, i);
		}
		break;
	case ACPI_TYPE_LOCAL_REFERENCE:
		status = AcpiGetName(obj->Reference.Handle, ACPI_FULL_PATHNAME, &buffer);
		if (ACPI_SUCCESS(status))
			fwts_log_info_verbatim(fw, "%*s%sReference:  %s", depth * 2, "",
				index_buf, full_name);
		break;
	default:
		fwts_log_info_verbatim(fw, "%*s%sUnknown type 0x%2.2x\n", depth * 2, "",
			index_buf, obj->Type);
		break;
	}
}

/*
 *   fwts_method_object_dump()
 *	dump out an object, minimal form
 */
void fwts_acpi_object_dump(fwts_framework *fw, const ACPI_OBJECT *obj)
{
	fwts_acpi_object_dump_recursive(fw, obj, 1, -1);
}

/*
 *  fwts_acpi_object_evaluate_report_error()
 *	report any errors found during object evaluation
 */
void fwts_acpi_object_evaluate_report_error(
	fwts_framework *fw,
	const char *name,
	const ACPI_STATUS status)
{
	int i;

	/* Generic cases */
	for (i = 0; errors[i].error_type; i++) {
		if (status == errors[i].status) {
			fwts_failed(fw, errors[i].level, errors[i].error_type,
				"Detected error '%s' when evaluating '%s'.",
				errors[i].error_text, name);
			if (errors[i].advice != NULL)
				fwts_advice(fw, "%s", errors[i].advice);
			return;
		}
	}

	/* Special cases */
	switch (status) {
	case AE_OK:
		break;
	case AE_AML_LOOP_TIMEOUT:
		fwts_warning(fw, "Detected an infinite loop when evaluating method '%s'. ", name);
		fwts_advice(fw, "This may occur because we are emulating the execution "
				"in this test environment and cannot handshake with "
				"the embedded controller or jump to the BIOS via SMIs. "
				"However, the fact that AML code spins forever means that "
				"lockup conditions are not being checked for in the AML bytecode.");
		break;
	/* Unknown?! */
	default:
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "AMLFailedToEvaluate",
			"Failed to evaluate '%s', got error code %d.", name, status);
		break;
	}
}

/*
 *  fwts_acpi_object_evaluate()
 *	evaluate object, return error status (handle this with
 *	fwts_method_evaluate_report_error()).
 * 	This returns buf which auto allocates any return values
 *	which need to be freed post-evalutation using free().
 */
ACPI_STATUS fwts_acpi_object_evaluate(fwts_framework *fw,
	char *name,
        ACPI_OBJECT_LIST *arg_list,
	ACPI_BUFFER	 *buf)
{
	FWTS_UNUSED(fw);

	buf->Length  = ACPI_ALLOCATE_BUFFER;
	buf->Pointer = NULL;

	return AcpiEvaluateObject(NULL, name, arg_list, buf);
}

int fwts_method_check_type__(
	fwts_framework *fw,
	const char *name,
	const ACPI_BUFFER *buf,
	const ACPI_OBJECT_TYPE type,
	const char *type_name)
{
	ACPI_OBJECT *obj;

	if ((buf == NULL) || (buf->Pointer == NULL)) {
		fwts_method_failed_null_object(fw, name, type_name);
		return FWTS_ERROR;
	}

	obj = buf->Pointer;

	if (!fwts_method_type_matches(obj->Type, type)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodReturnBadType",
			"Method %s did not return %s.", name, type_name);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  get_object_name()
 *	get objname from full path nanme
 *		ex. _BCL from _SB.PCI0.GFX0.LCD0._BCL
 */
static void get_object_name(const char *name, char* obj_name) {
	/* obj_name must have length of 5 */
	if (name != NULL && strlen(name) >= 4) {
		memcpy(obj_name, name + strlen(name) - 4, 4);
		obj_name[4] = '\0';
	}
}

/*
 *  fwts_method_check_element_type()
 *	check a element type of a sub-package
 */
int fwts_method_check_element_type(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const uint32_t subpkg,
	const uint32_t element,
	const ACPI_OBJECT_TYPE type)
{
	/* See references in actypes.h */
	static const char *acpi_object_names[] = {
		"Any",
		"Integer",
		"String",
		"Buffer",
		"Package",
		"Field Unit",
		"Device",
		"Event",
		"Method",
		"Mutex",
		"Region",
		"Power",
		"Processor",
		"Thermal",
		"Buffer Field",
		"DDB Handle",
		"Debug Object",
		"Region Field",
		"Bank Field",
		"Index Field",
		"Reference",
		"Alias",
		"Method Alias",
		"Notify",
		"Address Handler",
		"Resource",
		"Resource Field",
		"Scope"
	};

	if (obj->Package.Elements[element].Type != type) {
		char obj_name[5] = "_XYZ";
		char tmp[128];

		get_object_name(name, obj_name);
		snprintf(tmp, sizeof(tmp), "Method%sBadSubPackageReturnType", obj_name);

		if (type > FWTS_ARRAY_SIZE(acpi_object_names) - 1) {
			fwts_warning(fw, "Unknown ACPI object type detected");
			return FWTS_ERROR;
		}

		fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
			"%s sub-package %" PRIu32 " element %" PRIu32 " is a %s, "
			"expected a %s", name, subpkg, element,
			acpi_object_names[obj->Package.Elements[element].Type],
			acpi_object_names[type]);

		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  Common types that can be returned. This is not a complete
 *  list but it does cover the types we expect to return from
 *  an ACPI evaluation.
 */
const char *fwts_method_type_name(const ACPI_OBJECT_TYPE type)
{
	switch (type) {
	case ACPI_TYPE_INTEGER:
		return "integer";
	case ACPI_TYPE_STRING:
		return "string";
	case ACPI_TYPE_BUFFER:
		return "buffer";
	case ACPI_TYPE_PACKAGE:
		return "package";
	case ACPI_TYPE_BUFFER_FIELD:
		return "buffer_field";
	case ACPI_TYPE_LOCAL_REFERENCE:
		return "reference";
	case ACPI_TYPE_INTBUF:
		return "integer or buffer";
	default:
		return "unknown";
	}
}

/*
 *  method_passed_sane()
 *	helper function to report often used passed messages
 */
void fwts_method_passed_sane(
	fwts_framework *fw,
	const char *name,
	const char *type)
{
	fwts_passed(fw, "%s correctly returned a sane looking %s.", name, type);
}

/*
 *  method_passed_sane_uint64()
 *	helper function to report often used passed uint64 values
 */
void fwts_method_passed_sane_uint64(
	fwts_framework *fw,
	const char *name,
	const uint64_t value)
{
	fwts_passed(fw, "%s correctly returned sane looking "
		"value 0x%8.8" PRIx64 ".", name, value);
}

/*
 *  fwts_method_failed_null_object()
 *	helper function to report often used failed NULL object return
 */
void fwts_method_failed_null_object(
	fwts_framework *fw,
	const char *name,
	const char *type)
{
	fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodReturnNullObj",
		"%s returned a NULL object, and did not "
		"return %s.", name, type);
}

bool fwts_method_type_matches(
	const ACPI_OBJECT_TYPE t1,
	const ACPI_OBJECT_TYPE t2)
{
	if (t1 == ACPI_TYPE_INTBUF &&
	    (t2 == ACPI_TYPE_INTEGER || t2 == ACPI_TYPE_BUFFER))
		return true;

	if (t2 == ACPI_TYPE_INTBUF &&
	    (t1 == ACPI_TYPE_INTEGER || t1 == ACPI_TYPE_BUFFER))
		return true;

	return t1 == t2;
}

/*
 *  fwts_method_buffer_size()
 *	check whether buffer size is matched
 */
int fwts_method_buffer_size(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const size_t buf_size)
{
	if (obj->Buffer.Length != buf_size) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"MethodBadBufferSize",
			"%s should return a buffer of %zd bytes, "
			"but instead returned %" PRIu32 " bytes.",
			name, buf_size, obj->Buffer.Length);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  fwts_method_package_count_min()
 *	check that an ACPI package has at least 'min' elements
 */
int fwts_method_package_count_min(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const uint32_t min)
{
	if (obj->Package.Count < min) {
		char objname[5] = "_XYZ";
		char tmp[128];

		get_object_name(name, objname);
		snprintf(tmp, sizeof(tmp), "Method%sElementCount", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return package of at least %" PRIu32
			" element%s, got %" PRIu32 " element%s instead.",
			name, min, min == 1 ? "" : "s",
			obj->Package.Count, obj->Package.Count == 1 ? "" : "s");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  method_package_count_equal()
 *	check that an ACPI package has exactly 'count' elements
 */
int fwts_method_package_count_equal(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const uint32_t count)
{
	if (obj->Package.Count != count) {
		char objname[5] = "_XYZ";
		char tmp[128];

		get_object_name(name, objname);
		snprintf(tmp, sizeof(tmp), "Method%sElementCount", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return package of %" PRIu32
			" element%s, got %" PRIu32 " element%s instead.",
			name, count, count == 1 ? "" : "s",
			obj->Package.Count, obj->Package.Count == 1 ? "" : "s");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  fwts_method_subpackage_count_equal()
 *	check that an ACPI subpackage has exactly 'count' elements
 */
int fwts_method_subpackage_count_equal(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const uint32_t sub,
	const uint32_t count)
{
	if (obj->Package.Count != count) {
		char objname[5] = "_XYZ";
		char tmp[128];

		get_object_name(name, objname);
		snprintf(tmp, sizeof(tmp), "Method%sSubPackageElementCount", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s sub-package %" PRIu32 " was expected to have "
			"%" PRIu32 " element%s, got %" PRIu32 " element%s instead.",
			name, sub, count, count == 1 ? "" : "s",
			obj->Package.Count, obj->Package.Count == 1 ? "" : "s");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

int fwts_method_package_elements_all_type(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const ACPI_OBJECT_TYPE type)
{
	uint32_t i;
	bool failed = false;
	char tmp[128];

	for (i = 0; i < obj->Package.Count; i++) {
		if (!fwts_method_type_matches(obj->Package.Elements[i].Type, type)) {
			char objname[5] = "_XYZ";

			get_object_name(name, objname);
			snprintf(tmp, sizeof(tmp), "Method%sElementType", objname);
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
				"%s package element %" PRIu32 " was not the expected "
				"type '%s', was instead type '%s'.",
				name ? name : "unknown", i,
				fwts_method_type_name(type),
				fwts_method_type_name(obj->Package.Elements[i].Type));
			failed = true;
		}
	}

	return failed ? FWTS_ERROR: FWTS_OK;
}

/*
 *  fwts_method_package_elements_type__()
 *	sanity check fields in a package that all have
 *	the same type
 */
int fwts_method_package_elements_type__(
	fwts_framework *fw,
	const char *name,
	const ACPI_OBJECT *obj,
	const fwts_package_element *info,
	const uint32_t count)
{
	uint32_t i;
	bool failed = false;
	char tmp[128];

	if (fwts_method_package_count_equal(fw, name, obj, count) != FWTS_OK)
		return FWTS_ERROR;

	for (i = 0; i < obj->Package.Count; i++) {
		if (!fwts_method_type_matches(obj->Package.Elements[i].Type, info[i].type)) {
			char objname[5] = "_XYZ";

			get_object_name(name, objname);
			snprintf(tmp, sizeof(tmp), "Method%sElementType", objname);
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
				"%s package element %" PRIu32 " (%s) was not the expected "
				"type '%s', was instead type '%s'.",
				name ? name : "unknown", i, info[i].name,
				fwts_method_type_name(info[i].type),
				fwts_method_type_name(obj->Package.Elements[i].Type));
			failed = true;
		}
	}

	return failed ? FWTS_ERROR: FWTS_OK;
}

int fwts_method_test_revision(
	fwts_framework *fw,
	const char *name,
	const uint32_t cur_revision,
	const uint32_t spec_revision)
{
	char method[5];
	char tag[22];

	if (strlen(name) < 4)
		return FWTS_ERROR;

	memcpy(method, name + strlen(name) - 4, 4);
	method[4] = '\0';
	snprintf(tag, 22, "Method%sBadRevision", method);

	if (cur_revision != spec_revision) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, tag,
			"%s revision is not revision %" PRIu32 ".", name, spec_revision);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

/*
 *  fwts_method_test_integer_return
 *	check if an integer object was returned
 */
void fwts_method_test_integer_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned an integer.", name);
}

/*
 *  fwts_method_test_string_return
 *	check if an string object was returned
 */
void fwts_method_test_string_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_STRING) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned a string.", name);
}

/*
 *  fwts_method_test_reference_return
 *	check if a reference object was returned
 */
void fwts_method_test_reference_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_LOCAL_REFERENCE) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned a reference.", name);
}

/*
 *  fwts_method_test_NULL_return
 *	check if no object was returned
 */
void fwts_method_test_NULL_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	/*
	 *  In ACPICA SLACK mode null returns can be actually
	 *  forced to return ACPI integers. Blame an errata
	 *  and Windows compatibility for this mess.
	 */
	if (fw->acpica_mode & FWTS_ACPICA_MODE_SLACK) {
		if ((buf != NULL) && (buf->Pointer != NULL)) {
			ACPI_OBJECT *objtmp = buf->Pointer;
			if (fwts_method_type_matches(objtmp->Type, ACPI_TYPE_INTEGER)) {
				fwts_passed(fw, "%s returned an ACPI_TYPE_INTEGER as expected in slack mode.",
					name);
				return;
			}
		}
	}

	if (buf && buf->Length && buf->Pointer) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MethodShouldReturnNothing",
			"%s returned values, but was expected to return nothing.", name);
		fwts_log_info(fw, "Object returned:");
		fwts_acpi_object_dump(fw, obj);
		fwts_advice(fw,
			"This probably won't cause any errors, but it should "
			"be fixed as the AML code is not conforming to the "
			"expected behaviour as described in the ACPI "
			"specification.");
	} else
		fwts_passed(fw, "%s returned no values as expected.", name);
}

/*
 *  fwts_method_test_buffer_return
 *	check if a buffer object was returned
 */
void fwts_method_test_buffer_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned a buffer of %" PRIu32 " elements.",
			name, obj->Buffer.Length);
}

/*
 *  fwts_method_test_all_reference_package_return
 *	check if all elements are references in a package
 */
void fwts_method_test_all_reference_package_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_all_type(fw, name, obj, ACPI_TYPE_LOCAL_REFERENCE) != FWTS_OK)
		return;

	fwts_passed(fw,	"%s returned a sane package of %" PRIu32 " references.", name, obj->Package.Count);
}

/*
 *  fwts_method_test_integer_reserved_bits_return
 *	check if reserved bits are zero'ed in a returned integer
 */
void fwts_method_test_integer_reserved_bits_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint64_t *mask = (uint64_t *) private;
	bool failed = false;

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	if (obj->Integer.Value & *mask) {
		failed = true;
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MethodIllegalReserved",
			"%s returned value 0x%8.8" PRIx64 " and some of the "
			"reserved bits are set when they should be zero.",
			name, (uint64_t) obj->Integer.Value);
	}

	if (!failed)
		fwts_passed(fw, "%s correctly returned an integer.", name);
}

/*
 *  fwts_method_test_integer_max_return
 *	check if returned integer is less than a max value
 */
void fwts_method_test_integer_max_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint64_t *max = (uint64_t *) private;
	bool failed = false;

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	if (obj->Integer.Value > *max) {
		failed = true;
		fwts_failed(fw, LOG_LEVEL_HIGH, "MethodOutOfRangeValue",
			"%s returned value %" PRId64 " but range allowed "
			"is 0..%" PRId64 " .",
			name, (uint64_t) obj->Integer.Value, *max);
	}

	if (!failed)
		fwts_passed(fw, "%s correctly returned an integer.", name);
}

/*
 *  fwts_method_test_package_integer_return
 *	check if all integers in a returned package
 */
void fwts_method_test_package_integer_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint32_t *element_size = (uint32_t *) private;

	if (strlen(name) < 4)
		return;

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_count_equal(fw, name, obj, *element_size) != FWTS_OK)
		return;

	if (fwts_method_package_elements_all_type(fw, name, obj, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	fwts_method_passed_sane(fw, name, "package");
}

/*
 *  fwts_method_test_passed_failed_return
 *	check if 0 or 1 (false/true) integer is returned
 */
void fwts_method_test_passed_failed_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	char *method = (char *)private;
	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) == FWTS_OK) {
		uint32_t val = (uint32_t)obj->Integer.Value;
		if ((val == 0) || (val == 1))
			fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
		else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"MethodReturnZeroOrOne",
				"%s returned 0x%8.8" PRIx32 ", should return 0 "
				"or 1.", method, val);
			fwts_advice(fw,
				"Method %s should be returning the correct "
				"0 or 1 return values. "
				"Unexpected behaviour may occur because of "
				"this error, the AML code does not conform to "
				"the ACPI specification and should be fixed.",
				method);
		}
	}
}

/*
 *  fwts_method_test_polling_return
 *	check if a returned polling time is valid
 */
void fwts_method_test_polling_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) == FWTS_OK) {
		char *method = (char *)private;
		if (obj->Integer.Value < 36000) {
			fwts_passed(fw,
				"%s correctly returned sane looking value "
				"%f seconds", method,
				(float)obj->Integer.Value / 10.0);
		} else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"MethodPollTimeTooLong",
				"%s returned a value %f seconds > (1 hour) "
				"which is probably incorrect.",
				method, (float)obj->Integer.Value / 10.0);
			fwts_advice(fw,
				"The method is returning a polling interval "
				"which is very long and hence most probably "
				"incorrect.");
		}
	}
}

void fwts_evaluate_found_method(
	fwts_framework *fw,
	ACPI_HANDLE *parent,
	char *name,
	fwts_method_return check_func,
	void *private,
	ACPI_OBJECT_LIST *arg_list)
{
	ACPI_BUFFER	buf;
	ACPI_STATUS	status;
	int sem_acquired;
	int sem_released;

	fwts_acpica_sem_count_clear();

	buf.Length  = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;
	status = AcpiEvaluateObject(*parent, name, arg_list, &buf);

	if (ACPI_SUCCESS(status) && check_func != NULL) {
		ACPI_OBJECT *obj = buf.Pointer;
		check_func(fw, name, &buf, obj, private);
	}
	free(buf.Pointer);

	fwts_acpica_sem_count_get(&sem_acquired, &sem_released);
	if (sem_acquired != sem_released) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "AMLLocksAcquired",
			"%s left %d locks in an acquired state.",
			name, sem_acquired - sem_released);
		fwts_advice(fw,
			"Locks left in an acquired state generally indicates "
			"that the AML code is not releasing a lock. This can "
			"sometimes occur when a method hits an error "
			"condition and exits prematurely without releasing an "
			"acquired lock. It may be occurring in the method "
			"being tested or other methods used while evaluating "
			"the method.");
	}
}

int fwts_evaluate_method(fwts_framework *fw,
	const uint32_t test_type,  /* Manditory or optional */
	ACPI_HANDLE *parent,
	char *name,
	ACPI_OBJECT *args,
	uint32_t num_args,
	fwts_method_return check_func,
	void *private)
{
	ACPI_OBJECT_LIST	arg_list;
	ACPI_HANDLE	method;
	ACPI_STATUS	status;

	status = AcpiGetHandle (*parent, name, &method);
	if (ACPI_SUCCESS(status)) {
		arg_list.Count   = num_args;
		arg_list.Pointer = args;
		fwts_evaluate_found_method(fw, parent, name, check_func, private, &arg_list);
	}

	if (status == AE_NOT_FOUND && !(test_type & METHOD_SILENT)) {
		if (test_type & METHOD_MANDATORY) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodNotExist", "Object %s did not exist.", name);
			return FWTS_ERROR;
		}

		if (test_type & METHOD_OPTIONAL) {
			fwts_skipped(fw, "Skipping test for non-existent object %s.", name);
			return FWTS_SKIP;
		}
	}

	if (ACPI_FAILURE(status)) {
		const char *exception = AcpiFormatException(status);
		fwts_log_info(fw, "ACPICA Exception %s during lookup of method %s",
			      exception, name);
		return FWTS_ERROR;
	}

	return FWTS_OK;
}

bool fwts_method_valid_HID_string(const char *str)
{
	if (strlen(str) == 7) {
		/* PNP ID, must be 3 capitals followed by 4 hex */
		if (!isupper(str[0]) ||
		    !isupper(str[1]) ||
		    !isupper(str[2])) return false;
		if (!isxdigit(str[3]) ||
		    !isxdigit(str[4]) ||
		    !isxdigit(str[5]) ||
		    !isxdigit(str[6])) return false;
		return true;
	}

	if (strlen(str) == 8) {
		/* ACPI ID, must be 4 capitals or digits followed by 4 hex */
		if ((!isupper(str[0]) && !isdigit(str[0])) ||
		    (!isupper(str[1]) && !isdigit(str[1])) ||
		    (!isupper(str[2]) && !isdigit(str[2])) ||
		    (!isupper(str[3]) && !isdigit(str[3]))) return false;
		if (!isxdigit(str[4]) ||
		    !isxdigit(str[5]) ||
		    !isxdigit(str[6]) ||
		    !isxdigit(str[7])) return false;
		return true;
	}

	return false;
}

bool fwts_method_valid_EISA_ID(
	const uint32_t id,
	char *buf,
	const size_t buf_len)
{
	snprintf(buf, buf_len, "%c%c%c%02" PRIX32 "%02" PRIX32,
		0x40 + ((id >> 2) & 0x1f),
		0x40 + ((id & 0x3) << 3) + ((id >> 13) & 0x7),
		0x40 + ((id >> 8) & 0x1f),
		(id >> 16) & 0xff, (id >> 24) & 0xff);

	/* 3 chars in EISA ID must be upper case */
	if (!isupper(buf[0]) ||
	    !isupper(buf[1]) ||
	    !isupper(buf[2])) return false;

	/* Last 4 digits are always going to be hex, so pass */
	return true;
}

/*
 *  Section 6.2 Device Configurations Objects
 */
static void method_test_CRS_size(
	fwts_framework *fw,
	const char *name,		/* full _CRS or _PRS path name */
	const char *objname,		/* name of _CRS or _PRS object */
	const char *tag,		/* error log tag */
	const size_t crs_length,	/* size of _CRS buffer */
	const size_t hdr_length,	/* size of _CRS header */
	const size_t data_length,	/* length of _CRS data w/o header */
	const size_t min,		/* minimum allowed _CRS data size */
	const size_t max,		/* maximum allowed _CRS data size */
	bool *passed)			/* pass/fail flag */
{
	if (crs_length < data_length + hdr_length) {
		fwts_failed(fw, LOG_LEVEL_HIGH, tag,
			"%s Resource size is %zd bytes long but "
			"the size stated in the %s buffer header  "
			"is %zd and hence is longer. The resource "
			"buffer is too short.",
			name, crs_length, objname, data_length);
		*passed = false;
		return;
	}

	if ((data_length < min) || (data_length > max)) {
		if (min != max) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tag,
				"%s Resource data size was %zd bytes long, "
				"expected it to be between %zd and %zd bytes",
				name, data_length, min, max);
			*passed = false;
		} else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tag,
				"%s Resource data size was %zd bytes long, "
				"expected it to be %zd bytes",
				name, data_length, min);
			*passed = false;
		}
	}
}

static void method_test_CRS_small_size(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const size_t crs_length,
	const size_t min,
	const size_t max,
	bool *passed)
{
	size_t data_length = data[0] & 7;
	char tmp[128];

	snprintf(tmp, sizeof(tmp), "Method%sSmallResourceSize", objname);

	method_test_CRS_size(fw, name, objname, tmp,
		crs_length, 1, data_length, min, max, passed);
}

/*
 *  CRS small resource checks, simple checking
 */
void fwts_method_test_CRS_small_resource_items(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const size_t length,
	bool *passed,
	const char **tag)
{
	uint8_t tag_item = (data[0] >> 3) & 0xf;
	char tmp[128];

	static const char *types[] = {
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"IRQ Descriptor",
		"DMA Descriptor",
		"Start Dependent Functions Descriptor",
		"End Dependent Functions Descriptor",
		"I/O Port Descriptor",
		"Fixed Location I/O Port Descriptor",
		"Fixed DMA Descriptor",
		"Reserved",
		"Reserved",
		"Reserved",
		"Vendor Defined Descriptor",
		"End Tag Descriptor"
	};

	switch (tag_item) {
	case 0x4: /* 6.4.2.1 IRQ Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 2, 3, passed);
		break;
	case 0x5: /* 6.4.2.2 DMA Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 2, 2, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if ((data[2] & 3) == 3) {
			snprintf(tmp, sizeof(tmp), "Method%sDmaDescriptor", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s DMA transfer type preference is 0x%" PRIx8
				" which is reserved and invalid. See "
				"Section 6.4.2.2 of the ACPI specification.",
				name, data[2] & 3);
			*passed = false;
		}
		break;
	case 0x6: /* 6.4.2.3 Start Dependent Functions Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 0, 1, passed);
		break;
	case 0x7: /* 6.4.2.4 End Dependent Functions Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 0, 0, passed);
		break;
	case 0x8: /* 6.4.2.5 I/O Port Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 7, 7, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if (data[1] & 0xfe) {
			snprintf(tmp, sizeof(tmp), "Method%sIoPortInfoReservedNonZero", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s I/O Port Descriptor Information field "
				"has reserved bits that are non-zero, got "
				"0x%" PRIx8 " and expected 0 or 1 for this "
				"field. ", name, data[1]);
			*passed = false;
		}
		if (((data[1] & 1) == 0) && (data[3] > 3)) {
			snprintf(tmp, sizeof(tmp), "Method%sIoPortInfoMinBase10BitAddr", objname);
			fwts_failed(fw, LOG_LEVEL_LOW, tmp,
				"%s I/O Port Descriptor range minimum "
				"base address is more than 10 bits however "
				"the Information field indicates that only "
				"a 10 bit address is being used.", name);
			*passed = false;
		}
		if (((data[1] & 1) == 0) && (data[5] > 3)) {
			snprintf(tmp, sizeof(tmp), "Method%sIoPortInfoMaxBase10BitAddr", objname);
			fwts_failed(fw, LOG_LEVEL_LOW, tmp,
				"%s I/O Port Descriptor range maximum "
				"base address is more than 10 bits however "
				"the Information field indicates that only "
				"a 10 bit address is being used.", name);
			*passed = false;
		}
		break;
	case 0x9: /* 6.4.2.6 Fixed Location I/O Port Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 3, 3, passed);
		break;
	case 0xa: /* 6.4.2.7 Fixed DMA Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 5, 5, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if (data[5] > 5) {
			snprintf(tmp, sizeof(tmp), "Method%sFixedDmaTransferWidth", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s DMA transfer width is 0x%" PRIx8
				" which is reserved and invalid. See "
				"Section 6.4.2.7 of the ACPI specification.",
				name, data[5]);
			*passed = false;
		}
		break;
	case 0xe: /* 6.4.2.8 Vendor-Defined Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 1, 7, passed);
		break;
	case 0xf: /* 6.4.2.9 End Tag */
		method_test_CRS_small_size(fw, name, objname, data, length, 1, 1, passed);
		break;
	default:
		snprintf(tmp, sizeof(tmp), "Method%sUnkownSmallResourceItem", objname);
		fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
			"%s tag bits 6:3 is an undefined "
			"small tag item name, value 0x%" PRIx8 ".",
			name, tag_item);
		fwts_advice(fw,
			"A small resource data type tag (byte 0, "
			"bits 6:3 of the %s buffer) contains "
			"an undefined small tag item 'name'. "
			"The %s buffer is therefore undefined "
			"and can't be used.  See section "
			"'6.4.2 Small Resource Data Type' of the ACPI "
			"specification, and also table 6-161.",
			objname, objname);
		*passed = false;
		break;
	}

	*tag = types[tag_item];
}



void fwts_method_test_CRS_large_size(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const size_t crs_length,
	const size_t min,
	const size_t max,
	bool *passed)
{
	size_t data_length;
	char tmp[128];

	/* Small _CRS resources have a 3 byte header */
	if (crs_length < 3) {
		snprintf(tmp, sizeof(tmp), "Method%sBufferTooSmall", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return a buffer of at least three bytes in length.", name);
		*passed = false;
		return;
	}

	data_length = (size_t)data[1] + ((size_t)data[2] << 8);

	snprintf(tmp, sizeof(tmp), "Method%sLargeResourceSize",objname);
	method_test_CRS_size(fw, name, objname, tmp,
		crs_length, 3, data_length, min, max, passed);
}

/*
 * Some CRS value fetching helper functions.  We handle all the
 * addresses and lengths in 64 bits to make life easier
 */
static uint64_t method_CRS_val64(const uint8_t *data)
{
	uint64_t val =
		((uint64_t)data[7] << 56) | ((uint64_t)data[6] << 48) |
		((uint64_t)data[5] << 40) | ((uint64_t)data[4] << 32) |
		((uint64_t)data[3] << 24) | ((uint64_t)data[2] << 16) |
		((uint64_t)data[1] << 8)  | (uint64_t)data[0];

	return val;
}

static uint64_t method_CRS_val32(const uint8_t *data)
{
	uint64_t val =
		((uint64_t)data[3] << 24) | ((uint64_t)data[2] << 16) |
		((uint64_t)data[1] << 8)  | (uint64_t)data[0];

	return val;
}

static uint64_t method_CRS_val24(const uint8_t *data)
{
	/* 24 bit values assume lower 8 bits are zero */
	uint64_t val =
		((uint64_t)data[1] << 16) | ((uint64_t)data[0] << 8);

	return val;
}

static uint64_t method_CRS_val16(const uint8_t *data)
{
	uint64_t val =
		((uint64_t)data[1] << 8) | (uint64_t)data[0];

	return val;
}

/*
 *  Sanity check addresses according to table 6-179 of ACPI spec
 */
static void method_test_CRS_mif_maf(
	fwts_framework *fw,
	const char *name,		/* Full _CRS or _PRS path name */
	const char *objname,		/* _CRS or _PRS name */
	const uint8_t flag,		/* _MIF _MAF flag field */
	const uint64_t min,		/* Min address */
	const uint64_t max,		/* Max address */
	const uint64_t len,		/* Range length */
	const uint64_t granularity,	/* Address granularity */
	const char *tag,		/* failed error tag */
	const char *type,		/* Resource type */
	bool *passed)
{
	char tmp[128];
	uint8_t mif = (flag >> 2) & 1;
	uint8_t maf = (flag >> 3) & 1;

	static char *mif_maf_advice =
		"See section '6.4.3.5 Address Space Resource Descriptors' "
		"table 6-179 of the ACPI specification for more details "
		"about how the _MIF, _MAF and memory range and granularity "
		"rules apply. Typically the kernel does not care about these "
		"being correct, so this is a minor issue.";

	/* Table 6-179 Valid combination of Address Space Descriptors fields */
	if (len == 0) {
		if ((mif == 1) && (maf == 1)) {
			snprintf(tmp, sizeof(tmp), "Method%s%sMifMafBothOne", objname, tag);
			if (fw->flags & FWTS_FLAG_SBBR)
				fwts_warning(fw, tmp,
					"%s %s _MIF and _MAF flags are both "
					"set to one which is invalid when "
					"the length field is 0.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s _MIF and _MAF flags are both "
					"set to one which is invalid when "
					"the length field is 0.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}

		if (mif == 1) {
			const uint64_t tmpgran = granularity + 1;

			if (tmpgran == 0) {
				/* Avoid min % tmp division by zero, flag an error */
				snprintf(tmp, sizeof(tmp), "Method%s%sGranTooLarge", objname, tag);
				fwts_warning(fw, tmp,
					"%s %s granularity 0x%" PRIx64 " is too large when _MIF is 1.",
					name, type, granularity);
				*passed = false;
			} else if (min % tmpgran != 0) {
				snprintf(tmp, sizeof(tmp), "Method%s%sMinNotMultipleOfGran", objname, tag);
				if (fw->flags & FWTS_FLAG_SBBR)
					fwts_warning(fw, tmp,
						"%s %s _MIN address is not a multiple "
						"of the granularity when _MIF is 1.",
						name, type);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s _MIN address is not a multiple "
						"of the granularity when _MIF is 1.",
						name, type);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
		}
		if (maf == 1) {
			const uint64_t tmpgran = granularity - 1;

			if (tmpgran == 0) {
				/* Avoid max % tmp division by zero, flag an error */
				snprintf(tmp, sizeof(tmp), "Method%s%sGranTooSmall", objname, tag);
				fwts_warning(fw, tmp,
					"%s %s granularity 0x%" PRIx64 " is too small when _MAF is 1.",
					name, type, granularity);
				*passed = false;
			} else if (max % tmpgran != 0) {
				snprintf(tmp, sizeof(tmp), "Method%s%sMaxNotMultipleOfGran", objname, tag);
				if (fw->flags & FWTS_FLAG_SBBR)
					fwts_warning(fw, tmp,
						"%s %s _MAX address is not a multiple "
						"of the granularity when _MAF is 1.",
						name, type);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s _MAX address is not a multiple "
						"of the granularity when _MAF is 1.",
						name, type);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
		}
	} else {
		if ((mif == 0) && (maf == 0) &&
		    (len % (granularity + 1) != 0)) {
			snprintf(tmp, sizeof(tmp), "Method%s%sLenNotMultipleOfGran", objname, tag);
			if (fw->flags & FWTS_FLAG_SBBR)
				fwts_warning(fw, tmp,
					"%s %s length is not a multiple "
					"of the granularity when _MIF "
					"and _MIF are 0.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s length is not a multiple "
					"of the granularity when _MIF "
					"and _MIF are 0.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
		if (((mif == 0) && (maf == 1)) || ((mif == 1) && (maf == 0))) {
			snprintf(tmp, sizeof(tmp), "Method%s%sMifMafInvalid", objname, tag);
			if (fw->flags & FWTS_FLAG_SBBR)
				fwts_warning(fw, tmp,
					"%s %s _MIF and _MAF flags are either "
					"0 and 1 or 1 and 0 which is invalid when "
					"the length field is non-zero.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s _MIF and _MAF flags are either "
					"0 and 1 or 1 and 0 which is invalid when "
					"the length field is non-zero.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
		if ((mif == 1) && (maf == 1)) {
			if (granularity != 0) {
				snprintf(tmp, sizeof(tmp), "Method%s%sGranularityNotZero", objname, tag);
				if (fw->flags & FWTS_FLAG_SBBR)
					fwts_warning(fw, tmp,
						"%s %s granularity 0x%" PRIx64
						" is not zero as expected when "
						"_MIF and _MAF are both 1.",
						name, type, granularity);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s granularity 0x%" PRIx64
						" is not zero as expected when "
						"_MIF and _MAF are both 1.",
						name, type, granularity);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
			if (min > max) {
				snprintf(tmp, sizeof(tmp), "Method%s%sMaxLessThanMin", objname, tag);
				if (fw->flags & FWTS_FLAG_SBBR)
					fwts_warning(fw, tmp,
						"%s %s minimum address range 0x%" PRIx64
						" is greater than the maximum address "
						"range 0x%" PRIx64 ".",
						name, type, min, max);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s minimum address range 0x%" PRIx64
						" is greater than the maximum address "
						"range 0x%" PRIx64 ".",
						name, type, min, max);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
			if (max - min + 1 != len) {
				snprintf(tmp, sizeof(tmp), "Method%s%sLengthInvalid", objname, tag);
				if (fw->flags & FWTS_FLAG_SBBR)
					fwts_warning(fw, tmp,
						"%s %s length 0x%" PRIx64
						" does not match the difference between "
						"the minimum and maximum address ranges "
						"0x%" PRIx64 "-0x%" PRIx64 ".",
						name, type, len, min, max);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s length 0x%" PRIx64
						" does not match the difference between "
						"the minimum and maximum address ranges "
						"0x%" PRIx64 "-0x%" PRIx64 ".",
						name, type, len, min, max);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
		}
	}
}

/*
 *  CRS large resource checks, simple checking
 */
void fwts_method_test_CRS_large_resource_items(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const uint64_t length,
	bool *passed,
	const char **tag)
{
	uint64_t min, max, len, gra;
	uint8_t tag_item = data[0] & 0x7f;
	char tmp[128];

	static const char *types[] = {
		"Reserved",
		"24-bit Memory Range Descriptor",
		"Generic Register Descriptor",
		"Reserved",
		"Vendor Defined Descriptor",
		"32-bit Memory Range Descriptor",
		"32-bit Fixed Location Memory Range Descriptor",
		"DWORD Address Space Descriptor",
		"WORD Address Space Descriptor",
		"Extended IRQ Descriptor",
		"QWORD Address Space Descriptor",
		"Extended Address Space Descriptor",
		"GPIO Connection Descriptor",
		"Reserved",
		"Generic Serial Bus Connection Descriptor",
		"Pin Configuration Descriptor",
		"Pin Group Descriptor",
		"Pin Group Function Descriptor",
		"Pin Group Configuration Descriptor",
		"Reserved",
	};

	switch (tag_item) {
	case 0x1: /* 6.4.3.1 24-Bit Memory Range Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 9, 9, passed);
		if (!*passed)	/* Too short, abort */
			break;
		min = method_CRS_val24(&data[4]);
		max = method_CRS_val24(&data[6]);
		len = method_CRS_val16(&data[10]);
		if (max < min) {
			snprintf(tmp, sizeof(tmp), "Method%s24BitMemRangeMaxLessThanMin", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 24-Bit Memory Range Descriptor minimum "
				"address range 0x%" PRIx64 " is greater than "
				"the maximum address range 0x%" PRIx64 ".",
				name, min, max);
			*passed = false;
		}
		if (len > max + 1 - min) {
			snprintf(tmp, sizeof(tmp), "Method%s24BitMemRangeLengthTooLarge", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 24-Bit Memory Range Descriptor length "
				"0x%" PRIx64 " is greater than size between the "
				"the minimum and maximum address ranges "
				"0x%" PRIx64 "-0x%" PRIx64 ".",
				name, len, min, max);
			*passed = false;
		}
		break;
	case 0x2: /* 6.4.3.7 Generic Register Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 12, 12, passed);
		if (!*passed)
			break;
		switch (data[3]) {
		case 0x00 ... 0x04:
		case 0x0a:
		case 0x7f:
			/* Valid values */
			break;
		default:
			snprintf(tmp, sizeof(tmp), "Method%sGenericRegAddrSpaceIdInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s Generic Register Descriptor has an invalid "
				"Address Space ID 0x%" PRIx8 ".",
				name, data[3]);
			*passed = false;
		}
		if (data[6] > 4) {
			snprintf(tmp, sizeof(tmp), "Method%sGenericRegAddrSizeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s Generic Register Descriptor has an invalid "
				"Address Access Size 0x%" PRIx8 ".",
				name, data[6]);
			*passed = false;
		}
		break;
	case 0x4: /* 6.4.3.2 Vendor-Defined Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 0, 65535, passed);
		break;
	case 0x5: /* 6.4.3.3 32-Bit Memory Range Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 17, 17, passed);
		if (!*passed)
			break;
		min = method_CRS_val32(&data[4]);
		max = method_CRS_val32(&data[8]);
		len = method_CRS_val32(&data[16]);
		if (max < min) {
			snprintf(tmp, sizeof(tmp), "Method%s32BitMemRangeMaxLessThanMin", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 32-Bit Memory Range Descriptor minimum "
				"address range 0x%" PRIx64 " is greater than "
				"the maximum address range 0x%" PRIx64 ".",
				name, min, max);
			*passed = false;
		}
		if (len > max + 1 - min) {
			snprintf(tmp, sizeof(tmp), "Method%s32BitMemRangeLengthTooLarge", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 32-Bit Memory Range Descriptor length "
				"0x%" PRIx64 " is greater than size between the "
				"the minimum and maximum address ranges "
				"0x%" PRIx64 "-0x%" PRIx64 ".",
				name, len, min, max);
			*passed = false;
		}
		break;
	case 0x6: /* 6.4.3.4 32-Bit Fixed Memory Range Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 9, 9, passed);
		/* Not much can be checked for this descriptor */
		break;
	case 0x7: /* 6.4.3.5.2 DWord Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 23, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val32(&data[6]);
		min = method_CRS_val32(&data[10]);
		max = method_CRS_val32(&data[14]);
		len = method_CRS_val32(&data[22]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitDWordAddrSpace",
			types[0x7], passed);
		break;
	case 0x8: /* 6.4.3.5.3 Word Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 13, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val16(&data[6]);
		min = method_CRS_val16(&data[8]);
		max = method_CRS_val16(&data[10]);
		len = method_CRS_val16(&data[14]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitWordAddrSpace",
			types[0x8], passed);
		break;
	case 0x9: /* 6.4.3.6 Extended Interrupt Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 6, 65535, passed);
		/* Not much can be checked for this descriptor */
		break;
	case 0xa: /* 6.4.3.5.1 QWord Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 43, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val64(&data[6]);
		min = method_CRS_val64(&data[14]);
		max = method_CRS_val64(&data[22]);
		len = method_CRS_val64(&data[38]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitQWordAddrSpace",
			types[0xa], passed);
		break;
	case 0xb: /* 6.4.3.5.4 Extended Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 53, 53, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val64(&data[8]);
		min = method_CRS_val64(&data[16]);
		max = method_CRS_val64(&data[24]);
		len = method_CRS_val64(&data[40]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitExtAddrSpace",
			types[0xb], passed);
		break;
	case 0xc: /* 6.4.3.8.1 GPIO Connection Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 22, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if (data[4] > 2) {
			snprintf(tmp, sizeof(tmp), "Method%sGpioConnTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s GPIO Connection Descriptor has an invalid "
				"Connection Type 0x%" PRIx8 ".",
				name, data[4]);
			*passed = false;
			fwts_advice(fw,
				"The GPIO pin connection type is "
				"not recognised. It should be either "
				"0x00 (interrupt connection) or "
				"0x01 (I/O connection). See section "
				"6.4.3.8.1 of the ACPI specification.");
		}
		if ((data[9] > 0x03) && (data[9] < 0x80)) {
			snprintf(tmp, sizeof(tmp), "Method%sGpioConnTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s GPIO Connection Descriptor has an invalid "
				"Pin Configuration Type 0x%" PRIx8 ".",
				name, data[9]);
			*passed = false;
			fwts_advice(fw,
				"The GPIO pin configuration type "
				"is not recognised. It should be one of:"
				"0x00 (default), 0x01 (pull-up), "
				"0x02 (pull-down), 0x03 (no-pull), "
				"0x80-0xff (vendor defined). See table "
				"6-189 in section 6.4.3.8.1 of the ACPI "
				"specification.");
		}
		break;
	case 0xd: /* 6.4.3.9 Pin Function Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 17, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		fwts_acpi_reserved_bits("_CRS", "Flags[low]", data[4], 1, 7, passed);
		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);
		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);
		break;
	case 0xe: /* 6.4.3.8.2 Serial Bus Connection Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 11, 65535, passed);
		/* Don't care */
		break;
	case 0xf: /* 6.4.3.10 Pin Configuration Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 19, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits("_CRS", "Flags[low]", data[4], 2, 7, passed);
		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);

		if (data[6] > 0xd && data[6] < 0x80) {
			*passed = false;
			snprintf(tmp, sizeof(tmp), "Method%sPinConfigTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s Pin Configuration Descriptor has an invalid "
				"Type 0x%" PRIx8 ".", name, data[6]);
			fwts_advice(fw,
				"The Pin Configuration type is "
				"not recognised. It should be either in "
				"range of 0..0x0D or 0x80..0xFF. See "
				"section 6.4.3.10 of the ACPI spec.");
		}

		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);
		break;
	case 0x10: /* 6.4.3.11 Pin Group Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 13, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits("_CRS", "Flags[low]", data[4], 1, 7, passed);
		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);

		break;
	case 0x11: /* 6.4.3.12 Pin Group Function Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 16, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits("_CRS", "Flags[low]", data[4], 2, 7, passed);
		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);
		fwts_acpi_reserved_zero("_CRS", "Resource Source Index", data[8], passed);
		break;
	case 0x12: /* 6.4.3.13 Pin Group Configuration Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 19, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits("_CRS", "Flags[low]", data[4], 2, 7, passed);
		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);

		if (data[6] > 0xd && data[6] < 0x80) {
			*passed = false;
			snprintf(tmp, sizeof(tmp), "Method%sPinConfigTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s Pin Group Configuration Descriptor has an invalid "
				"Type 0x%" PRIx8 ".", name, data[6]);
			fwts_advice(fw,
				"The Pin Group Configuration type is "
				"not recognised. It should be either in "
				"range of 0..0x0D or 0x80..0xFF. See "
				"section 6.4.3.10 of the ACPI spec.");
		}

		fwts_acpi_reserved_zero("_CRS", "Flags[high]", data[5], passed);
		break;
	default:
		snprintf(tmp, sizeof(tmp), "Method%sUnkownLargeResourceItem", objname);
		fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
			"%s tag bits 6:0 is an undefined "
			"large tag item name, value 0x%" PRIx8 ".",
			name, tag_item);
		fwts_advice(fw,
			"A large resource data type tag (byte 0 of the "
			"%s buffer) contains an undefined large tag "
			"item 'name'. The %s buffer is therefore "
			"undefined and can't be used.  See section "
			"'6.4.3 Large Resource Data Type' of the ACPI "
			"specification, and also table 6-173.",
			objname, objname);
		*passed = false;
		break;
	}

	*tag = types[tag_item < 16 ? tag_item : 0];
}

/*
 * Device Identification Objects - see Section 6 Device Configuration
 */

int fwts_method_test_ADR(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_ADR", NULL, 0, fwts_method_test_integer_return, NULL);
}

void fwts_method_valid_CID_Type(
	fwts_framework *fw,
	char *name,
	ACPI_OBJECT *obj)
{
	char tmp[8];

	switch (obj->Type) {
	case ACPI_TYPE_STRING:
		if (obj->String.Pointer) {
			if (fwts_method_valid_HID_string(obj->String.Pointer))
				fwts_passed(fw,
					"%s returned a string '%s' "
					"as expected.",
					name, obj->String.Pointer);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"MethodCIDInvalidString",
					"%s returned a string '%s' "
					"but it was not a valid PNP ID or a "
					"valid ACPI ID.",
					name, obj->String.Pointer);
		} else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_CIDNullString",
				"%s returned a NULL string.", name);
		}
		break;
	case ACPI_TYPE_INTEGER:
		if (fwts_method_valid_EISA_ID((uint32_t)obj->Integer.Value,
			tmp, sizeof(tmp)))
			fwts_passed(fw, "%s returned an integer "
				"0x%8.8" PRIx64 " (EISA ID %s).",
				name, (uint64_t)obj->Integer.Value, tmp);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"MethodCIDInvalidInteger",
				"%s returned a integer 0x%8.8" PRIx64 " "
				"(EISA ID %s) but this is not a valid "
				"EISA ID encoded PNP ID.",
				name, (uint64_t)obj->Integer.Value, tmp);
		break;
	}
}

void fwts_method_test_CID_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint32_t i;

	FWTS_UNUSED(buf);
	FWTS_UNUSED(private);

	if (obj == NULL) {
		fwts_method_failed_null_object(fw, name, "a buffer or integer");
		return;
	}

	switch (obj->Type) {
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_INTEGER:
		fwts_method_valid_CID_Type(fw, name, obj);
		break;
	case ACPI_TYPE_PACKAGE:
		if (fwts_method_package_count_min(fw, name, obj, 1) != FWTS_OK)
			return;

		for (i = 0; i < obj->Package.Count; i++){
			ACPI_OBJECT *pkg = &obj->Package.Elements[i];
			fwts_method_valid_CID_Type(fw, name, pkg);
		}
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "Method_CIDBadReturnType",
			"%s did not return a string or an integer.", name);
		break;
	}
}

int fwts_method_test_CID(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_CID", NULL, 0, fwts_method_test_CID_return, NULL);
}

int fwts_method_test_CLS(fwts_framework *fw, ACPI_HANDLE *device)
{
	uint32_t element_size = 3;
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_CLS", NULL, 0, fwts_method_test_package_integer_return, &element_size);
}

int fwts_method_test_DDN(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_DDN", NULL, 0, fwts_method_test_string_return, NULL);
}

void fwts_method_test_HID_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	char tmp[8];

	FWTS_UNUSED(buf);
	FWTS_UNUSED(private);

	if (obj == NULL) {
		fwts_method_failed_null_object(fw, name, "a buffer or integer");
		return;
	}

	switch (obj->Type) {
	case ACPI_TYPE_STRING:
		if (obj->String.Pointer) {
			if (fwts_method_valid_HID_string(obj->String.Pointer))
				fwts_passed(fw,
					"%s returned a string '%s' "
					"as expected.",
					name, obj->String.Pointer);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"MethodHIDInvalidString",
					"%s returned a string '%s' "
					"but it was not a valid PNP ID or a "
					"valid ACPI ID.",
					name, obj->String.Pointer);
		} else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_HIDNullString",
				"%s returned a NULL string.", name);
		}
		break;
	case ACPI_TYPE_INTEGER:
		if (fwts_method_valid_EISA_ID((uint32_t)obj->Integer.Value,
			tmp, sizeof(tmp)))
			fwts_passed(fw, "%s returned an integer "
				"0x%8.8" PRIx64 " (EISA ID %s).",
				name, (uint64_t)obj->Integer.Value, tmp);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"MethodHIDInvalidInteger",
				"%s returned a integer 0x%8.8" PRIx64 " "
				"(EISA ID %s) but this is not a valid "
				"EISA ID encoded PNP ID.",
				name, (uint64_t)obj->Integer.Value, tmp);
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "Method_HIDBadReturnType",
			"%s did not return a string or an integer.", name);
		break;
	}
}

int fwts_method_test_HID(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_HID", NULL, 0, fwts_method_test_HID_return, NULL);
}

int fwts_method_test_HRV(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_HRV", NULL, 0, fwts_method_test_integer_return, NULL);
}

void fwts_method_test_MLS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	uint32_t i;
	bool failed = false;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_all_type(fw, name, obj, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	/* Could be one or more packages */
	for (i = 0; i < obj->Package.Count; i++) {
		ACPI_OBJECT *pkg = &obj->Package.Elements[i];

		if (pkg->Package.Count != 2) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_MLSSubPackageElementCount",
				"%s sub-package %" PRIu32 " was expected to "
				"have 2 elements, got %" PRIu32 " elements instead.",
				name, i, pkg->Package.Count);
			failed = true;
			continue;
		}

		if (pkg->Package.Elements[0].Type != ACPI_TYPE_STRING) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_MLSBadSubPackageReturnType",
				"%s sub-package %" PRIu32
				" element 0 is not a string.",
				name, i);
			failed = true;
		}

		if (pkg->Package.Elements[1].Type != ACPI_TYPE_BUFFER) {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_MLSBadSubPackageReturnType",
				"%s sub-package %" PRIu32
				" element 1 is not a buffer.",
				name, i);
			failed = true;
		}
	}

	if (!failed)
		fwts_method_passed_sane(fw, name, "package");
}

int fwts_method_test_MLS(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_MLS", NULL, 0, fwts_method_test_MLS_return, NULL);
}

void fwts_method_test_PLD_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	/* All elements in the package must be buffers */
	if (fwts_method_package_elements_all_type(fw, name, obj, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	fwts_method_passed_sane(fw, name, "package");
}

int fwts_method_test_PLD(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_PLD", NULL, 0, fwts_method_test_PLD_return, NULL);
}

void fwts_method_test_SUB_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(buf);
	FWTS_UNUSED(private);

	if (obj == NULL) {
		fwts_method_failed_null_object(fw, name, "a buffer");
		return;
	}

	if (obj->Type == ACPI_TYPE_STRING)
		if (obj->String.Pointer) {
			if (fwts_method_valid_HID_string(obj->String.Pointer))
				fwts_passed(fw,
					"%s returned a string '%s' "
					"as expected.",
					name, obj->String.Pointer);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					"MethodSUBInvalidString",
					"%s returned a string '%s' "
					"but it was not a valid PNP ID or a "
					"valid ACPI ID.",
					name, obj->String.Pointer);
		} else {
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_SUBNullString",
				"%s returned a NULL string.", name);
		}
	else {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "Method_SUBBadReturnType",
			"Method _SUB did not return a string.");
	}
}

int fwts_method_test_SUB(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_SUB", NULL, 0, fwts_method_test_SUB_return, NULL);
}

int fwts_method_test_SUN(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_SUN", NULL, 0, fwts_method_test_integer_return, NULL);
}

int fwts_method_test_STR(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_STR", NULL, 0, fwts_method_test_buffer_return, NULL);
}

void fwts_method_test_UID_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(buf);
	FWTS_UNUSED(private);

	if (obj == NULL) {
		fwts_method_failed_null_object(fw, name, "a buffer or integer");
		return;
	}

	switch (obj->Type) {
	case ACPI_TYPE_STRING:
		if (obj->String.Pointer)
			fwts_passed(fw,
				"%s returned a string '%s' as expected.",
				name, obj->String.Pointer);
		else
			fwts_failed(fw, LOG_LEVEL_MEDIUM,
				"Method_UIDNullString",
				"%s returned a NULL string.", name);
		break;
	case ACPI_TYPE_INTEGER:
		fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "Method_UIDBadReturnType",
			"Method %s did not return a string or an integer.", name);
		break;
	}
}

int fwts_method_test_UID(fwts_framework *fw, ACPI_HANDLE *device)
{
	return fwts_evaluate_method(fw, METHOD_OPTIONAL, device,
		"_UID", NULL, 0, fwts_method_test_UID_return, NULL);
}

void fwts_method_test_BMD_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_count_equal(fw, name, obj, 5) != FWTS_OK)
		return;

	if (fwts_method_package_elements_all_type(fw, name, obj, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	fwts_acpi_reserved_bits("_BMD", "Status Flags",
		obj->Package.Elements[0].Integer.Value, 7, 31, &failed);

	fwts_acpi_reserved_bits("_BMD", "Capability Flags",
		obj->Package.Elements[1].Integer.Value, 6, 31, &failed);

	if (!failed)
		fwts_method_passed_sane(fw, name, "package");
}

/*
 * Section 9.20 NVDIMM Devices
 */
static void check_nvdimm_status(
	fwts_framework *fw,
	char *name,
	uint16_t status,
	bool *passed)
{
	if (status > 6) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"MethodBadStatus",
			"%s: Expected Status to be 0..6, got %" PRIx16,
			name, status);
	}
}

static void check_nvdimm_extended_status(
	fwts_framework *fw,
	char *name,
	uint16_t ext_status,
	uint16_t expected,
	bool *passed)
{
	if (ext_status != expected) {
		*passed = false;
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"MethodBadExtendedStatus",
			"%s: Expected Extended Status to be %" PRIx16
			", got %" PRIx16, name, expected, ext_status);
	}
}

void fwts_method_test_NBS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;
	nbs_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		passed = false;

	ret = (nbs_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &passed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 0, &passed);
	fwts_acpi_reserved_bits("_NBS", "Validation Flags",
		ret->validation_flags, 1, 15, &passed);

	if (passed)
		fwts_method_passed_sane(fw, name, "buffer");
}

void fwts_method_test_NCH_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;
	nch_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		passed = false;

	ret = (nch_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &passed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 0, &passed);
	fwts_acpi_reserved_bits("_NCH", "Validation Flags",
		ret->extended_status, 2, 15, &passed);

	/* Health Status Flags [2..7], [11.15], [19..31] are reserved */
	fwts_acpi_reserved_bits("_NCH", "Health Status Flags",
		ret->health_status_flags, 2, 7, &passed);
	fwts_acpi_reserved_bits("_NCH", "Health Status Flags",
		ret->health_status_flags, 11, 15, &passed);
	fwts_acpi_reserved_bits("_NCH", "Health Status Flags",
		ret->health_status_flags, 19, 31, &passed);

	fwts_acpi_reserved_bits("_NCH", "Health Status Attributes",
		ret->health_status_attributes, 1, 31, &passed);

	if (passed)
		fwts_method_passed_sane(fw, name, "buffer");
}

void fwts_method_test_NIC_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;
	nic_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		passed = false;

	ret = (nic_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &passed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 0, &passed);

	/* Health Error Injection Capabilities [2..7], [11.15], [19..31] are reserved */
	fwts_acpi_reserved_bits("_NIC", "Health Error Injection Capabilities",
		ret->health_error_injection, 2, 7, &passed);
	fwts_acpi_reserved_bits("_NIC", "Health Error Injection Capabilities",
		ret->health_error_injection, 11, 15, &passed);
	fwts_acpi_reserved_bits("_NIC", "Health Error Injection Capabilities",
		ret->health_error_injection, 19, 31, &passed);

	fwts_acpi_reserved_bits("_NIC", "Health Status Attributes Capabilities",
		ret->health_status_attributes, 1, 31, &passed);

	if (passed)
		fwts_method_passed_sane(fw, name, "buffer");
}

void fwts_method_test_NIH_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;
	nih_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 4) != FWTS_OK)
		passed = false;

	ret = (nih_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &passed);
	check_nvdimm_extended_status(fw, name, ret->extended_status, 1, &passed);

	if (passed)
		fwts_method_passed_sane(fw, name, "buffer");
}

void fwts_method_test_NIG_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;
	nig_return_t *ret;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_BUFFER) != FWTS_OK)
		return;

	if (fwts_method_buffer_size(fw, name, obj, 64) != FWTS_OK)
		passed = false;

	ret = (nig_return_t *) obj->Buffer.Pointer;
	check_nvdimm_status(fw, name, ret->status, &passed);
	check_nvdimm_extended_status(fw, name,  ret->extended_status, 0, &passed);
	fwts_acpi_reserved_bits("_NIG", "Validation Flags",
		ret->validation_flags, 2, 15, &passed);

	/* Injected Health Status Errors [2..7], [11.15], [19..31] are reserved */
	fwts_acpi_reserved_bits("_NIG", "Injected Health Status Errors",
		ret->health_status_errors, 2, 7, &passed);
	fwts_acpi_reserved_bits("_NIG", "Injected Health Status Errors",
		ret->health_status_errors, 11, 15, &passed);
	fwts_acpi_reserved_bits("_NIG", "Injected Health Status Errors",
		ret->health_status_errors, 19, 31, &passed);

	fwts_acpi_reserved_bits("_NIG", "Health Status Attributes of Injected Errors",
		ret->health_status_attributes, 1, 31, &passed);

	if (passed)
		fwts_method_passed_sane(fw, name, "buffer");
}

void fwts_method_test_STA_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) != FWTS_OK)
		return;

	if ((obj->Integer.Value & 3) == 2) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"Method_STAEnabledNotPresent",
			"%s indicates that the device is enabled "
			"but not present, which is impossible.", name);
		passed = false;
	}
	fwts_acpi_reserved_bits("_STA", "Reserved Bits", obj->Integer.Value, 5, 31, &passed);

	if (passed)
		fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
}

/*
 *  Section 10.2 Control Method Batteries
 */

void fwts_method_test_BIF_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;

	static const fwts_package_element elements[] = {
		{ ACPI_TYPE_INTEGER,	"Power Unit" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity" },
		{ ACPI_TYPE_INTEGER,	"Last Full Charge Capacity" },
		{ ACPI_TYPE_INTEGER,	"Battery Technology" },
		{ ACPI_TYPE_INTEGER,	"Design Voltage" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity of Warning" },
		{ ACPI_TYPE_INTEGER,	"Design Capactty of Low" },
		{ ACPI_TYPE_INTEGER,	"Battery Capacity Granularity 1" },
		{ ACPI_TYPE_INTEGER,	"Battery Capacity Granularity 2" },
		{ ACPI_TYPE_STRING,	"Model Number" },
		{ ACPI_TYPE_STRING,	"Serial Number" },
		{ ACPI_TYPE_STRING,	"Battery Type" },
		{ ACPI_TYPE_STRING,	"OEM Information" }
	};

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_type(fw, name, obj, elements) != FWTS_OK)
		return;

	/* Sanity check each field */
	/* Power Unit */
	if (obj->Package.Elements[0].Integer.Value > 0x00000002) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIFBadUnits",
			"%s: Expected Power Unit (Element 0) to be "
			"0 (mWh) or 1 (mAh), got 0x%8.8" PRIx64 ".",
			name, (uint64_t)obj->Package.Elements[0].Integer.Value);
		failed = true;
	}
#ifdef FWTS_METHOD_PEDANDTIC
	/*
	 * Since this information may be evaluated by communicating with
	 * the EC we skip these checks as we can't do this from userspace
	 */
	/* Design Capacity */
	if (obj->Package.Elements[1].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIFBadCapacity",
			"%s: Design Capacity (Element 1) is "
			"unknown: 0x%8.8" PRIx64 ".",
			name, obj->Package.Elements[1].Integer.Value);
		failed = true;
	}
	/* Last Full Charge Capacity */
	if (obj->Package.Elements[2].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIFChargeCapacity",
			"%s: Last Full Charge Capacity (Element 2) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, obj->Package.Elements[2].Integer.Value);
		failed = true;
	}
#endif
	/* Battery Technology */
	if (obj->Package.Elements[3].Integer.Value > 0x00000002) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"Method_BIFBatTechUnit",
			"%s: Expected Battery Technology Unit "
			"(Element 3) to be 0 (Primary) or 1 "
			"(Secondary), got 0x%8.8" PRIx64 ".",
			name, (uint64_t)obj->Package.Elements[3].Integer.Value);
		failed = true;
	}
#ifdef FWTS_METHOD_PEDANDTIC
	/*
	 * Since this information may be evaluated by communicating with
	 * the EC we skip these checks as we can't do this from userspace
	 */
	/* Design Voltage */
	if (obj->Package.Elements[4].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIFDesignVoltage",
			"%s: Design Voltage (Element 4) is "
			"unknown: 0x%8.8" PRIx64 ".",
			name, obj->Package.Elements[4].Integer.Value);
		failed = true;
	}
	/* Design capacity warning */
	if (obj->Package.Elements[5].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIFDesignCapacityE5",
			"%s: Design Capacity Warning (Element 5) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, obj->Package.Elements[5].Integer.Value);
		failed = true;
	}
	/* Design capacity low */
	if (obj->Package.Elements[6].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIFDesignCapacityE6",
			"%s: Design Capacity Warning (Element 6) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, obj->Package.Elements[6].Integer.Value);
		failed = true;
	}
#endif
	if (failed)
		fwts_advice(fw,
			"Battery %s package contains errors. It is "
			"worth running the firmware test suite "
			"interactive 'battery' test to see if this "
			"is problematic.  This is a bug an needs to "
			"be fixed.", name);
	else
		fwts_method_passed_sane(fw, name, "package");
}

void fwts_method_test_BIX_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;
	uint64_t revision = 0;

	/* Revision 0 in ACPI 5.x */
	static const fwts_package_element elements[] = {
		{ ACPI_TYPE_INTEGER,	"Revision" },
		{ ACPI_TYPE_INTEGER,	"Power Unit" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity" },
		{ ACPI_TYPE_INTEGER,	"Last Full Charge Capacity" },
		{ ACPI_TYPE_INTEGER,	"Battery Technology" },
		{ ACPI_TYPE_INTEGER,	"Design Voltage" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity of Warning" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity of Low" },
		{ ACPI_TYPE_INTEGER,	"Cycle Count" },
		{ ACPI_TYPE_INTEGER,	"Measurement Accuracy" },
		{ ACPI_TYPE_INTEGER,	"Max Sampling Time" },
		{ ACPI_TYPE_INTEGER,	"Min Sampling Time" },
		{ ACPI_TYPE_INTEGER,	"Max Averaging Interval" },
		{ ACPI_TYPE_INTEGER,	"Min Averaging Interval" },
		{ ACPI_TYPE_INTEGER,	"Battery Capacity Granularity 1" },
		{ ACPI_TYPE_INTEGER,	"Battery Capacity Granularity 2" },
		{ ACPI_TYPE_STRING,	"Model Number" },
		{ ACPI_TYPE_STRING,	"Serial Number" },
		{ ACPI_TYPE_STRING,	"Battery Type" },
		{ ACPI_TYPE_STRING,	"OEM Information" }
	};

	/* Revision 1 in ACPI 6.x introduces swapping capability */
	static const fwts_package_element elements_v1[] = {
		{ ACPI_TYPE_INTEGER,	"Revision" },
		{ ACPI_TYPE_INTEGER,	"Power Unit" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity" },
		{ ACPI_TYPE_INTEGER,	"Last Full Charge Capacity" },
		{ ACPI_TYPE_INTEGER,	"Battery Technology" },
		{ ACPI_TYPE_INTEGER,	"Design Voltage" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity of Warning" },
		{ ACPI_TYPE_INTEGER,	"Design Capacity of Low" },
		{ ACPI_TYPE_INTEGER,	"Cycle Count" },
		{ ACPI_TYPE_INTEGER,	"Measurement Accuracy" },
		{ ACPI_TYPE_INTEGER,	"Max Sampling Time" },
		{ ACPI_TYPE_INTEGER,	"Min Sampling Time" },
		{ ACPI_TYPE_INTEGER,	"Max Averaging Interval" },
		{ ACPI_TYPE_INTEGER,	"Min Averaging Interval" },
		{ ACPI_TYPE_INTEGER,	"Battery Capacity Granularity 1" },
		{ ACPI_TYPE_INTEGER,	"Battery Capacity Granularity 2" },
		{ ACPI_TYPE_STRING,	"Model Number" },
		{ ACPI_TYPE_STRING,	"Serial Number" },
		{ ACPI_TYPE_STRING,	"Battery Type" },
		{ ACPI_TYPE_STRING,	"OEM Information" },
		{ ACPI_TYPE_INTEGER,	"Battery Swapping Capability" }
	};

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (obj->Package.Count > 1 && obj->Package.Elements[0].Type == ACPI_TYPE_INTEGER)
		revision = obj->Package.Elements[0].Integer.Value;

	switch (revision) {
	case 0:
		if (fwts_method_package_elements_type(fw, name, obj, elements) != FWTS_OK)
			return;
		break;
	case 1:
		if (fwts_method_package_elements_type(fw, name, obj, elements_v1) != FWTS_OK)
			return;
		break;
	default:
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXRevision",
			"%s: Expected %s (Element 0) to be "
			"0 or 1, got 0x%8.8" PRIx64 ".",
			name, elements_v1[0].name,
			(uint64_t)obj->Package.Elements[0].Integer.Value);
		return;
	}

	/* Sanity check each field */
	/* Power Unit */
	if (obj->Package.Elements[1].Integer.Value > 0x00000002) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXPowerUnit",
			"%s: Expected %s (Element 1) to be "
			"0 (mWh) or 1 (mAh), got 0x%8.8" PRIx64 ".",
			name, elements[1].name,
			(uint64_t)obj->Package.Elements[1].Integer.Value);
		failed = true;
	}
#ifdef FWTS_METHOD_PEDANDTIC
	/*
	 * Since this information may be evaluated by communicating with
	 * the EC we skip these checks as we can't do this from userspace
	 */
	/* Design Capacity */
	if (obj->Package.Elements[2].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXDesignCapacity",
			"%s: %s (Element 2) is "
			"unknown: 0x%8.8" PRIx64 ".",
			name, elements[2].name,
			obj->Package.Elements[2].Integer.Value);
		failed = true;
	}
	/* Last Full Charge Capacity */
	if (obj->Package.Elements[3].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXFullChargeCapacity",
			"%s: %s (Element 3) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, elements[3].name,
			obj->Package.Elements[3].Integer.Value);
		failed = true;
	}
#endif
	/* Battery Technology */
	if (obj->Package.Elements[4].Integer.Value > 0x00000002) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"Method_BIXBatteryTechUnit",
			"%s: %s "
			"(Element 4) to be 0 (Primary) or 1 "
			"(Secondary), got 0x%8.8" PRIx64 ".",
			name, elements[4].name,
			(uint64_t)obj->Package.Elements[4].Integer.Value);
		failed = true;
	}
#ifdef FWTS_METHOD_PEDANDTIC
	/*
	 * Since this information may be evaluated by communicating with
	 * the EC we skip these checks as we can't do this from userspace
	 */
	/* Design Voltage */
	if (obj->Package.Elements[5].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXDesignVoltage",
			"%s: %s (Element 5) is unknown: "
			"0x%8.8" PRIx64 ".",
			name, elements[5].name,
			obj->Package.Elements[5].Integer.Value);
		failed = true;
	}
	/* Design capacity warning */
	if (obj->Package.Elements[6].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXDesignCapacityE6",
			"%s: %s (Element 6) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, elements[6].name,
			obj->Package.Elements[6].Integer.Value);
		failed = true;
	}
	/* Design capacity low */
	if (obj->Package.Elements[7].Integer.Value > 0x7fffffff) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXDesignCapacityE7",
			"%s: %s (Element 7) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, elements[7].name,
			obj->Package.Elements[7].Integer.Value);
		failed = true;
	}
	/* Cycle Count: value = 0 ~ 0xFFFFFFFE or 0xFFFFFFFF (unknown) */

	/* Measurement Accuracy */
	if (obj->Package.Elements[9].Integer.Value > 100000) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXMeasurementAccuracy",
			"%s: %s (Element 9) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, elements[9].name,
			obj->Package.Elements[9].Integer.Value);
		failed = true;
	}

#endif

	/* Swapping Capability */
	if (revision == 1 && obj->Package.Elements[20].Integer.Value > 2) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BIXSwappingCapability",
			"%s: %s (Element 20) "
			"is unknown: 0x%8.8" PRIx64 ".",
			name, elements_v1[20].name,
			obj->Package.Elements[20].Integer.Value);
		failed = true;
	}

	if (failed)
		fwts_advice(fw,
			"Battery %s package contains errors. It is "
			"worth running the firmware test suite "
			"interactive 'battery' test to see if this "
			"is problematic.  This is a bug an needs to "
			"be fixed.", name);
	else
		fwts_method_passed_sane(fw, name, "package");
}

void fwts_method_test_BST_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool failed = false;

	static const fwts_package_element elements[] = {
		{ ACPI_TYPE_INTEGER,	"Battery State" },
		{ ACPI_TYPE_INTEGER,	"Battery Rate" },
		{ ACPI_TYPE_INTEGER,	"Battery Remaining Capacity" },
		{ ACPI_TYPE_INTEGER,	"Battery Present Voltage" },
	};

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_type(fw, name, obj, elements) != FWTS_OK)
		return;

	/* Sanity check each field */
	/* Battery State */
	fwts_acpi_reserved_bits("_BST", "Battery State",
			obj->Package.Elements[0].Integer.Value, 4, 31, &failed);
	/* Ensure bits 0 (discharging) and 1 (charging) are not both set, see 10.2.2.6 */
	if (((obj->Package.Elements[0].Integer.Value) & 3) == 3) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL,
			"Method_BSTBadState",
			"%s: Battery State (Element 0) is "
			"indicating both charging and discharginng "
			"which is not allowed. Got value 0x%8.8" PRIx64 ".",
			name, (uint64_t)obj->Package.Elements[0].Integer.Value);
		failed = true;
	}
	/* Battery Present Rate - cannot check, pulled from EC */
	/* Battery Remaining Capacity - cannot check, pulled from EC */
	/* Battery Present Voltage - cannot check, pulled from EC */
	if (failed)
		fwts_advice(fw,
			"Battery %s package contains errors. It is "
			"worth running the firmware test suite "
			"interactive 'battery' test to see if this "
			"is problematic.  This is a bug an needs to "
			"be fixed.", name);
		else
			fwts_method_passed_sane(fw, name, "package");
}

void fwts_method_test_BPC_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;

	static const fwts_package_element elements[] = {
		{ ACPI_TYPE_INTEGER,	"Revision," },
		{ ACPI_TYPE_INTEGER,	"Power Threshold Support" },
		{ ACPI_TYPE_INTEGER,	"Max Instantaneous Peak Power Threshold" },
		{ ACPI_TYPE_INTEGER,	"Max Sustainable Peak Power Threshold" }
	};

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_type(fw, name, obj, elements) != FWTS_OK)
		return;

	if (fwts_method_test_revision(fw, name, obj->Package.Elements[0].Integer.Value, 1) != FWTS_OK)
		passed = false;

	fwts_acpi_reserved_bits("_BPC", "Power Threshold Support Capability",
		obj->Package.Elements[1].Integer.Value, 2, 31, &passed);

	if (!passed)
		fwts_advice(fw,
			"Battery %s package contains errors.", name);
		else
			fwts_method_passed_sane(fw, name, "package");
}

void fwts_method_test_BPS_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	bool passed = true;

	static const fwts_package_element elements[] = {
		{ ACPI_TYPE_INTEGER,	"Revision," },
		{ ACPI_TYPE_INTEGER,	"Instantaneous Peak Power Level" },
		{ ACPI_TYPE_INTEGER,	"Instantaneous Peak Power Period" },
		{ ACPI_TYPE_INTEGER,	"Sustainable Peak Power Level" },
		{ ACPI_TYPE_INTEGER,	"Sustainable Peak Power Period" },
	};

	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_PACKAGE) != FWTS_OK)
		return;

	if (fwts_method_package_elements_type(fw, name, obj, elements) != FWTS_OK)
		return;

	if (fwts_method_test_revision(fw, name, obj->Package.Elements[0].Integer.Value, 1) != FWTS_OK)
		passed = false;

	if (!passed)
		fwts_advice(fw,
			"Battery %s package contains errors.", name);
		else
			fwts_method_passed_sane(fw, name, "package");
}
