target_a=cJSONPP.a
BUILDDIR=build
ODIR=$(BUILDDIR)/obj/lib
BINDIR=$(BUILDDIR)/bin
IDIR=
_OBJ = cJsonPP.opp


#_OBJ += TestAlgebraicCircleFits.opp
#target_bin=TestAlgebraicCircleFits
target_a=cJSONPP.a

include $(MakeTemplate)

.PHONY: test clean_test

# Make sure build directories exist
$(BUILDDIR) $(ODIR):
	mkdir -p $@

pre_build: | $(BUILDDIR) $(ODIR)

test: $(target_a)
	$(MAKE) -C test
	$(MAKE) -C test test

clean_test:
	$(MAKE) -C test clean

clean: clean_test
	rm -rf $(BUILDDIR)
