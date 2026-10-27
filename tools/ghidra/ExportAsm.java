// Ekspor disassembly setiap fungsi ke <outdir>/<alamat>.asm (Ghidra headless postScript).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        out.mkdirs();
        Listing listing = currentProgram.getListing();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isExternal()) continue;
            try (PrintWriter w = new PrintWriter(new File(out, "0x" + Long.toHexString(f.getEntryPoint().getOffset()) + ".asm"))) {
                w.println("; " + f.getName() + " " + f.getSignature());
                for (Instruction ins : listing.getInstructions(f.getBody(), true))
                    w.println(ins.getAddress() + ": " + ins);
            }
        }
    }
}
