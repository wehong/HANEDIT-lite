# HANEDIT lite - Open Watcom wmake makefile (16-bit real mode, no DOS extender)
#   wmake            -> HANEDITL.EXE  (needs HANEDIT.FNT, optional HANJA.DIC)
CC     = wcc
CFLAGS = -q -bt=dos -ml -ox -s -zt=256 -wx -zq -i=src -i=libhangul
HFLAGS = -q -bt=dos -ml -ox -s -zt=256 -zq -za99 -DENABLE_EXTERNAL_KEYBOARDS=0 -i=libhangul

OBJS = main.obj vga.obj kbd.obj fontfile.obj hangulctype.obj hangulinputcontext.obj hangulkeyboard.obj

HANEDITL.EXE: $(OBJS)
	wlink name HANEDITL.EXE system dos option quiet option stack=16k option map=hanedit.map file { $(OBJS) }

main.obj: src/main.c src/vga.h src/kbd.h src/font.h libhangul/hangul.h
	$(CC) $(CFLAGS) src/main.c -fo=$@
vga.obj: src/vga.c src/vga.h src/font.h
	$(CC) $(CFLAGS) src/vga.c -fo=$@
kbd.obj: src/kbd.c src/kbd.h
	$(CC) $(CFLAGS) src/kbd.c -fo=$@
fontfile.obj: src/fontfile.c src/font.h
	$(CC) $(CFLAGS) src/fontfile.c -fo=$@
hangulctype.obj: libhangul/hangulctype.c
	$(CC) $(HFLAGS) libhangul/hangulctype.c -fo=$@
hangulinputcontext.obj: libhangul/hangulinputcontext.c
	$(CC) $(HFLAGS) libhangul/hangulinputcontext.c -fo=$@
hangulkeyboard.obj: libhangul/hangulkeyboard.c libhangul/hangulkeyboard.h
	$(CC) $(HFLAGS) libhangul/hangulkeyboard.c -fo=$@

clean: .SYMBOLIC
	rm -f *.obj HANEDITL.EXE *.err hanedit.map
