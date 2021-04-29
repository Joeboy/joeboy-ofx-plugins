UNAME_SYSTEM := $(shell uname -s)

# In theory the support libs are open source, but I'm not sure where the required versions are,
# other than being distributed with Davinci Resolve
CXXFLAGS = -fvisibility=hidden -Wno-deprecated -I/opt/resolve/Developer/OpenFX/Support/include -I/opt/resolve/Developer/OpenFX/OpenFX-1.4/include

ifeq ($(UNAME_SYSTEM), Linux)
	CXXFLAGS += -fPIC
	CUDAPATH ?= /usr
	NVCC ?= ${CUDAPATH}/bin/nvcc
	NVCCFLAGS = --compiler-options="-fPIC"
	LDFLAGS = -shared -fvisibility=hidden -L${CUDAPATH}/lib64 -lcuda -lcudart_static
	BUNDLE_DIR = QualiFlower-0.2.ofx.bundle/Contents/Linux-x86-64
	CUDA_OBJ = CudaKernel.o
else
	LDFLAGS = -bundle -fvisibility=hidden -F/Library/Frameworks -framework OpenCL -framework Metal -framework AppKit
	BUNDLE_DIR = QualiFlower-0.2.ofx.bundle/Contents/MacOS/
endif

QualiFlower.ofx: qualiflower.o ${CUDA_OBJ} ofxsCore.o ofxsImageEffect.o ofxsInteract.o ofxsLog.o ofxsMultiThread.o ofxsParams.o ofxsProperty.o ofxsPropertyValidation.o
	$(CXX) $^ -o $@ $(LDFLAGS)
	mkdir -p $(BUNDLE_DIR)
	cp QualiFlower.ofx $(BUNDLE_DIR)/QualiFlower-0.2.ofx

CudaKernel.o: CudaKernel.cu
	${NVCC} -c $< $(NVCCFLAGS)

%.o: ../Support/Library/%.cpp
	$(CXX) -c $< $(CXXFLAGS)

clean:
	rm -f *.o *.ofx
	rm -fr QualiFlower-0.2.ofx.bundle

install: QualiFlower.ofx
	cp -fr QualiFlower-0.2.ofx.bundle /Library/OFX/Plugins

test: QualiFlower.ofx
	OFX_PLUGIN_PATH=/home/joe/building/joeboy-ofx-plugins/QualiFlowerx:/home/joe/building/joeboy-ofx-plugins/QualiFlower:/home/joe/building/joeboy-ofx-plugins/GainPlugin /opt/resolve/bin/resolve