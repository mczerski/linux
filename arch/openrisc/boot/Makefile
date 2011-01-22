

obj-y += linked_dtb.o

obj-y += system.dtb.o
#obj-y += aac.dtb.o

clean-files := *.dtb.S

#DTC_FLAGS ?= -p 1024

$(obj)/%.dtb: $(src)/dts/%.dts
	$(call cmd,dtc)

