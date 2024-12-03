
#include "mem.h"
#include "cpu.h"
#include "loader.h"
#include <stdio.h>
#include "mm.h"
int main() {
	// struct pcb_t * ld = load("input/proc/m0s");
	struct pcb_t * proc = load("input/proc/m1s");
	struct memphy_struct * init_mem = malloc(sizeof(struct memphy_struct));
	struct mm_struct * mm = malloc(sizeof(struct mm_struct));
	init_mm(mm, proc);
	init_memphy(init_mem, 1024, 1);
	proc->mram = init_mem;
	proc->mm = mm;
	unsigned int i;
	for (i = 0; i < proc->code->size; i++) {
		run(proc);
		// run(ld);
	}
	dump();
	return 0;
}

