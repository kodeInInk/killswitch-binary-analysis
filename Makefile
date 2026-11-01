.DEFAULT_GOAL := help

.PHONY: help build audit test clean

TARGETS = demo/obedient demo/stubborn demo/orphan demo/runaway demo/leashed demo/sneaky demo/devserver.sh

help:
	@echo "make build   compile the auditor and the demo programs"
	@echo "make audit   run the audit on all demo targets"
	@echo "make test    check the audit against expected.txt"
	@echo "make clean   remove binaries and results"

build:
	@echo "Compiling..."
	gcc -Wall -o audit audit.c
	$(MAKE) -s -C demo

audit: build
	@echo "Starting audit..."
	mkdir -p results
	./audit $(TARGETS) | tee results/audit.txt

test: build
	@echo "Running tests..."
	./audit $(TARGETS) | diff - expected.txt && echo "All tests passed"

clean:
	@echo "Cleaning up..."
	$(MAKE) -s -C demo clean
	rm -rf audit results
