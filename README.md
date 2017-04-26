Modified code example from class 2017 mobile processor.
This code is for educational purpose only.
Do not re-distribute it with any other purposes.
Do not remove or modify this readme file.

# single_cycle_mips_example
This implements functional behavior of single-cycle MIPS microprocessor.
All instructions are completed in a cycle.
-. Implements 31 Integer instructions in the MIPS green sheet.
-. Execute MIPS binary, begin from 0, memory size is fixed (32KB).
-. You can give input using mips-cross compiler
-. mips-gcc with options "-c -mips1 " for compilation, and then
-. mips-objcopy with options "-O binary -j .text" for input binary
-. if you have function call, then you have to hand-carve binary to supply accurte jal address
-. Counts the total number of cycles.
-. Prints out the result (return value)
