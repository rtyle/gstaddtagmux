PRODUCT=gstaddtagmux
VERSION=0.1
PACKAGE=$(PRODUCT)-$(VERSION)

INCS=\
	gstaddtagmux.h\

SRCS=\
	gstaddtagmux.c\

OBJS=$(SRCS:.c=.o)

FILES=$(INCS) $(SRCS) Makefile LICENSE README

PKGS=glib-2.0 gstreamer-1.0 gstreamer-tag-1.0

LIBS=$$(pkg-config --libs $(PKGS))

CFLAGS+=-g -Wall -fPIC $$(pkg-config --cflags $(PKGS))

all: lib$(PRODUCT).so

lib$(PRODUCT).so: $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LIBS)

clean:
	$(RM) lib$(PRODUCT).so $(OBJS) $(PACKAGE).tgz
	$(RM) -r $(PACKAGE)

$(PACKAGE).tgz: $(FILES)
	mkdir $(PACKAGE)
	cp $(FILES) $(PACKAGE)
	tar czf $(PACKAGE).tgz $(PACKAGE)
	$(RM) -r $(PACKAGE)

build: $(PACKAGE).tgz
	tar xzf $(PACKAGE).tgz
	cd $(PACKAGE); $(MAKE)
