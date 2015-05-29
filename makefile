vmm: vmm.c vmm.h
	gcc -g vmm.c -o vmm
clean:
	rm -f vmm
