WIDGETS = $(shell python generator.py)

all: $(WIDGETS)
	$(MKOCTFILE) -lxwidgets __xwidgets_load__.cc

$(WIDGETS): generator.py Widget.mako
	python generator.py Widget.mako