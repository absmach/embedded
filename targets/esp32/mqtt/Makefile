define platformio
	platformio -f run --target $(1)
endef

run-%:
	$(call platformio,$*)

upload: run-upload
clean: run-clean
program: run-program
uploadfs:run-uploadfs
update: run-update
