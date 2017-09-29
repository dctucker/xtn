# Wedit Makefile for project xtn
SRCDIR=c:\xtn
CFLAGS=-Ic:\lcc\include  -g2 
CC=$(LCCROOT)\bin\lcc.exe
LINKER=$(LCCROOT)\bin\lcclnk.exe
OBJS=\
	xtn.res \
	xtn.obj

LIBS=wsock32.lib SHELL32.lib
EXE=xtn.exe

$(EXE):	$(OBJS) Makefile
	$(LINKER)  -subsystem windows -o $(SRCDIR)\lcc\xtn.exe $(OBJS) $(LIBS)

# Build xtn.res
XTN_RC=\
	$(SRCDIR)\xtnres.h\

xtn.res:	$(XTN_RC) c:\xtn\xtn.rc
	$(LCCROOT)\bin\lrc -Ic:\xtn -Ic:\lcc\include  c:\xtn\xtn.rc

# Build xtn.c
XTN_C=\
	$(SRCDIR)\xtnres.h\

xtn.obj: $(XTN_C) $(SRCDIR)\xtn.c
	$(CC) -c $(CFLAGS) $(SRCDIR)\xtn.c

link:
	$(LINKER)  -subsystem windows -o $(SRCDIR)\lcc\xtn.exe $(OBJS) $(LIBS)

clean:
	del $(OBJS) c:\xtn\lcc\xtn.exe
