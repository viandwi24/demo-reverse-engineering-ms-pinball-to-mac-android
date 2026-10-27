// Buat fungsi di alamat yang hanya dirujuk vtable, lalu ekspor decompile + asm-nya.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.io.*;

public class MakeFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        out.mkdirs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            Address a = toAddr(args[i]);
            Function f = getFunctionAt(a);
            if (f == null) {
                disassemble(a);
                f = createFunction(a, null);
            }
            if (f == null) { println("gagal: " + args[i]); continue; }
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            try (PrintWriter w = new PrintWriter(new File(out, args[i] + ".c"))) {
                w.println(r.getDecompiledFunction() == null ? "// decompile gagal" : r.getDecompiledFunction().getC());
            }
            try (PrintWriter w = new PrintWriter(new File(out, args[i] + ".asm"))) {
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true))
                    w.println(ins.getAddress() + ": " + ins);
            }
        }
    }
}
