BUILDDIR := .build

BIN := slimemold
SRCS := slimemold.c slimemold_simulation.c util.c encode_video.c
LDLIBS := -lm
objs = $(patsubst %.c,$(BUILDDIR)/%.o, $(SRCS))

CFLAGS := -Wall -Wextra -Werror -MMD

# make D=1 to compile with debug flags
ifeq ($(D), 1)
CFLAGS +=  -g -O0
else
CFLAGS +=  -O2
endif

# make V=1 to compile in verbose mode
ifneq ($(V), 1)
Q = @
endif

.PHONY: all
all: $(BIN)

deps := $(patsubst %.c,$(BUILDDIR)/%.d,$(SRCS))
-include $(deps)


slimemold: $(objs)
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	$(Q)mkdir -p $@

.PHONY: clean
clean:
	@echo "clean"
	$(Q)rm -f $(BIN) $(objs) $(deps)
