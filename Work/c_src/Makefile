# Quest emulator — pure emulation build (no native translations).
# The previous full build (emulator + translation experiments) is preserved
# in Makefile.full; the translation sources remain on disk for reference.

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -I. -MMD -MP
LDFLAGS  = -lpthread

TARGET   = emulator

SRCS = Launch.cpp \
       hw/PageSet.cpp \
       hw/Memory.cpp \
       hw/Instruction.cpp \
       hw/NovaCompute.cpp \
       hw/EagleInstruction.cpp \
       hw/EagleGeneral.cpp \
       hw/EagleCompute.cpp \
       hw/EagleFloat.cpp \
       hw/EagleStack.cpp \
       hw/EagleSpecial.cpp \
       hw/Decoder.cpp \
       hw/Machine.cpp \
       hw/Mapper.cpp \
       hw/AddressBook.cpp \
       hw/MachineThread.cpp \
       hw/QueueEntry.cpp \
       hw/Lockstep.cpp \
       hw/NativeRegistry.cpp \
       hw/RTStubs.cpp \
       hw/RTBridge.cpp \
       runtime/fill_words.cpp \
       runtime/udiv32.cpp \
       runtime/unsigned_to_char.cpp \
       runtime/o_on.cpp \
       runtime/i_lock.cpp \
       runtime/i_alloc.cpp \
       runtime/t_area.cpp \
       runtime/frames.cpp \
       runtime/o_signal.cpp \
       runtime/lib_error.cpp \
       runtime/o_area.cpp \
       runtime/p_defon.cpp \
       runtime/r_signal.cpp \
       runtime/def_on.cpp \
       runtime/mv_error_handler.cpp \
       runtime/native_error_handler.cpp \
       runtime/check_error_handler.cpp \
       debug/CallStack.cpp \
       debug/Capture.cpp \
       debug/SymbolTable.cpp \
       debug/MemoryDumper.cpp \
       debug/Disassembler.cpp \
       os/AOSVSSymbols.cpp \
       os/Trace.cpp \
       os/ArrayPage.cpp \
       os/MappedPage.cpp \
       os/Utility.cpp \
       os/OSError.cpp \
       os/FS.cpp \
       os/FSFile.cpp \
       os/FSDirectory.cpp \
       os/FSChannel.cpp \
       os/FSConsole.cpp \
       os/FSTerminal.cpp \
       os/FSPagedFile.cpp \
       os/FSStreamedFile.cpp \
       os/OS.cpp \
       os/OSMessage.cpp \
       os/OSProcess.cpp \
       os/OSTask.cpp \
       os/LockstepMediator.cpp \
       os/ProbeSuppressions.cpp \
       os/MirrorPage.cpp \
       os/OSContext.cpp \
       os/OSContextSystem.cpp \
       os/OSContextShared.cpp \
       os/OSContextIPC.cpp \
       os/OSContextFS.cpp \
       os/OSContextTask.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: all clean
