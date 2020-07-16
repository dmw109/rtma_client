#!/usr/bin/make -f
MAKEFILE_DIR := $(dir $(firstword $(MAKEFILE_LIST)))

# Directory make command was launched in
ROOT_DIR 	:= $(CURDIR)

PROJECTDIR 	:= $(shell pwd)
PROJECT 	:= $(notdir $(PROJECTDIR))

SRCDIR      := $(PROJECTDIR)/src
INCDIR      := $(PROJECTDIR)/include
LIBDIR      := $(PROJECTDIR)/lib
BUILDDIR    := $(PROJECTDIR)/obj
TARGETDIR   := $(PROJECTDIR)/bin
RESDIR      := $(PROJECTDIR)/res

TARGET_NAME	:= librtma_c.so
TARGET		:= $(TARGETDIR)/$(TARGET_NAME)

SRCEXT      := c
DEPEXT      := d
OBJEXT      := o
LIBEXT      := so
BINEXT		:= 

SRCS     	:= $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJS     	:= $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SRCS:.$(SRCEXT)=.$(OBJEXT)))

# External Tools
SHELL 		:= /bin/bash
INSTALL 	:= /usr/bin/install
PREFIX 		:= ~/.local
BINPREFIX 	:=
BINDIR 		:= $(PREFIX)/bin
BINTARGET	:= $(BINDIR)/$(BINPREFIX)$(TARGET)

CC 			:= gcc
CDEBUG 		:= -g
DEFS 		:= -D _UNIX_C
INC         := -I$(INCDIR) -I/usr/local/include
INCDEP      := -I$(INCDIR)
LIB     	:=

CFLAGS 		:= $(CDEBUG) $(DEFS)
LDFLAGS 	:= -g

#Defauilt Make
all: resources TAGS $(TARGET)

#Remake
remake: cleaner all

#Copy Resources from Resources Directory to Target Directory
.ONESHELL:
resources: directories
	@
	if ! [ -z "$$(ls -A $(RESDIR))" ]; then
		cp $(RESDIR)/* $(TARGETDIR)
	fi

#Make the Directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@rm -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@rm -rf $(TARGETDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJS:.$(OBJEXT)=.$(DEPEXT))

#Link
$(TARGET): $(OBJS)
	@echo "Compiling...$(TARGET)"
	@$(CC) -shared -o $(TARGET) $^ $(LIB)
	@echo "DONE!"

#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@echo 'Compiling object files...'
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(INCDEP) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

install: all
	$(INSTALL) $(TARGET) $(BINTARGET)

.ONESHELL:
uinstall:
	@
	if ! [ -z "$(BINTARGET)" ]; then
		rm $(BINTARGET)
	fi

run: all
	$(BINDIR)/$(TARGET)

TAGS: $(SRCS)
	@ctags $(SRCS)

#Non-File Targets
.PHONY: all remake clean cleaner resources run
