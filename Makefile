TARGETS += nettestc nettests

# Set to n to generate statically linked files
DYNAMIC ?= y

# ----------------------------------------------------------------------------

include Makefile.inc

nettestc_SOURCES = nettestc.c
$(eval $(call prog_rules,nettestc))

nettests_SOURCES = nettests.c
$(eval $(call prog_rules,nettests))
