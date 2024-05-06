obj-m += mpro.o
mpro-y := mpro_drv.o mpro_flip.o mpro_sysfs.o mpro_modes.o mpro_plane.o mpro_conn.o mpro_fbdev.o
ifeq ($(MAKING_MODULES),1)
-include $(TOPDIR)/Rules.make
endif
