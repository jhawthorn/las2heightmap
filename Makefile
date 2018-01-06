
SRCFILES := $(wildcard src/*.cc)
OBJFILES := $(SRCFILES:%.cc=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)
TARGET = las2heightmap
CLEANFILES = $(CLEANFILES) $(DEPFILES) $(OBJFILES) $(TARGET)
CXXFLAGS ?= -O3 -g -Wall `libpng-config --cflags`
LIBS ?= -llas `libpng-config --ldflags`

# User configuration
-include config.mk

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CXX) $(LDFLAGS) -o $@ $(OBJFILES) $(LIBS)

-include $(DEPFILES)

%.o: %.cc Makefile
	$(CXX) $(CFLAGS) -MMD -MP -MT "$*.d" -c -o $@ $<

# Clean
clean:
	$(RM) $(CLEANFILES)

.PHONY: clean

