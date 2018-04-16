

.PHONY=domake
curPath=$(abspath ./)
domake:
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc
	ls $(curPath)/lib  $(curPath)/include  || \
	(cd src-1.2.11 &&( \
		sh ./configure --sharedlibdir=$(curPath)/lib --includedir==$(curPath)/include && \
		make && \
		make install\
	))

clean:
	cd src-1.2.11 && make clean
	-rm -r lib include
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc clean
