SUBDIRS         = src
doc_DATA        = README.md COPYING
EXTRA_DIST      = README.md
#CLEANFILES      = README

BINARY_DISTDIR = $(PACKAGE)-$(VERSION)-bin
BINARY_TARBALL = dist/$(BINARY_DISTDIR)-$(host_cpu).tar.gz

## Generate MD5 checksum file
MD5 = md5sum
md5-dist: dist
	@for file in $(DIST_ARCHIVES); do	\
		$(MD5) $$file > $$file.md5;	\
	done

#README: README.md

#%.md: ;
#	pandoc -f markdown -t plain --wrap=none $< -o $@

binary-dist: all
	@echo "Creating binary tarball..."
	mkdir -p $(BINARY_DISTDIR)/bin
	cp $(SUBDIRS)/bootcount $(BINARY_DISTDIR)/bin/
	cp $(EXTRA_DIST) $(BINARY_DISTDIR)/

	mkdir -p $(shell dirname $(BINARY_TARBALL))
	tar -czf $(BINARY_TARBALL) $(BINARY_DISTDIR)
	rm -rf $(BINARY_DISTDIR)
	@echo "Binary tarball created: $(BINARY_TARBALL)"
