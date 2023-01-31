WIDGETS = $(shell python generator.py)

.PHONY: clean
clean:
	@rm -rf *.m
	@rm -rf *.oct

all: $(WIDGETS)
	$(MKOCTFILE) -lxwidgets __xwidgets_load__.cc

$(WIDGETS): generator.py Widget.mako
	python generator.py Widget.mako