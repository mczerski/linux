# BK Id: %F% %I% %G% %U% %#%
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds

# Changes for or32 by Matjaz Breskvar
# Based on:
# arch/i386/Makefile

KBUILD_DEFCONFIG := usbeth_defconfig

LDFLAGS         :=
OBJCOPYFLAGS    := -O binary -R .note -R .comment -S
LDFLAGS_vmlinux :=
LIBGCC 		 = $(shell $(CC) $(CFLAGS) -print-file-name=libgcc.a)

#KBUILD_CFLAGS		+= -pipe -g -mredzone=0
KBUILD_CFLAGS		+= -pipe -ffixed-r10
#KBUILD_CFLAGS		+= -fsigned-char -pipe -ffixed-r10 \
#		   -ffixed-r16 -ffixed-r17 -ffixed-r18 -ffixed-r19 \
#		   -ffixed-r20 -ffixed-r21 -ffixed-r22 -ffixed-r23 \
#		   -ffixed-r24 -ffixed-r25 -ffixed-r26 -ffixed-r27 \
#		   -ffixed-r28 -ffixed-r29 -ffixed-r30 -ffixed-r31 \
#                   -Wno-uninitialized

#KBUILD_CFLAGS += $(call cc-option,-mredzone=128)
#KBUILD_CFLAGS += -mredzone=0

ifeq ($(CONFIG_OPENRISC_HAVE_INST_MUL),y)
	KBUILD_CFLAGS += $(call cc-option,-mhard-mul)
else
	KBUILD_CFLAGS += $(call cc-option,-msoft-mul)
endif

ifeq ($(CONFIG_OPENRISC_HAVE_INST_DIV),y)
	KBUILD_CFLAGS += $(call cc-option,-mhard-div)
else
	KBUILD_CFLAGS += $(call cc-option,-msoft-div)
endif



head-y 		:= arch/openrisc/kernel/head.o arch/openrisc/kernel/init_task.o


core-y		+= arch/openrisc/lib/ \
		   arch/openrisc/kernel/ \
		   arch/openrisc/mm/ \
		   arch/openrisc/board/ 
libs-y		+= $(LIBGCC)


core-y		+= arch/openrisc/boot/

all: vmlinux
