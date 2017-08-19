# boot loader

FINAL := boot
OBJS := bootasm.o bootc.o

MAIN: $(FINAL)

clean:
	rm -f *.o $(FINAL)
	
$(FINAL): boot.o
	$(OBJCOPY) -S -O binary boot.o $(FINAL)
	
boot.o: $(OBJS)
	$(LD) $(LDFLAGS) -N -e start -T $(BASE)/tool/boot.ld -o boot.o $(OBJS)
	
bootasm.o: bootasm.S
	$(CC) $(CFLAGS) -o bootasm.o bootasm.S

bootc.o: bootc.c
	$(CC) $(CFLAGS) -o bootc.o bootc.c
