.SUFFIXES:

OCTS_SRC = __xwidgets_load__.cc

LIBS = -lxwidgets

OCTS = $(OCTS_SRC:.cc=.oct)
WIDGETS := $(shell python generator.py)


all: $(OCTS) $(WIDGETS)

.PHONY: clean
clean:
	@rm -rf $(WIDGETS)
	@rm -rf $(OCTS)

%.m: generator.py Widget.mako
	python generator.py Widget.mako $@

%.oct: %.cc
	$(MKOCTFILE) $(LIBS) $< -o $@
