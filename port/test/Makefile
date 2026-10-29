# The port test's reference tools, built against the libmgba setup.sh builds (tools/mgba):
# refdump.c, survey.c and sound_survey.c are kurutest (tests/gameplay/kurutest.c, included) with their own main.
ROOT := ../..
MGBA := $(ROOT)/tools/mgba
TOOLS := build/refdump build/survey build/sound_survey

all: $(TOOLS)

build/%: %.c $(ROOT)/tests/gameplay/kurutest.c $(MGBA)/lib/libmgba.a
	@mkdir -p $(@D)
	@echo "cc $<"
	@cc -O2 -w -I$(MGBA)/include $< $(MGBA)/lib/libmgba.a -lm -lpthread -o $@

clean:
	rm -rf build

.PHONY: all clean
