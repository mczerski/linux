#
# Makefile for the linux kernel.
#

obj-y		:= initrd.o 
                   # 8051-mem.o

IMAGE		:= initrd-bb.ext2
MNTDIR          := mnt
BUSYVER    := 1.14.1
BUSYOUTDIR      := ~/Documents/busybox/$(BUSYVER)/busybox-$(BUSYVER)/_install

#IMAGE_8051	:= 8051.mem 

arch/$(ARCH)/support/initrd.o: arch/$(ARCH)/support/$(IMAGE) \
				arch/$(ARCH)/support/tools/bintoc 
	arch/$(ARCH)/support/tools/bintoc .initrd < arch/$(ARCH)/support/$(IMAGE) |$(AS) -o arch/$(ARCH)/support/initrd.o

arch/$(ARCH)/support/initrd-comp.o: arch/$(ARCH)/support/initrd-bb-9-or32.ext2 \
				arch/$(ARCH)/support/tools/bintoc 
	gzip -c arch/$(ARCH)/support/initrd-bb-9-or32.ext2 | arch/$(ARCH)/support/tools/bintoc |$(AS) -o arch/$(ARCH)/support/initrd.o

arch/$(ARCH)/support/8051-mem.o: arch/$(ARCH)/support/$(IMAGE_8051) \
				arch/$(ARCH)/support/tools/bintoc 
	arch/$(ARCH)/support/tools/bintoc .init.mem.8051 < arch/$(ARCH)/support/$(IMAGE_8051) |$(AS) -o arch/$(ARCH)/support/8051-mem.o

initrd-comp.o: initrd-bb-9-or32.ext2 tools/bintoc 
	gzip -c initrd-bb-9-or32.ext2 | tools/bintoc | $(AS) -o initrd.o

arch/$(ARCH)/support/tools/bintoc: arch/$(ARCH)/support/tools/bintoc.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $@.c

rdmount:
	@if [ ! -d $(MNTDIR) ]; then mkdir $(MNTDIR); fi
	@echo; echo "    Mounting RAMdisk image in" $(MNTDIR)/; echo
	sudo mount -o loop $(IMAGE) $(MNTDIR)

rdumount:
	@echo; echo "    Unmounting RAMdisk image from" $(MNTDIR)/; echo
	sudo umount $(MNTDIR)

rdcopy:
	@echo; echo "    Copying BusyBox apps from " $(BUSYOUTDIR); echo
	sudo cp -r $(BUSYOUTDIR)/* $(MNTDIR)

rdupdate: rdmount rdcopy rdumount
	rm -f *.o
