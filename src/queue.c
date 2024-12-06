#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
	if (q->size >= MAX_QUEUE_SIZE) {
#ifdef MLQ_SCHED
		printf("Cannot enqueue, ready-queue %d is already full\n", proc->prio);
#else
		printf("Cannot enqueue, ready-queue is already full\n");
#endif
		return;
	}
	int i = 0;
	for(i = (q->size - 1); i >= 0; --i){
			if(q->proc[i]->priority > proc->priority){
					q->proc[i+1] = q->proc[i];
					continue;
			}
			break;
	}
	q->proc[i+1] = proc;
	q->size = q->size + 1;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
    if(q == NULL || q->size <= 0){
        printf("Queue is invalid to dequeue!\n");
        return NULL;
    }
        
	struct pcb_t* proc = q->proc[0];
	
	for(int i = 0; i<(q->size-1); ++i){
		q->proc[i] = q->proc[i+1];
	}
	q->proc[q->size-1]=NULL;
	(q->size) = (q->size) - 1;
	return proc;
}

