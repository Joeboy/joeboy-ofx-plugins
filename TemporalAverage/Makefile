CXXFLAGS = -I/home/joe/building/openfx/include
OPTIMIZER = -O3
BUNDLE_DIRNAME = TemporalAverage-0.1.ofx.bundle

bundle : temporalaverage.dso
	mkdir -p $(BUNDLE_DIRNAME)/Contents/Linux-x86-64
	mkdir -p $(BUNDLE_DIRNAME)/Contents/Resources
	cp temporalaverage.dso $(BUNDLE_DIRNAME)/Contents/Linux-x86-64/TemporalAverage-0.1.ofx

temporalaverage.dso : temporalaverage.o
	$(CXX) -shared temporalaverage.o -o temporalaverage.dso
#	strip -fhls temporalaverage.dso

%.o : %.cpp
	$(CXX) -fPIC $(CXXFLAGS) -c -o $@ $^

clean :
	rm -f *.o *.dso
	rm -f $(BUNDLE_DIRNAME)/Contents/Linux-x86-64/*.ofx

test : bundle
#	OFX_PLUGIN_PATH=./ sam-plugins
	rm -rf out/*.png
	OFX_PLUGIN_PATH=./ sam do reader in/#####.png // joeboy:temporalaverage // writer out/#####.png
	djv_view out/00001.png

install : bundle
	cp -r $(BUNDLE_DIRNAME) /usr/local/OFX/
