CXX = clang++
PROJECT_DIR = $(shell pwd)
DEP_INSTALL_DIR = $(PROJECT_DIR)/install
DEP_BUILD_DIR = $(PROJECT_DIR)/build

TOOLS_SOURCE = $(wildcard $(PROJECT_DIR)/tools/*.cpp)
TOOLS_OBJECTS = $(addprefix $(DEP_BUILD_DIR)/tools/,$(foreach file,$(TOOLS_SOURCE),$(notdir $(basename $(file))).o))
TOOLS_EXECUTABLES = $(addprefix $(DEP_INSTALL_DIR)/tools/,$(foreach file,$(TOOLS_SOURCE),$(notdir $(basename $(file)))))

ifndef JOBS
JOBS := $(shell nproc)
endif

.PHONY: all clean install_deps tools yaml-cpp tfhe oblivious-SEAL osprey mage relic emp-tool emp-ot emp-sh2pc show

all: yaml-cpp tfhe oblivious-SEAL osprey mage relic emp-tool emp-ot emp-sh2pc tools

tools: $(TOOLS_EXECUTABLES)

mage: yaml-cpp tfhe oblivious-SEAL osprey
	cd $(PROJECT_DIR)/mage && \
		make clean PROJECT_DIR=$(PROJECT_DIR) BINDIR=$(DEP_INSTALL_DIR)/mage && \
		make PROJECT_DIR=$(PROJECT_DIR) BINDIR=$(DEP_INSTALL_DIR)/mage -j$(JOBS) && \
		make lib PROJECT_DIR=$(PROJECT_DIR) BINDIR=$(DEP_INSTALL_DIR)/mage -j$(JOBS)

$(DEP_INSTALL_DIR)/mage: $(DEP_INSTALL_DIR)
	mkdir $@ || true

$(DEP_INSTALL_DIR)/tools/%: $(DEP_BUILD_DIR)/tools/%.o $(DEP_INSTALL_DIR)/tools yaml-cpp tfhe osprey oblivious-SEAL mage relic emp-tool emp-ot emp-sh2pc
	$(CXX) $< -pthread -laio -lssl -lcrypto -lboost_program_options -lboost_random -lboost_system -lgmp \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/yaml-cpp/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/tfhe/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/mage/lib \
		-Wl,-rpath,$(PROJECT_DIR)/osprey/bin \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/oblivious-SEAL/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/relic/lib \
		-Wl,-rpath,$(DEP_INSTALL_DIR)/emp-tool/lib \
		-L$(PROJECT_DIR)/install/yaml-cpp/lib -l:libyaml-cpp.so \
		-L$(PROJECT_DIR)/install/tfhe/lib -l:libtfhe-spqlios-fma.so \
		-L$(PROJECT_DIR)/install/mage/lib -l:libmage.so \
		-L$(PROJECT_DIR)/osprey/bin -l:libosprey.so \
		-L$(PROJECT_DIR)/install/oblivious-SEAL/lib -l:libseal.so.4.1.1 \
		-L$(PROJECT_DIR)/install/relic/lib -l:librelic.so \
		-L$(PROJECT_DIR)/install/emp-tool/lib -l:libemp-tool.so \
		-o $@

$(DEP_BUILD_DIR)/tools/%.o: $(PROJECT_DIR)/tools/%.cpp $(DEP_BUILD_DIR)/tools yaml-cpp tfhe osprey oblivious-SEAL mage relic emp-tool emp-ot emp-sh2pc
	$(CXX) -std=c++20 -Ofast -DNDEBUG -fPIE -march=native -maes -mrdseed -ggdb3 -pthread \
		-DBOOST_ALL_NO_LIB -DBOOST_SYSTEM_DYN_LINK -DEMP_CIRCUIT_PATH=$(DEP_INSTALL_DIR)/emp-tool/include/emp-tool/circuits/files/ -DEMP_USE_RANDOM_DEVICE -DCKKS \
		-I$(PROJECT_DIR)/install/yaml-cpp/include \
		-I$(PROJECT_DIR)/install/tfhe/include \
		-I$(PROJECT_DIR)/install/oblivious-SEAL/include/SEAL-4.1 \
		-I$(PROJECT_DIR)/mage/src \
		-I$(PROJECT_DIR)/osprey \
		-I$(PROJECT_DIR)/install/relic/include \
		-I$(PROJECT_DIR)/install/emp-tool/include \
		-I$(PROJECT_DIR)/install/emp-ot/include \
		-I$(PROJECT_DIR)/install/emp-sh2pc/include \
		-c $< -o $@

$(DEP_INSTALL_DIR)/tools: $(DEP_INSTALL_DIR)
	mkdir $@ || true

$(DEP_BUILD_DIR)/tools: $(DEP_BUILD_DIR)
	mkdir $@ || true

yaml-cpp: $(DEP_INSTALL_DIR)/yaml-cpp
	cd $(PROJECT_DIR)/yaml-cpp && \
		cmake -B $(DEP_BUILD_DIR)/yaml-cpp -DYAML_BUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ && \
		cmake --build $(DEP_BUILD_DIR)/yaml-cpp -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/yaml-cpp

$(DEP_INSTALL_DIR)/yaml-cpp: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/yaml-cpp
	mkdir $@ || true

$(DEP_BUILD_DIR)/yaml-cpp: $(DEP_BUILD_DIR)
	mkdir $@ || true

tfhe: $(DEP_INSTALL_DIR)/tfhe
	cd $(PROJECT_DIR)/tfhe && \
		cmake -S src -B $(DEP_BUILD_DIR)/tfhe -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ && \
		cmake --build $(DEP_BUILD_DIR)/tfhe -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/tfhe

$(DEP_INSTALL_DIR)/tfhe: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/tfhe
	mkdir $@ || true

$(DEP_BUILD_DIR)/tfhe: $(DEP_BUILD_DIR)
	mkdir $@ || true

oblivious-SEAL: $(DEP_INSTALL_DIR)/oblivious-SEAL
	cd $(PROJECT_DIR)/oblivious-SEAL && \
		cmake -B $(DEP_BUILD_DIR)/oblivious-SEAL -DSEAL_USE_ZLIB=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ -DOSPREY_SOURCE_DIR=$(PROJECT_DIR)/osprey && \
		cmake --build $(DEP_BUILD_DIR)/oblivious-SEAL -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/oblivious-SEAL

relic: $(DEP_INSTALL_DIR)/relic
	cd $(PROJECT_DIR)/relic && \
		cmake -B $(DEP_BUILD_DIR)/relic -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ -DCMAKE_C_COMPILER=gcc-9 -DFB_POLYN=251 && \
		cmake --build $(DEP_BUILD_DIR)/relic -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/relic

$(DEP_INSTALL_DIR)/relic: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/relic
	mkdir $@ || true

$(DEP_BUILD_DIR)/relic: $(DEP_BUILD_DIR)
	mkdir $@ || true

emp-tool: $(DEP_INSTALL_DIR)/emp-tool relic
	cd $(PROJECT_DIR)/emp-tool && \
		cmake -B $(DEP_BUILD_DIR)/emp-tool -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ -DOSPREY_SOURCE_DIR=$(PROJECT_DIR)/osprey -DCMAKE_PREFIX_PATH=$(DEP_INSTALL_DIR)/relic && \
		cmake --build $(DEP_BUILD_DIR)/emp-tool --verbose -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/emp-tool

$(DEP_INSTALL_DIR)/emp-tool: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/emp-tool
	mkdir $@ || true

$(DEP_BUILD_DIR)/emp-tool: $(DEP_BUILD_DIR)
	mkdir $@ || true

emp-ot: $(DEP_INSTALL_DIR)/emp-ot relic emp-tool
	cd $(PROJECT_DIR)/emp-ot && \
		cmake -B $(DEP_BUILD_DIR)/emp-ot -DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ -DCMAKE_PREFIX_PATH="$(DEP_INSTALL_DIR)/relic;$(DEP_INSTALL_DIR)/emp-tool;$(PROJECT_DIR)/osprey" && \
		cmake --build $(DEP_BUILD_DIR)/emp-ot -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/emp-ot

$(DEP_INSTALL_DIR)/emp-ot: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/emp-ot
	mkdir $@ || true

$(DEP_BUILD_DIR)/emp-ot: $(DEP_BUILD_DIR)
	mkdir $@ || true

emp-sh2pc: $(DEP_INSTALL_DIR)/emp-sh2pc osprey relic emp-tool emp-ot
	cd $(PROJECT_DIR)/emp-sh2pc && \
		cmake -B $(DEP_BUILD_DIR)/emp-sh2pc \
		-DCMAKE_INSTALL_PREFIX=$(DEP_INSTALL_DIR)/$@ \
		-DCMAKE_PREFIX_PATH="$(DEP_INSTALL_DIR)/relic;$(DEP_INSTALL_DIR)/emp-tool;$(DEP_INSTALL_DIR)/emp-ot" -DOSPREY_SOURCE_DIR=$(PROJECT_DIR)/osprey && \
		cmake --build $(DEP_BUILD_DIR)/emp-sh2pc --verbose -j$(JOBS) && \
		cmake --install $(DEP_BUILD_DIR)/emp-sh2pc

$(DEP_INSTALL_DIR)/emp-sh2pc: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/emp-sh2pc
	mkdir $@ || true

$(DEP_BUILD_DIR)/emp-sh2pc: $(DEP_BUILD_DIR)
	mkdir $@ || true

$(DEP_INSTALL_DIR)/oblivious-SEAL: $(DEP_INSTALL_DIR) $(DEP_BUILD_DIR)/oblivious-SEAL $(PROJECT_DIR)/osprey/bin/libosprey.so
	mkdir $@ || true

$(DEP_BUILD_DIR)/oblivious-SEAL: $(DEP_BUILD_DIR)
	mkdir $@ || true

$(PROJECT_DIR)/osprey/bin/libosprey.so: $(DEP_INSTALL_DIR)
	cd $(PROJECT_DIR)/osprey && make clean PROJECT_DIR=$(PROJECT_DIR) && make PROJECT_DIR=$(PROJECT_DIR) -j$(JOBS)

$(DEP_INSTALL_DIR): $(DEP_BUILD_DIR)
	mkdir $@ || true

$(DEP_BUILD_DIR):
	mkdir $@ || true

install_deps:
	sudo apt install -y build-essential clang cmake libssl-dev libaio-dev cgroup-tools
	cd $(PROJECT_DIR)/osprey && ./install_deps.sh --install-osprey-deps

clean:
	cd $(PROJECT_DIR)/osprey && make clean PROJECT_DIR=$(PROJECT_DIR)
	cd $(PROJECT_DIR)/mage && make clean PROJECT_DIR=$(PROJECT_DIR)
	cd $(PROJECT_DIR) && rm -rf $(DEP_BUILD_DIR) && rm -rf $(DEP_INSTALL_DIR)

