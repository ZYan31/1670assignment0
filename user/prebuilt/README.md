# Pre-compiled programs

Place `.elf` files in this directory for pre-compiled user programs.
The makefile will link these files into your `kernel8.img`, just like
your other user programs compiled from C code.

In order to run a pre-built program in your kernel, both the program
and your kernel need to adhere to the Syscall ABI we specify for the
course.  If you have trouble running one of our pre-built programs,
make sure you're following our ABI specification!

These files are ELF files and contain only machine code.  You should
not try to edit them.
