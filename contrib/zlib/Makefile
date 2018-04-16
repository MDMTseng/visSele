

.PHONY=domake
curPath=$(abspath ./)
domake:
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc
	cd src-1.2.11 &&( make || ( \
		sh ./configure --sharedlibdir=$(curPath)/lib --includedir==$(curPath)/include && \
		make && \
		make install\
	))

clean:
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc clean
