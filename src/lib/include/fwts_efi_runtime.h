/*
 * Copyright (C) 2016-2026 Canonical Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _FWTS_EFI_RUNTIME_H_
#define _FWTS_EFI_RUNTIME_H_

typedef enum {
	EfiResetCold,
	EfiResetWarm,
	EfiResetShutdown,
	EfiResetPlatformSpecific
} EFI_RESET_TYPE;

typedef struct {
	uint32_t	Data1;
	uint16_t	Data2;
	uint16_t	Data3;
	uint8_t		Data4[8];
} __attribute__ ((packed)) EFI_GUID;

typedef struct {
	uint16_t	Year;		/* 1900 – 9999 */
	uint8_t		Month;		/* 1 – 12 */
	uint8_t		Day;		/* 1 – 31 */
	uint8_t		Hour;		/* 0 – 23 */
	uint8_t		Minute;		/* 0 – 59 */
	uint8_t		Second;		/* 0 – 59 */
	uint8_t		Pad1;
	uint32_t	Nanosecond;	/* 0 – 999,999,999 */
	int16_t		TimeZone;	/* -1440 to 1440 or 2047 */
	uint8_t		Daylight;
	uint8_t		Pad2;
} __attribute__ ((packed)) EFI_TIME;

typedef struct {
	uint32_t	Resolution;
	uint32_t	Accuracy;
	uint8_t		SetsToZero;
} __attribute__ ((packed)) EFI_TIME_CAPABILITIES;

typedef struct {
	EFI_GUID CapsuleGuid;
	uint32_t HeaderSize;
	uint32_t Flags;
	uint32_t CapsuleImageSize;
} __attribute__ ((packed)) EFI_CAPSULE_HEADER;

struct efi_getvariable {
	uint16_t	*VariableName;
	EFI_GUID	*VendorGuid;
	uint32_t	*Attributes;
	uint64_t	*DataSize;
	void		*Data;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_setvariable {
	uint16_t	*VariableName;
	EFI_GUID	*VendorGuid;
	uint32_t	Attributes;
	uint64_t	DataSize;
	void		*Data;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_getnextvariablename {
	uint64_t	*VariableNameSize;
	uint16_t	*VariableName;
	EFI_GUID	*VendorGuid;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_queryvariableinfo {
	uint32_t	Attributes;
	uint64_t	*MaximumVariableStorageSize;
	uint64_t	*RemainingVariableStorageSize;
	uint64_t	*MaximumVariableSize;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_gettime {
	EFI_TIME		*Time;
	EFI_TIME_CAPABILITIES	*Capabilities;
	uint64_t		*status;
} __attribute__ ((packed));

struct efi_settime {
	EFI_TIME		*Time;
	uint64_t		*status;
} __attribute__ ((packed));

struct efi_getwakeuptime {
	uint8_t		*Enabled;
	uint8_t		*Pending;
	EFI_TIME	*Time;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_setwakeuptime {
	uint8_t		Enabled;
	EFI_TIME	*Time;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_getnexthighmonotoniccount {
	uint32_t	*HighCount;
	uint64_t	*status;
} __attribute__ ((packed));

struct efi_querycapsulecapabilities {
	EFI_CAPSULE_HEADER	**CapsuleHeaderArray;
	uint64_t		CapsuleCount;
	uint64_t		*MaximumCapsuleSize;
	EFI_RESET_TYPE		*ResetType;
	uint64_t		*status;
} __attribute__ ((packed));

/* ioctl calls that are permitted to the /dev/efi_runtime interface. */
#define EFI_RUNTIME_GET_VARIABLE \
	_IOWR('p', 0x01, struct efi_getvariable)
#define EFI_RUNTIME_SET_VARIABLE \
	_IOW('p', 0x02, struct efi_setvariable)

#define EFI_RUNTIME_GET_TIME \
	_IOR('p', 0x03, struct efi_gettime)
#define EFI_RUNTIME_SET_TIME \
	_IOW('p', 0x04, struct efi_settime)

#define EFI_RUNTIME_GET_WAKETIME \
	_IOR('p', 0x05, struct efi_getwakeuptime)
#define EFI_RUNTIME_SET_WAKETIME \
	_IOW('p', 0x06, struct efi_setwakeuptime)

#define EFI_RUNTIME_GET_NEXTVARIABLENAME \
	_IOWR('p', 0x07, struct efi_getnextvariablename)

#define EFI_RUNTIME_QUERY_VARIABLEINFO \
	_IOR('p', 0x08, struct efi_queryvariableinfo)

#define EFI_RUNTIME_GET_NEXTHIGHMONOTONICCOUNT \
	_IOR('p', 0x09, struct efi_getnexthighmonotoniccount)

#define EFI_RUNTIME_QUERY_CAPSULECAPABILITIES \
	_IOR('p', 0x0A, struct efi_querycapsulecapabilities)

#define EFI_RUNTIME_RESET_SYSTEM \
	_IOW('p', 0x0B, struct efi_resetsystem)

#define EFI_RUNTIME_GET_SUPPORTED_MASK \
	_IOR('p', 0x0C, unsigned int)

#endif /* _FWTS_EFI_RUNTIME_H_ */
