rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

FINAL := kernel.o
OBJS := \
	$(patsubst %.c,%.o, $(call rwildcard,,*.c)) \
	$(patsubst %.S,%.o, $(call rwildcard,,*.S))
	
MAIN: $(OBJS)
	$(LD) $(LDFLAGS) -r -o $(FINAL) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(BASE)/kernel/ -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -I$(BASE)/kernel/ -o $@ $<

clean:
	rm -f $(OBJS) $(FINAL)
