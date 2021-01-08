# Set correct frame number for MLV Lite files

CC=gcc
CFLAGS=-m64 -O2 -Wall -D_FILE_OFFSET_BITS=64 -std=c99
MINGW=x86_64-w64-mingw32
MINGW_GCC=$(MINGW)-gcc
MINGW_AR=$(MINGW)-ar
MINGW_CFLAGS=-m64 -mno-ms-bitfields -O2 -Wall -D_FILE_OFFSET_BITS=64 -std=c99
TARGET1=mlv_setframes
TARGET2=fpmutil

.FORCE:

all:: $(TARGET1) $(TARGET1).exe $(TARGET2) $(TARGET2).exe strip

$(TARGET1): .FORCE
	$(CC) -c $(TARGET1).c $(CFLAGS)
	$(CC) $(TARGET1).o -o $(TARGET1) -lm -m64

$(TARGET1).exe: .FORCE
	$(MINGW_GCC) -c $(TARGET1).c $(MINGW_CFLAGS)
	$(MINGW_GCC) $(TARGET1).o -o $(TARGET1).exe -lm -m64

$(TARGET2): .FORCE
	$(CC) -c $(TARGET2).c $(CFLAGS)
	$(CC) $(TARGET2).o -o $(TARGET2) -lm -m64

$(TARGET2).exe: .FORCE
	$(MINGW_GCC) -c $(TARGET2).c $(MINGW_CFLAGS)
	$(MINGW_GCC) $(TARGET2).o -o $(TARGET2).exe -lm -m64

strip::
	strip $(TARGET1) $(TARGET1).exe $(TARGET2) $(TARGET2).exe

clean::
	$(RM) $(TARGET1) $(TARGET1).exe $(TARGET1).o $(TARGET2) $(TARGET2).exe $(TARGET2).o
