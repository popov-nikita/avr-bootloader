all:

targets    := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
top_mkfile := $(realpath $(firstword $(MAKEFILE_LIST)))
ifeq ($(top_mkfile),)
    $(error realpath error)
endif
src_root   := $(dir $(top_mkfile))

empty    :=
space    := $(empty) $(empty)
quote    := '
#' # Work around syntax highlighting error
shell_esq = $(quote)$(subst $(quote),$(quote)\$(quote)$(quote),$(1))$(quote)

ifeq ($(origin O),command line)

# Cancel command-line variables
MAKEOVERRIDES :=

$(targets):
	@cd $(call shell_esq,$(O)) && $(MAKE) --no-print-directory -f $(top_mkfile) $(@)

.PHONY: $(targets)

else

# Ok, normal flow goes here
__chk_dir   = test -d $(1) || mkdir -p $(1)
chk_dir     = $(call __chk_dir,$(dir $(1)))
chk_tgt_dir = $(call chk_dir,$(@))
# We produce dependency files as side effect of compilation process
dep_file    = $(basename $(@)).d

program_elf      := avr-bld.elf
program_elf_orig := avr-bld.elf.original
program_ihex     := avr-bld.ihex
__link_lds       := tools/link.lds
link_lds         := $(src_root)$(__link_lds)
fix_relocs_c     := $(src_root)tools/elf-fix-relocs.c
fix_relocs       := ./tools/elf-fix-relocs
avr_upldr_c      := $(src_root)tools/avr-uploader.c
avr_upldr        := ./tools/avr-uploader

src_map      := $(src_root)source-files.map
__src_files  := $(shell cat $(src_map))
__c_srcs     := $(filter %.c,$(__src_files))
__a_srcs     := $(filter %.S,$(__src_files))
src_files    := $(addprefix $(src_root),$(__src_files))
c_srcs       := $(addprefix $(src_root),$(__c_srcs))
a_srcs       := $(addprefix $(src_root),$(__a_srcs))
o_files      := $(addsuffix .o,$(basename $(__src_files)))
# Stashed dep files
d_files      := $(patsubst %.c,$(src_root)deps/%.d,$(notdir $(__c_srcs)))

#Fix me
sections     := .head.text .text .data

CC           := avr-gcc
AS           := avr-as
LD           := avr-ld
OBJCOPY      := avr-objcopy
HOSTCC       := gcc

C_INC_FLAGS  := -nostdinc
C_INC_FLAGS  += -isystem $(shell $(CC) -print-file-name=include)
C_INC_FLAGS  += -I $(src_root)include

AFLAGS       := -mmcu=avr5
CFLAGS       := -O2                                                 \
                -Wall                                               \
                -Wextra                                             \
                -Werror                                             \
                -Wstack-usage=32                                    \
                -Wno-misspelled-isr                                 \
                -mmcu=avr5                                          \
                -mstrict-X                                          \
                -fshort-enums                                       \
                -ffreestanding
CFLAGS       += $(C_INC_FLAGS)
LDFLAGS      := -q -T $(link_lds) --gc-sections
OCFLAGS      := -I elf32-avr -O ihex
OCFLAGS      += $(addprefix -j$(space),$(sections))

all:

clean:

gen-deps:

help:

.PHONY: all clean gen-deps help

all: $(program_ihex) $(avr_upldr)

$(program_ihex): $(program_elf)
	@$(chk_tgt_dir)
	$(OBJCOPY) $(OCFLAGS) $(<) $(@)

$(program_elf): $(program_elf_orig) $(fix_relocs)
	@$(chk_tgt_dir)
	$(fix_relocs) $(program_elf_orig) $(@)

$(program_elf_orig): $(o_files) $(link_lds)
	@$(chk_tgt_dir)
	$(LD) $(LDFLAGS) -o $(@) $(o_files)

$(addsuffix .o,$(basename $(__c_srcs))): %.o : $(src_root)%.c
	@$(chk_tgt_dir)
	$(CC) $(CFLAGS) -c -o $(@) $(<) -MMD -MP -MT $(@) -MF $(dep_file)
	sed -i $(call shell_esq,s|$(src_root)|\$$(src_root)|g) $(dep_file)

$(addsuffix .o,$(basename $(__a_srcs))): %.o : $(src_root)%.S
	@$(chk_tgt_dir)
	$(AS) $(AFLAGS) -o $(@) $(<)

-include $(d_files)

$(c_srcs):

$(a_srcs):

$(fix_relocs): $(fix_relocs_c)
	@$(chk_tgt_dir)
	$(HOSTCC) -O2 -Wall -Wextra -o $(@) $(<)

$(fix_relocs_c):

$(link_lds):

$(avr_upldr): $(avr_upldr_c)
	@$(chk_tgt_dir)
	$(HOSTCC) -O2 -Wall -Wextra -o $(@) $(<)

$(avr_upldr_c):

clean-files := $(program_ihex)          \
               $(program_elf)           \
               $(program_elf_orig)      \
               $(fix_relocs)            \
               $(avr_upldr)             \
               $(o_files)               \
               $(addsuffix .d,$(basename $(__c_srcs)))

clean:
	rm -f -v $(clean-files)
	if ! test -f $(__link_lds); then    \
                find . -mindepth 1 -maxdepth 1 -type d -execdir rm -r -f -v $(call shell_esq,{}) \;;    \
        fi

gen-deps:
	$(call chk_dir,$(src_root)deps/)
	find . -name $(call shell_esq,*.d) -type f -execdir mv $(call shell_esq,{}) $(src_root)deps/ -v \;

help:
	@echo $(call shell_esq,Available targets:)
	@echo $(call shell_esq,    all      -- build bootloader image in ihex format. This is default.)
	@echo $(call shell_esq,    clean    -- clean working directory)
	@echo $(call shell_esq,    gen-deps -- copy *.d files into <src tree>/deps directory)
	@echo $(call shell_esq,    help     -- display this message)
	@echo $(call shell_esq,You can set O=<directory> argument in command line.)
	@echo $(call shell_esq,If it is set then build objects are produced inside this directory)
	@echo $(call shell_esq,rather than inside source tree.)

endif
