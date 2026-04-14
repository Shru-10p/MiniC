.PHONY: help configure build rebuild clean distclean ir bc tokens ast native run lli

BUILD_DIR ?= build
BUILD_TYPE ?= Release
CMAKE ?= cmake
CLANG ?= clang
LLI ?= lli

SRC ?= examples/arith.mc
OUT ?= $(basename $(notdir $(SRC)))
IR ?= $(OUT).ll
BC ?= $(OUT).bc
BIN ?= $(OUT)

MINICC := $(BUILD_DIR)/minicc

help:
	@echo "MiniC Makefile targets"
	@echo "  make configure            Configure CMake in $(BUILD_DIR)"
	@echo "  make build                Build minicc"
	@echo "  make rebuild              Clean-first rebuild"
	@echo "  make clean                Clean build artifacts via CMake"
	@echo "  make distclean            Remove $(BUILD_DIR) and generated outputs"
	@echo "  make ir SRC=examples/arith.mc"
	@echo "  make bc SRC=examples/arith.mc"
	@echo "  make tokens SRC=examples/cond.mc"
	@echo "  make ast SRC=examples/funcs.mc"
	@echo "  make native SRC=examples/arith.mc"
	@echo "  make run SRC=examples/arith.mc"
	@echo "  make lli SRC=examples/arith.mc"

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

rebuild: configure
	$(CMAKE) --build $(BUILD_DIR) --clean-first -j

clean:
	@if [ -d "$(BUILD_DIR)" ]; then $(CMAKE) --build $(BUILD_DIR) --target clean; fi

distclean: clean
	rm -rf $(BUILD_DIR)
	rm -f $(IR) $(BC) $(BIN)

ir: build
	$(MINICC) --emit-ir -o $(IR) $(SRC)

bc: build
	$(MINICC) --emit-bc -o $(BC) $(SRC)

tokens: build
	$(MINICC) --dump-tokens $(SRC)

ast: build
	$(MINICC) --dump-ast $(SRC)

native: ir
	$(CLANG) -o $(BIN) $(IR)

run: native
	./$(BIN)

lli: ir
	$(LLI) $(IR)