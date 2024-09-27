PROJECT_DIR = /home/ubuntu/MPSCSpecExecProject
DEP_INSTALL_DIR = $(PROJECT_DIR)/install
DEP_BUILD_DIR = $(PROJECT_DIR)/build

all: $(DEP_INSTALL_DIR)/yaml-cpp $(DEP_INSTALL_DIR)/tfhe $(DEP_INSTALL_DIR)/oblivious-SEAL $(DEP_INSTALL_DIR)/osprey
	cd $(PROJECT_DIR)/mage && \
		make clean && \
		make

$(DEP_INSTALL_DIR)/yaml-cpp: $(DEP_INSTALL_DIR) install_deps
	mkdir $@
	mkdir $(DEP_BUILD_DIR)/yaml-cpp
	cd $(PROJECT_DIR)/yaml-cpp && \
		cmake -B $(DEP_BUILD_DIR)/yaml-cpp -DYAML_BUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$@ && \
		cmake --build $(DEP_BUILD_DIR)/yaml-cpp && \
		cmake --install $(DEP_BUILD_DIR)/yaml-cpp

$(DEP_INSTALL_DIR)/tfhe: $(DEP_INSTALL_DIR) install_deps
	mkdir $@
	mkdir $(DEP_BUILD_DIR)/tfhe
	cd $(PROJECT_DIR)/tfhe && \
		cmake -S src -B $(DEP_BUILD_DIR)/tfhe -DCMAKE_INSTALL_PREFIX=$@ && \
		cmake --build $(DEP_BUILD_DIR)/tfhe && \
		cmake --install $(DEP_BUILD_DIR)/tfhe

$(DEP_INSTALL_DIR)/oblivious-SEAL: $(DEP_INSTALL_DIR) install_deps $(DEP_INSTALL_DIR)/osprey
	mkdir $@
	mkdir $(DEP_BUILD_DIR)/oblivious-SEAL
	cd $(PROJECT_DIR)/oblivious-SEAL && \
		cmake -B $(DEP_BUILD_DIR)/oblivious-SEAL -DSEAL_USE_ZLIB=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$@ && \
		cmake --build $(DEP_BUILD_DIR)/oblivious-SEAL && \
		cmake --install $(DEP_BUILD_DIR)/oblivious-SEAL

$(DEP_INSTALL_DIR)/osprey: $(DEP_INSTALL_DIR) install_deps
	cd $(PROJECT_DIR)/osprey && make clean && make

$(DEP_INSTALL_DIR): $(DEP_BUILD_DIR)
	mkdir $@

$(DEP_BUILD_DIR):
	mkdir $@

install_deps:
	sudo apt install -y build-essential clang cmake libssl-dev libaio-dev
	cd $(PROJECT_DIR)/osprey && ./install_deps.sh --install-osprey-deps

clean:
	cd $(PROJECT_DIR)/osprey && make clean
	cd $(PROJECT_DIR)/mage && make clean
	cd $(PROJECT_DIR) && rm -rf $(DEP_BUILD_DIR) && rm -rf $(DEP_INSTALL_DIR)

