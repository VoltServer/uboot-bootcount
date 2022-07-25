SUBDIRS         = src
doc_DATA        = README.md COPYING
EXTRA_DIST      = README.md
#CLEANFILES      = README

## Generate MD5 checksum file
MD5 = md5sum
md5-dist: dist
	@for file in $(DIST_ARCHIVES); do	\
		$(MD5) $$file > $$file.md5;	\
	done

#README: README.md

#%.md: ;
#	pandoc -f markdown -t plain --wrap=none $< -o $@

