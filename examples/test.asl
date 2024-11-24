DefinitionBlock ("test.aml", "DSDT", 1, "OEMID ", "TABLEID ", 0x00000000) // Header
{
    Scope (_SB)  // namespace
    {
        Device (PCI0) // namespace + object
        {
            Device (NVME)
            {
                Name (Fooo, ^.Fooo) // PCI0.NVME.Fooo
                Name (_HID, EisaId ("PNP0000"))
            }
            Name (Fooo, NVME.Fooo)

            Name (_HID, EisaId ("PNP0A03"))
        }
        Device (PCI1)
        {
            Name (_HID, EisaId ("PNP0A03"))
        }
    }
}
