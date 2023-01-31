XWIDGETS_SRC = $(shell python generator.py)
OCTS_SRC = __xwidgets_load__.cc

XWIDGETS_DIR = +xwidgets

LIBS = -lxwidgets

WIDGETS := $(addprefix $(XWIDGETS_DIR)/,$(XWIDGETS_SRC))
OCTS = $(OCTS_SRC:.cc=.oct)

all: $(OCTS) $(WIDGETS)
	$(file > FILES,$(OCTS) $(WIDGETS))

.PHONY: clean
clean:
	@rm -rf $(WIDGETS)
	@rm -rf $(OCTS)
	@rm -rf FILES

%.oct: %.cc
	$(MKOCTFILE) $(LIBS) $< -o $@

+xwidgets/%.m: generator.py Widget.mako
	python generator.py Widget.mako $@
