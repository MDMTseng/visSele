

.PHONY=domake MinGW_build NonMinGW_build NonMinGW_clean MinGW_clean
curPath=$(abspath ./)

ifeq ($(OS)$(CC),Windows_NTcc)
	build_target=MinGW_build
	clean_target=MinGW_clean
else
	build_target=NonMinGW_build
	clean_target=NonMinGW_clean
endif


domake:$(build_target)

MinGW_build:
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc
	ls $(curPath)/lib  $(curPath)/include  || \
	(cd src-1.2.11 &&( \
		sh ./configure&& \
		make -fwin32/Makefile.gcc all SHARED_MODE=1 &&\
		make -fwin32/Makefile.gcc install SHARED_MODE=1 LIBRARY_PATH=$(curPath)/staticlib INCLUDE_PATH=$(curPath)/include BINARY_PATH=$(curPath)/lib \
	))
	cp lib/zlib1.dll lib/zlib.dll

NonMinGW_build:
	#cd src-1.2.11 ; make -fwin32/Makefile.crossgcc
	ls $(curPath)/lib  $(curPath)/include  || \
	(cd src-1.2.11 &&( \
		sh ./configure --sharedlibdir=$(curPath)/lib --libdir=$(curPath)/staticlib --includedir=$(curPath)/include --prefix=$(curPath)/pref && \
		make && \
		make install\
	))

clean:$(clean_target)

NonMinGW_clean:
	cd src-1.2.11 && make clean
	-rm -r lib include staticlib pref
MinGW_clean:
	cd src-1.2.11 && make -fwin32/Makefile.gcc clean SHARED_MODE=1 
	-rm -r lib include staticlib pref
