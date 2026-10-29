// Ghidra headless post-script: write every function Ghidra found as
//   <entry hex> <thumb|arm> <body min hex> <body max hex> <name>
// to the file named by the first script argument, then `# end <n> functions`.
// The file is written under a temporary name and renamed when complete, so an
// interrupted run never leaves a truncated export behind.
//
// @category GBA
import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;

import java.io.PrintWriter;
import java.math.BigInteger;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

public class ExportFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        Path out = Path.of(getScriptArgs()[0]);
        Path tmp = Path.of(out + ".tmp");
        Register tmode = currentProgram.getRegister("TMode");
        int n = 0;
        try (PrintWriter w = new PrintWriter(tmp.toFile())) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                var entry = f.getEntryPoint();
                BigInteger t = currentProgram.getProgramContext().getValue(tmode, entry, false);
                String mode = (t != null && t.intValue() == 1) ? "thumb" : "arm";
                w.printf("%08X %s %08X %08X %s%n", entry.getOffset(), mode,
                    f.getBody().getMinAddress().getOffset(), f.getBody().getMaxAddress().getOffset(), f.getName());
                n++;
            }
            w.printf("# end %d functions%n", n);
        }
        Files.move(tmp, out, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        println("Exported " + n + " functions to " + out);
    }
}
