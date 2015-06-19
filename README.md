sm5emu
======

Emulator for Sharp SM5 MCU
--------------------------

sm5emu is a lousy emulator for Sharp's 4-bit SM5 microcontroller.
Commonly found in pocket calculators and electronic toys from the 90's,
the SM5 forms the core of the Nintendo 64 CIC copy protection chip.

Features
--------

sm5emu has the following whiz-bang features:
 
 - built-in debugger
 - memory viewer / poke
 - code breakpoints
 - memory breakpoints

Running
-------

Launch sm5emu by providing it with the image of an SM5 binary. The
emulator launches in debugger mode, and output follows:

    0.00 : lbmx 2
      PC=0.00 A=0 X=0 BM=0 BL=0 SB=00 C=0 SP=0 skip=0
      P0=0 P1=0 P2=0 hiz=1   cycle=0 div=0
    >

The first line shows the PC, registers, and whether the instruction
displayed will be skipped. The second line shows the status of ports,
internal clock cycle, and div (used on some SM5 variants).

Pressing enter executes the current instruction and the debugger
displays the next instruction to be executed. You can see that ```BM```
is now 2 as a result of running the previous instruction:

    0.01 : lblx 2
      PC=0.01 A=0 X=0 BM=2 BL=0 SB=00 C=0 SP=0 skip=0
      P0=0 P1=0 P2=0 hiz=1   cycle=1 div=0
    >

Debugging
---------

The debugger supports the following commands:

    <enter> - step (execute current instruction)
    p - print state
    r - run
    t - toggle trace (display state while running)
    q - quit

    m - display memory
    sp - display stack pointer

    b <page> <addr> - break
    mb <addr> [<end addr>] - set memory breakpoint
    cb - clear breakpoint
    cmb - clear memory breakpoint
    hiz - toggle break on Hi-Z

    skip - toggle skip
    poke <addr> <value> - poke into memory
    port <number> <value> - set port data

Wishlist
--------

sm5emu is pretty barebones. Here's a list of stuff I wish it had:

 - overhaul of port system
  - better input simulation
  - output viewer
 - state saving and rewind
 - better documentation
 - some kind of GUI (maybe ncurses?)

Author
------

Mike Ryan wrote this emulator in 2014 and 2015 as part of an effort to
reverse engineer the Nintendo 64 CIC copy protection chip.
