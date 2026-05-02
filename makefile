CXX=gcc
LIBS=-lusb-1.0 -lncursesw
SOURCE=src/program.c
DESTDIR=build
OUTFILE=test

$(DESTDIR)/$(OUTFILE):
	mkdir -p $(DESTDIR)
	$(CXX) $(FLAGS) $(SOURCE) -o $(DESTDIR)/$(OUTFILE) $(LIBS)
