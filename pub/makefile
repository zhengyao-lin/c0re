rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

FINAL := pub.o

OBJS := $(patsubst %.c,%.o, $(call rwildcard,,*.c))

MAIN: $(OBJS)
	$(LD) $(LDFLAGS) -r -o $(FINAL) $(OBJS)
	
%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(FINAL) $(OBJS)
