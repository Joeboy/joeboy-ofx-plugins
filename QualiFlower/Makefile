PLUGIN_NAME = QualiFlower
VERSION = 0.2

UNAME_SYSTEM := $(shell uname -s)

# In theory the Support libs are open source, but I'm not sure where the required versions are,
# other than being distributed with Davinci Resolve
OPENFX_PATH := /opt/resolve/Developer/OpenFX

CXXFLAGS = -fvisibility=hidden -Wno-deprecated -I$(OPENFX_PATH)/Support/include -I$(OPENFX_PATH)/OpenFX-1.4/include


ifeq ($(UNAME_SYSTEM), Linux)
	CXXFLAGS += -fPIC
	CUDAPATH ?= /usr
	NVCC ?= ${CUDAPATH}/bin/nvcc
	NVCCFLAGS = --compiler-options="-fPIC"
	LDFLAGS = -shared -fvisibility=hidden -L${CUDAPATH}/lib64 -lcuda -lcudart_static
	BUNDLE_DIR =$(PLUGIN_NAME)-$(VERSION).ofx.bundle/Contents/Linux-x86-64
	CUDA_OBJ = CudaKernel.o
else
#   Completely untested and probably won't work:
	LDFLAGS = -bundle -fvisibility=hidden -F/Library/Frameworks -framework OpenCL -framework Metal -framework AppKit
	BUNDLE_DIR = $(PLUGIN_NAME)-$(VERSION).ofx.bundle/Contents/MacOS/
endif

$(PLUGIN_NAME).ofx: qualiflower.o ${CUDA_OBJ} ofxsCore.o ofxsImageEffect.o ofxsInteract.o ofxsLog.o ofxsMultiThread.o ofxsParams.o ofxsProperty.o ofxsPropertyValidation.o
	$(CXX) $^ -o $@ $(LDFLAGS)
	mkdir -p $(BUNDLE_DIR)
	cp $(PLUGIN_NAME).ofx $(BUNDLE_DIR)/$(PLUGIN_NAME)-$(VERSION).ofx

CudaKernel.o: CudaKernel.cu
	${NVCC} -c $< $(NVCCFLAGS)

%.o: $(OPENFX_PATH)/Support/Library/%.cpp
	$(CXX) -c $< $(CXXFLAGS)

clean:
	rm -f *.o *.ofx
	rm -fr $(PLUGIN_NAME)-$(VERSION).ofx.bundle

install: $(PLUGIN_NAME).ofx
	cp -fr $(PLUGIN_NAME)-$(VERSION).ofx.bundle /Library/OFX/Plugins

test: $(PLUGIN_NAME).ofx
	OFX_PLUGIN_PATH=$(CURDIR) /opt/resolve/bin/resolve