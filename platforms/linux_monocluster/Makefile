# where is soclib?
#
SOCLIB_DIR=$(shell soclib-cc --getpath)

# name definitions
#
SOCLIB_DESC=desc.py
SOCLIB_TOP=top.cpp
SIMULATOR_BINARY=system.x
SIMULATOR_CMD=./$(SIMULATOR_BINARY) --nobanner

# extra arguments for compilation
#
#SOCLIB_CC_ARGS=-v
SOCLIB_CC_ARGS+=-b common:mips32

# extra arguments for execution
#
# gdb
# - F: start the simulation in a frozen state so it can be attached with a gdb
#   client
# - X: disable automatic break whenever an exception is caught, the exception
#   handler will be called transparently
# - S: make the simulation stop and wait for a gdb attachment whenever an
#   exception is caught
# - C: dump a trace of every inter-functions branch
# - T: exit the simulator on trap exception
# - Z: same as C but display only function's entrypoint
# - W: disable automatic break whenever a watchpoint is hit, just report it on
#   stderr (watchpoints can be defined using SOCLIB_GDB_WATCH)
SIMULATOR_GDB=
ifeq ("$(origin GDB)", "command line")
    ifeq ($(GDB), 1)
    	SIMULATOR_GDB=SOCLIB_GDB=FCX
    else
    	SIMULATOR_GDB=SOCLIB_GDB=$(GDB)
    endif
endif
# ncpus
SIMULATOR_NCPUS=4
ifeq ("$(origin NCPUS)", "command line")
	SIMULATOR_NCPUS=$(NCPUS)
endif
# vmlinux
SIMULATOR_VMLINUX=./vmlinux
ifeq ("$(origin VMLINUX)", "command line")
	SIMULATOR_VMLINUX=$(VMLINUX)
endif
# dsk
SIMULATOR_DSK=
ifeq ("$(origin DSK)", "command line")
	SIMULATOR_DSK=--dsk $(DSK)
endif
# trace
SIMULATOR_TRACE=
ifeq ("$(origin TRACE)", "command line")
	SIMULATOR_TRACE=--trace $(TRACE)
endif
# ncycles
SIMULATOR_NCYCLES=
ifeq ("$(origin NCYCLES)", "command line")
	SIMULATOR_NCYCLES=--ncycles $(NCYCLES)
endif

# recipes
all: $(SIMULATOR_BINARY)

$(SIMULATOR_BINARY): $(SOCLIB_DESC) $(SOCLIB_TOP)
	soclib-cc $(SOCLIB_CC_ARGS) -P -p $(SOCLIB_DESC) -o $(SIMULATOR_BINARY)

run_tsar_boot: all tsar_boot.bin
	$(SIMULATOR_GDB) $(SIMULATOR_CMD) --ncpus $(SIMULATOR_NCPUS) --rom tsar_boot.bin $(SIMULATOR_DSK) $(SIMULATOR_TRACE) $(SIMULATOR_NCYCLES)

run_dummy_boot: all
	$(SIMULATOR_GDB) $(SIMULATOR_CMD) --ncpus $(SIMULATOR_NCPUS) --rom $(SIMULATOR_VMLINUX) $(SIMULATOR_DSK) --dummy-boot $(SIMULATOR_TRACE) $(SIMULATOR_NCYCLES)

cscope.out:
	soclib-cc -p $(SOCLIB_DESC) --tags

clean:
	soclib-cc -P -p $(SOCLIB_DESC) -x -o $(SIMULATOR_BINARY)
	rm -f $(SIMULATOR_BINARY) *.o vci*

.PHONY: $(SIMULATOR_BINARY)
