all: $(TARGETS)

VERSION := $(shell git describe --tags --abbrev=10 \
			--dirty --long --always 2> /dev/null || \
				echo "v0.0.0")
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
CFLAGS += -O2 -Wall -D_GNU_SOURCE -D__VERSION=\"$(VERSION)\"
CFLAGS += -MMD    # automatic .d dependency file generation
ifneq ($(DYNAMIC),y)
CFLAGS += -static
endif
# CFLAGS += -Werror

ifeq ($(DYNAMIC),y)
define lib_rules
$1: $(foreach n,$($(1)_LDLIBS),lib$n.so)
lib$(1).so: $($(1)_SOURCES)
	$(CC) -shared -fPIC $(CFLAGS) $($(1)_CFLAGS) $(CPPFLAGS) $($(1)_CPPFLAGS) \
		$($(1)_SOURCES) -o lib$(1).so \
		$(LDFLAGS) $($(1)_LDFLAGS) $(LDLIBS) $(foreach n,$($(1)_LDLIBS),-l$n)
-include $($(1)_SOURCES:%.c=%.d)

lib$(1).so_clean:
	rm -rf lib$(1).so $($(1)_SOURCES:.c=.o) $($(1)_SOURCES:.c=.d)
clean: lib$(1).so_clean
endef
else
define lib_rules
$(1): $(foreach n,$($(1)_LDLIBS),lib$n.a)
lib$(1).a: $($(1)_SOURCES:.c=.o)
	$(AR) rcu $$@ $$+
-include $($(1)_SOURCES:%.c=%.d)

lib$(1).a_clean:
	rm -rf lib$(1).a $($(1)_SOURCES:.c=.o) $($(1)_SOURCES:.c=.d)
clean: lib$(1).a_clean
endef
endif

define prog_rules
ifeq ($(DYNAMIC),y)
$1: $(foreach n,$($(1)_LDLIBS),lib$n.so)
else
$1: $(foreach n,$($(1)_LDLIBS),lib$n.a)
endif

$1: $(patsubst %.c, %.o, $($(1)_SOURCES))
	$(CC) $(CFLAGS) $($(1)_CFLAGS) $(CPPFLAGS) $($(1)_CPPFLAGS) \
		$(patsubst %.c, %.o, $($(1)_SOURCES)) -o $$@ \
                $(LDFLAGS) $($(1)_LDFLAGS) $(LDLIBS) $(foreach n,$($(1)_LDLIBS),-l$n)

-include $($(1)_SOURCES:%.c=%.d)

$(1)_clean:
	rm -rf $1 $($(1)_SOURCES:.c=.o) $($(1)_SOURCES:.c=.d)
clean: $(1)_clean
endef

%.o : %.c
	$(CC) -c $(CFLAGS) $(${@}_CFLAGS) $(CPPFLAGS) $(${@}_CPPFLAGS) $< \
		-o $@
