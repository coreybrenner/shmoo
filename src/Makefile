SRCS := input.c vector.c
OBJS := $(SRCS:%.c=%.o)
LIBS := shmoo

shared_lib = lib$(strip $1).so
shared_obj = $(patsubst %.c,%.So,$1)
static_lib = lib$(strip $1).a
static_obj = $(patsubst %.c,%.To,$1)

SHARED_LIBS = $(foreach L,$(LIBS),$(call shared_lib,$L))
STATIC_LIBS = $(foreach L,$(LIBS),$(call static_lib,$L))

STATIC_CFLAGS =
SHARED_CFLAGS = -fPIC -shared
CFLAGS = -I../include -W -Wall -pedantic -std=c99 -D_DEFAULT_SOURCE

$(foreach L,$(LIBS), \
  $(eval $(call shared_lib,$L) : $(call shared_obj,$(SRCS) $($(strip $1)_SRCS))) \
  $(eval $(call static_lib,$L) : $(call static_obj,$(SRCS) $($(strip $1)_SRCS))) \
 )

.PHONY: all shared static
all   : shared static
shared: $(SHARED_LIBS)
static: $(STATIC_LIBS)

%.To : %.c
	$(CC) -o $@ -c $< $(CFLAGS) $(STATIC_CFLAGS)

%.So : %.c
	$(CC) -o $@ -c $< $(CFLAGS) $(SHARED_CFLAGS)

lib%.so :
	$(CC) -o $@ $< -shared -Wl,-soname,$@.0

lib%.a  :
	$(AR) cru $@ $<


.PHONY: clean
clean :
	$(RM) -f *.To *.So *.Go $(SHARED_LIBS) $(STATIC_LIBS)

