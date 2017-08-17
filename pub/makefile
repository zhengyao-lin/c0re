FINAL := pub.o

OBJS := string.o

MAIN: $(OBJS)
	$(LD) $(LDFLAGS) -r -o $(FINAL) $(OBJS)
	
%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(FINAL) $(OBJS)
