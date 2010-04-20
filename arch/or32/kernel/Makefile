# Linux/or32 Makefile
#
# Makefile for the linux kernel.
#
#

extra-y	:= head.o vmlinux.lds init_task.o

obj-y	:= setup.o misc.o idle.o or32_ksyms.o process.o \
	   traps.o time.o irq.o entry.o ptrace.o signal.o sys_or32.o su.o

obj-$(CONFIG_MODULES)		+= module.o

clean:
