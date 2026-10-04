/******************************************************************************
 *
 * Module Name: aecommon - common include for the AcpiExec utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef _AECOMMON
#define _AECOMMON

#ifdef _MSC_VER                 /* disable some level-4 warnings */
#pragma warning(disable:4100)   /* warning C4100: unreferenced formal parameter */
#endif

#include "acpi.h"
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acdebug.h"
#include "actables.h"
#include "acinterp.h"
#include "amlresrc.h"
#include "acapps.h"


/*
 * Debug Regions
 */
typedef struct ae_region
{
    ACPI_PHYSICAL_ADDRESS   Address;
    UINT32                  Length;
    void                    *Buffer;
    void                    *NextRegion;
    UINT8                   SpaceId;

} AE_REGION;

typedef struct ae_debug_regions
{
    UINT32                  NumberOfRegions;
    AE_REGION               *RegionList;

} AE_DEBUG_REGIONS;


/*
 * Init file entry
 */
typedef struct init_file_entry
{
    char                    *Name;
    char                    *Value;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    BOOLEAN                 IsUsed;

} INIT_FILE_ENTRY;

extern BOOLEAN              AcpiGbl_UseLocalFaultHandler;
extern BOOLEAN              AcpiGbl_VerboseHandlers;
extern BOOLEAN              AcpiGbl_IgnoreErrors;
extern BOOLEAN              AcpiGbl_AbortLoopOnTimeout;
extern UINT8                AcpiGbl_RegionFillValue;
extern INIT_FILE_ENTRY      *AcpiGbl_InitEntries;
extern UINT32               AcpiGbl_InitFileLineCount;
extern UINT8                AcpiGbl_UseHwReducedFadt;
extern BOOLEAN              AcpiGbl_DisplayRegionAccess;
extern BOOLEAN              AcpiGbl_DoInterfaceTests;
extern BOOLEAN              AcpiGbl_LoadTestTables;
extern FILE                 *AcpiGbl_NamespaceInitFile;
extern ACPI_CONNECTION_INFO AeMyContext;

extern UINT8                Ssdt2Code[];
extern UINT8                Ssdt3Code[];
extern UINT8                Ssdt4Code[];


#define TEST_OUTPUT_LEVEL(lvl)          if ((lvl) & OutputLevel)

#define OSD_PRINT(lvl,fp)               TEST_OUTPUT_LEVEL(lvl) {\
                                            AcpiOsPrintf PARAM_LIST(fp);}

#define AE_PREFIX                       "ACPI Exec: "

void ACPI_SYSTEM_XFACE
AeSignalHandler (
    int                     Sig);

ACPI_STATUS
AeExceptionHandler (
    ACPI_STATUS             AmlStatus,
    ACPI_NAME               Name,
    UINT16                  Opcode,
    UINT32                  AmlOffset,
    void                    *Context);

ACPI_STATUS
AeBuildLocalTables (
    ACPI_NEW_TABLE_DESC     *TableList);

ACPI_STATUS
AeInstallTables (
    void);

ACPI_STATUS
AeLoadTables (
    void);

void
AeDumpNamespace (
    void);

void
AeDumpObject (
    char                    *MethodName,
    ACPI_BUFFER             *ReturnObj);

void
AeDumpBuffer (
    UINT32                  Address);

void
AeExecute (
    char                    *Name);

void
AeSetScope (
    char                    *Name);

void
AeCloseDebugFile (
    void);

void
AeOpenDebugFile (
    char                    *Name);

ACPI_STATUS
AeDisplayAllMethods (
    UINT32                  DisplayCount);

/* aetests */

void
AeMiscellaneousTests (
    void);

void
AeLateTest (
    void);

/* aeregion */

ACPI_STATUS
AeRegionHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT64                  *Value,
    void                    *HandlerContext,
    void                    *RegionContext);

/* aeinstall */

ACPI_STATUS
AeInstallDeviceHandlers (
    void);

void
AeInstallRegionHandlers (
    void);

void
AeOverrideRegionHandlers (
    void);

/* aehandlers */

ACPI_STATUS
AeInstallEarlyHandlers (
    void);

ACPI_STATUS
AeInstallLateHandlers (
    void);

UINT32
AeGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    void                    *Context);

void
AeGlobalEventHandler (
    UINT32                  Type,
    ACPI_HANDLE             GpeDevice,
    UINT32                  EventNumber,
    void                    *Context);

/* aeinitfile */

int
AeOpenInitializationFile (
    char                    *Filename);

ACPI_STATUS
AeProcessInitFile (
    void);

ACPI_STATUS
AeLookupInitFileEntry (
    char                    *Pathname,
    ACPI_OPERAND_OBJECT     **ObjDesc);

void
AeDisplayUnusedInitFileItems (
    void);

void
AeDeleteInitFileList (
    void);

/* aeexec */

void
AeTestBufferArgument (
    void);

void
AeTestPackageArgument (
    void);

ACPI_STATUS
AeGetDevices (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
AeSetupConfiguration (
    void                    *RegionAddr);

ACPI_STATUS
ExecuteOSI (
    char                    *OsiString,
    UINT64                  ExpectedResult);

void
AeGenericRegisters (
    void);

#if (!ACPI_REDUCED_HARDWARE)
void
AfInstallGpeBlock (
    void);
#endif /* !ACPI_REDUCED_HARDWARE */

#endif /* _AECOMMON */
