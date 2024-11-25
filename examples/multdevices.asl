DefinitionBlock ("multdevices.aml", "DSDT", 1, "OEMID ", "TABLEID ", 0x00000000)
{
    Scope (_SB)
    {
        Device (PCI0)
        {
            Device (_SUB)
            {
                Name (_HID, EisaId ("PNP0A03"))
            }
            Name (_HID, EisaId ("PNP0A04"))
        }
        Device (PCI1)
        {
            Name (_HID, EisaId ("PNP0A03"))
        }
    }
}
