VMM: cmd vmm
cmd: cmd.c vmm.h 
	gcc -g cmd.c -o cmd
vmm: vmm.c vmm.h
	gcc -g vmm.c -o vmm
clean:
	rm -f vmm cmd
