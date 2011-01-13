

obj-y += linked_dtb.o

# Where the DTS files live
dtstree         := $(srctree)/$(src)/dts

# Ensure system.dtb exists
$(obj)/linked_dtb.o: $(obj)/system.dtb

DTC_FLAGS ?= -p 1024

$(obj)/%.dtb: $(src)/dts/%.dts
	$(call cmd,dtc)

