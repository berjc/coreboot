ifeq ($(CONFIG_NORTHBRIDGE_AMD_AMDK8),y)

ramstage-y += northbridge.c
ramstage-y += misc_control.c
ramstage-y += get_sblk_pci1234.c
ramstage-$(CONFIG_HAVE_ACPI_TABLES) += acpi.c
ifeq ($(CONFIG_K8_REV_F_SUPPORT),y)
romstage-$(CONFIG_HAVE_ACPI_RESUME) += exit_from_self.c
endif

# Enable this if you want to check the values of the PCI routing registers.
# Call show_all_routes() anywhere amdk8.h is included.
#ramstage-y += util.c

# Not sure what to do with these yet. How did raminit_test even work?
# Should be a target in -y form.
#if CONFIG_K8_REV_F_SUPPORT
#
#makerule raminit_test
#	depends "$(TOP)/src/northbridge/amd/amdk8/raminit_test.c"
#	depends "$(TOP)/src/northbridge/amd/amdk8/raminit_f.c"
#	action "$(HOSTCC) $(HOSTCFLAGS) $(CPUFLAGS) -Wno-unused-function -I$(TOP)/src/include -g  $< -o $@"
#end
#
#else
#
#makerule raminit_test
#	depends "$(TOP)/src/northbridge/amd/amdk8/raminit_test.c"
#	depends "$(TOP)/src/northbridge/amd/amdk8/raminit.c"
#	action "$(HOSTCC) $(HOSTCFLAGS) $(CPUFLAGS) -Wno-unused-function -I$(TOP)/src/include -g  $< -o $@"
#end
#
#end
#

endif
