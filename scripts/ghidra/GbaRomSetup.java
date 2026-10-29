// Ghidra headless pre-script: prepare a raw GBA ROM import.
//
// The ROM is imported with BinaryLoader as ARM:LE:32:v4t at 0x08000000. This script
// adds the GBA's RAM/IO regions (so references resolve), and seeds only the
// cartridge entry point (ARM, 0x08000000). Everything else is left to Ghidra's own
// auto-analysis, so the function list it produces is an independent opinion for
// scripts/discover_functions.py.
//
// @category GBA
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;

public class GbaRomSetup extends GhidraScript {
    private void addRam(String name, long start, long size, boolean execute) throws Exception {
        Memory mem = currentProgram.getMemory();
        Address a = toAddr(start);
        if (mem.getBlock(a) != null) return;
        var block = mem.createUninitializedBlock(name, a, size, false);
        block.setRead(true);
        block.setWrite(true);
        block.setExecute(execute);
    }

    @Override
    public void run() throws Exception {
        var rom = currentProgram.getMemory().getBlock(toAddr(0x08000000L));
        rom.setName("ROM");
        rom.setWrite(false);
        rom.setExecute(true);
        addRam("EWRAM", 0x02000000L, 0x40000L, true);
        addRam("IWRAM", 0x03000000L, 0x8000L, true);
        addRam("IO", 0x04000000L, 0x400L, false);
        addRam("PALETTE", 0x05000000L, 0x400L, false);
        addRam("VRAM", 0x06000000L, 0x18000L, false);
        addRam("OAM", 0x07000000L, 0x400L, false);

        // ARM-mode entry (b 0x080000C0); TMode defaults to 0 (ARM) for ARM:LE:32:v4t.
        Address entry = toAddr(0x08000000L);
        disassemble(entry);
        createFunction(entry, "_start");
        currentProgram.getSymbolTable().addExternalEntryPoint(entry);

        setAnalysisOption(currentProgram, "ARM Aggressive Instruction Finder", "true");
    }
}
