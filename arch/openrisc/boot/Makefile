

obj-y += linked_dtb.o

# Where the DTS files live
dtstree         := $(srctree)/$(src)/dts

# Ensure system.dtb exists
$(obj)/linked_dtb.o: $(obj)/system.dtb

# Rule to build device tree blobs
DTC = $(objtree)/scripts/dtc/dtc

# Rule to build device tree blobs
quiet_cmd_dtc = DTC     $@
      cmd_dtc = $(DTC) -O dtb -o $(obj)/$*.dtb -b 0 -p 1024 $(dtstree)/$*.dts

$(obj)/%.dtb: $(dtstree)/%.dts
	$(call if_changed,dtc)

