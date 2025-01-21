# Copyright (c) 2025, coreybrenner
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
SRCS := input.c vector.c string.c cursor.c char.c
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

