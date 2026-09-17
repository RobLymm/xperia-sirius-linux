# Six separate modules, as upstream builds them: each service file registers
# its own driver.
obj-m += q6voice-common.o
obj-m += q6mvm.o
obj-m += q6cvs.o
obj-m += q6cvp.o
obj-m += q6voice.o
obj-m += q6voice-dai.o
ccflags-y += -I$(src)
