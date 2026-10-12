/*
 * PCC-backed power-meter methods must be skipped when FWTS cannot perform
 * the hardware transaction. Other methods must still be evaluated normally.
 * Both fixtures use region-relative COMD offset 8 to exercise ACPICA's PCC
 * handler. Region addresses select PCC channels, not subspace types.
 * Run python3 generate-fixture.py with iasl on PATH to regenerate the dump.
 */
DefinitionBlock ("", "DSDT", 2, "FWTS", "PCCTEST", 1)
{
    External (MISS, IntObj)
    Scope (\_SB)
    {
        Device (PMC0)
        {
            Name (_HID, "ACPI000D")
            Name (_CRS, ResourceTemplate () {})
            /* PCC-backed methods that hold a mutex during evaluation. */
            OperationRegion (PMR0, PCC, 3, 64)
            Field (PMR0, DWordAcc, NoLock, Preserve)
            {
                Offset (8),
                COMD, 32,
                DATA, 32
            }
            Mutex (PMTX, 0)
            Method (SEND, 1, Serialized)
            {
                Acquire (PMTX, 0xFFFF)
                Store (ToBuffer (Arg0), COMD)
                Release (PMTX)
                Return (DATA)
            }
            Method (_GAI, 0, Serialized) { Return (SEND (One)) }
            Method (_GHL, 0, Serialized) { Return (SEND (2)) }
            Method (_PMM, 0, Serialized) { Return (SEND (3)) }
        }
        Device (PMC1)
        {
            Name (_HID, "ACPI000D")
            /* PCC-backed methods without the mutex path. */
            OperationRegion (PMR0, PCC, 0, 64)
            Field (PMR0, WordAcc, NoLock, Preserve)
            {
                Offset (8),
                COMD, 16,
                Offset (16),
                DATA, 32
            }
            Method (_GAI, 0, Serialized) { Store (Buffer (2) {1, 0}, COMD) Return (DATA) }
            Method (_GHL, 0, Serialized) { Store (Buffer (2) {2, 0}, COMD) Return (DATA) }
            Method (_PMM, 0, Serialized) { Store (Buffer (2) {3, 0}, COMD) Return (DATA) }
        }
        Device (GOOD)
        {
            Name (_HID, "ACPI000D")
            Method (_GAI, 0, Serialized) { Return (42) }
            Method (_GHL, 0, Serialized) { Return (43) }
            Method (_PMM, 0, Serialized) { Return (44) }
        }
        Device (BAD0)
        {
            Name (_HID, "ACPI000D")
            Method (_GAI, 0, Serialized) { Return (MISS) }
        }
        Device (BAD1)
        {
            Name (_HID, "ACPI000D")
            OperationRegion (REG0, 0x81, Zero, 16)
            Field (REG0, ByteAcc, NoLock, Preserve) { DATA, 8 }
            Method (_GAI, 0, Serialized) { Return (DATA) }
        }
    }
}
