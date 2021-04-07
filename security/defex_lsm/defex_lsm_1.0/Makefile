#
# Makefile for the Defex
#

# Features to Enable
PED_ENABLE=true
SAFEPLACE_ENABLE=true

# caches to enable
CACHES_ENABLE=true

obj-y := defex_lsm.o
obj-y += defex_catch_list.o
obj-y += defex_ht.o
obj-y += defex_config.o
obj-y += defex_sysfs.o
obj-y += defex_procs.o
obj-y += defex_catch_list_compat.o
obj-y += defex_rules.o

ifeq ($(PED_ENABLE), true)
    obj-y += defex_priv.o
    EXTRA_CFLAGS += -DDEFEX_PED_ENABLE
endif

ifeq ($(SAFEPLACE_ENABLE), true)
    obj-y += defex_safeplace.o
    EXTRA_CFLAGS += -DDEFEX_SAFEPLACE_ENABLE
endif

ifneq (,$(wildcard $(src)/pack_rules.c))
    EXTRA_CFLAGS += -DDEFEX_USE_PACKED_RULES
    hostprogs-y := pack_rules
    clean-files := $(src)/defex_packed_rules.inc

    $(obj)/defex_sysfs.o: $(obj)/pack_rules $(obj)/defex_packed_rules.inc

    quiet_cmd_pack = PACK    $<
          cmd_pack = $(obj)/pack_rules $< $@

    SOURCE_RULES := $(src)/defex_rules_reduced.c
    ifeq (,$(wildcard $(SOURCE_RULES)))
        SOURCE_RULES := $(src)/defex_rules.c
    endif

    $(src)/defex_packed_rules.inc: $(SOURCE_RULES) $(obj)/pack_rules
	$(call cmd,pack)
endif

ifeq ($(CACHES_ENABLE), true)
    obj-y += defex_caches.o
    EXTRA_CFLAGS += -DDEFEX_CACHES_ENABLE
endif

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
    obj-y += defex_debug.o
    EXTRA_CFLAGS += -DDEFEX_PERMISSIVE_SP
    EXTRA_CFLAGS += -DDEFEX_DEBUG_ENABLE
    EXTRA_CFLAGS += -DDEFEX_SYSFS_ENABLE
endif

ifeq ($(SEC_BUILD_OPTION_VTS), true)
$(warning [DEFEX] VTS true $(SEC_BUILD_OPTION_VTS))
EXTRA_CFLAGS += -DDEFEX_PERMISSIVE_SP
EXTRA_CFLAGS += -DDEFEX_PERMISSIVE_PED
else
$(warning [DEFEX] VTS false $(SEC_BUILD_OPTION_VTS))
endif
