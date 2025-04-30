CC := g++
SRCDIR := src
BUILDDIR := build
TARGET := bin/L1simulate
 
SRCEXT := cpp
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
CFLAGS := -g -Wall

LDFLAGS :=
LIB := -pthread
INC := -I $(SRCDIR)

$(TARGET): $(OBJECTS)
	@echo "Linking..."
	@mkdir -p bin
	@echo " $(CC) $^ -o $(TARGET) $(LIB) $(LDFLAGS)"; $(CC) $^ -o $(TARGET) $(LIB) $(LDFLAGS)
	@cp $(TARGET) ./L1simulate # Copy the executable to the root directory

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo " $(CC) $(CFLAGS) $(INC) -c -o $@ $<"; $(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	@echo "Cleaning..."; 
	@echo " $(RM) -r $(BUILDDIR) $(TARGET) L1simulate"; $(RM) -r $(BUILDDIR) $(TARGET) L1simulate

.PHONY: clean

run: $(TARGET)
	@echo "Running the program..."
	@./L1simulate $(ARGS)

.PHONY: run
