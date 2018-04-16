

.PHONY=domake
curPath=$(abspath ./)
domake:
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc
	ls $(curPath)/lib  $(curPath)/include  || \
	(cd src-1.2.11 &&( \
		sh ./configure --sharedlibdir=$(curPath)/lib --libdir=$(curPath)/staticlib --includedir=$(curPath)/include --prefix=$(curPath)/pref && \
		make && \
		make install\
	))

clean:
	cd src-1.2.11 && make clean
	-rm -r lib include
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc clean
