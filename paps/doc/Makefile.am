DOXYGEN = doxygen

all: doxygen-html

doxygen-html: Doxyfile
	$(DOXYGEN) $<

clean-generic:
	rm -rf doxygen-html

EXTRA_DIST = Doxyfile example-output.png mainpage.h

htmldir = share/doc/libpaps/html
install-data-hook:
	mkdir -p $(prefix)/$(htmldir)
	cp -dpR doxygen-html/* $(prefix)/$(htmldir)
