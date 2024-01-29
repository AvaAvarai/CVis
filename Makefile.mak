CC = gcc
CFLAGS = -Wall -o
LIBS = -lfreeglut -lopengl32 -lglu32

SRC = CVis.c

OUTPUT = CVis

all: $(OUTPUT)

$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) $@ $< $(LIBS)

clean:
	rm -f $(OUTPUT)
