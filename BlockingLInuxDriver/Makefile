TARGET_MODULE:=blocking-module

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := main.o device_file.o
	obj-m := $(TARGET_MODULE).o

else
	BUILDSYSTEM_DIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)

all:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean

load:
	sudo insmod ./$(TARGET_MODULE).ko

unload:
	sudo rmmod $(subst -,_,$(TARGET_MODULE)) 2>/dev/null || true

run: all unload load
	@echo ""
	@echo "=============================================="
	@echo "        BLOCKING vs BUSY-WAIT TEST"
	@echo "=============================================="
	@echo ""

	# ---------- BLOCKING ----------
	@echo "Setting BLOCKING mode..."
	@echo 0 | sudo tee /dev/blocking-driver > /dev/null

	@echo "Running Blocking Read..."
	@/usr/bin/time -f "%E %P %U %S" -o block.tmp \
		sh -c "sudo cat /dev/blocking-driver > /dev/null"

	# ---------- BUSY WAIT ----------
	@echo "Setting BUSY-WAIT mode..."
	@echo 1 | sudo tee /dev/blocking-driver > /dev/null

	@echo "Running Busy-Wait Read..."
	@/usr/bin/time -f "%E %P %U %S" -o busy.tmp \
		sh -c "sudo cat /dev/blocking-driver > /dev/null"

	# ---------- TABLE ----------
	@echo ""
	@echo "================ RESULT TABLE ================"
	@printf "%-20s %-12s %-8s %-10s %-10s\n" "Mode" "Elapsed" "CPU" "User(s)" "System(s)"
	@printf "%-20s %-12s %-8s %-10s %-10s\n" "--------------------" "------------" "--------" "----------" "----------"

	@read e c u s < block.tmp; \
	printf "%-20s %-12s %-8s %-10s %-10s\n" "Blocking Read" $$e $$c $$u $$s

	@read e c u s < busy.tmp; \
	printf "%-20s %-12s %-8s %-10s %-10s\n" "Busy-Wait Read" $$e $$c $$u $$s

	@echo "=============================================="
	@echo ""

	@rm -f block.tmp busy.tmp

endif