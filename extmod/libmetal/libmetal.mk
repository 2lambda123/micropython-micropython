# Makefile directives for libmetal

# libmetal is intended to run through a pre-processor (as part of its CMake-based build system).
# This replicates the basic functionality of the pre-processor, including adding a "micropython"
# platform that is almost identical to the built-in "generic" platform with a few small changes
# provided by the files in extmod/libmetal.
$(BUILD)/openamp: $(BUILD)
	$(MKDIR) -p $@

$(BUILD)/openamp/metal: $(BUILD)/openamp
	$(MKDIR) -p $@

$(BUILD)/openamp/metal/config.h: $(BUILD)/openamp/metal $(TOP)/$(LIBMETAL_DIR)/lib/config.h
	@$(ECHO) "GEN $@"
	@for file in $(TOP)/$(LIBMETAL_DIR)/lib/*.c $(TOP)/$(LIBMETAL_DIR)/lib/*.h; do $(SED) -e "s/@PROJECT_SYSTEM@/micropython/g" -e "s/@PROJECT_PROCESSOR@/arm/g" $$file > $(BUILD)/openamp/metal/$$(basename $$file); done
	$(MKDIR) -p $(BUILD)/openamp/metal/processor/arm
	@$(CP) $(TOP)/$(LIBMETAL_DIR)/lib/processor/arm/*.h $(BUILD)/openamp/metal/processor/arm
	$(MKDIR) -p $(BUILD)/openamp/metal/compiler/gcc
	@$(CP) $(TOP)/$(LIBMETAL_DIR)/lib/compiler/gcc/*.h $(BUILD)/openamp/metal/compiler/gcc
	$(MKDIR) -p $(BUILD)/openamp/metal/system/micropython
	@$(CP) $(TOP)/$(LIBMETAL_DIR)/lib/system/generic/*.c $(TOP)/$(LIBMETAL_DIR)/lib/system/generic/*.h $(BUILD)/openamp/metal/system/micropython
	@$(CP) $(TOP)/extmod/libmetal/metal/system/micropython/* $(BUILD)/openamp/metal/system/micropython
	@$(CP) $(TOP)/extmod/libmetal/metal/config.h $(BUILD)/openamp/metal/config.h

# libmetal's source files.
SRC_LIBMETAL_C := $(addprefix $(BUILD)/openamp/metal/,\
	device.c \
	dma.c \
	init.c \
	io.c \
	irq.c \
	log.c \
	shmem.c \
	softirq.c \
	version.c \
	system/micropython/condition.c \
	system/micropython/device.c \
	system/micropython/io.c \
	system/micropython/irq.c \
	system/micropython/shmem.c \
	system/micropython/time.c \
	)

# These files are generated by the rule above (along with config.h), so we need to make the .c
# files depend on that step -- can't use the .o files as the target because the .c files don't
# exist yet.
$(SRC_LIBMETAL_C): $(BUILD)/openamp/metal/config.h

# Qstr processing will include the generated libmetal headers, so add them as a qstr requirement.
QSTR_GLOBAL_REQUIREMENTS += $(BUILD)/openamp/metal/config.h
