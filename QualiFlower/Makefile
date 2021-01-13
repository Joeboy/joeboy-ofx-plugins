CXXFLAGS = -I/home/joe/building/openfx/include -Wall
OPTIMIZER = -O3
BUNDLE_DIRNAME = QualiFlower-0.1.ofx.bundle

bundle : qualiflower.dso
	mkdir -p $(BUNDLE_DIRNAME)/Contents/Linux-x86-64
	mkdir -p $(BUNDLE_DIRNAME)/Contents/Resources
	cp qualiflower.dso $(BUNDLE_DIRNAME)/Contents/Linux-x86-64/QualiFlower-0.1.ofx

qualiflower.dso : qualiflower.o
	$(CXX) -shared qualiflower.o -o qualiflower.dso
#	strip -fhls qualiflower.dso

%.o : %.cpp
	$(CXX) -fPIC $(CXXFLAGS) -c -o $@ $^

clean :
	rm -f *.o *.dso
	rm -f $(BUNDLE_DIRNAME)/Contents/Linux-x86-64/*.ofx

test : bundle
#	OFX_PLUGIN_PATH=./ sam-plugins
	rm -rf out/*.png
	OFX_PLUGIN_PATH=./ sam do reader in/#####.png // joeboy:qualiflower // writer out/#####.png
	djv_view out/00001.png

install : bundle
	cp -r $(BUNDLE_DIRNAME) /usr/local/OFX/
