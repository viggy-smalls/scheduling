/*
 * student.c
 * Multithreaded OS Simulation for CS 425, Project 3
 * Acknowledgement: The code is authored by Kishore Ramachandran at Gatech. 
 *
 * This file contains the CPU scheduler for the simulation.  
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os-sim.h"


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
 
static pcb_t **current;  
static pcb_t *ready;
static pthread_mutex_t current_mutex;
static pthread_mutex_t ready_mutex;
static pthread_cond_t ready_cond;
static int waiting[16];
static int rr;
static int preempt_time;




/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *      with a pointer to NULL to select the idle process.
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
	pcb_t *head = ready;
	pcb_t *temp;
	
	if(head == NULL){ 
		context_switch(cpu_id, NULL, preempt_time);
	}
	else{
		//Critical section
		pthread_mutex_lock(&ready_mutex);
		temp = head;
		temp->state = PROCESS_RUNNING;
		
		if(head->next == NULL){
			head = NULL;
		}
		else{
			head = temp->next;
		}
		
		//Lock current mutex
		pthread_mutex_lock(&current_mutex);
		current[cpu_id] = temp;
		
		//Exit Section
		pthread_mutex_unlock(&current_mutex);
		pthread_mutex_unlock(&ready_mutex);
		
		//Context switch
		context_switch(cpu_id, temp, preempt_time);
	}
}

/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
    //Entry
	pthread_mutex_lock(&current_mutex);
	waiting[cpu_id] = 0;
	
    while(pthread_cond_wait(&ready_cond, &current_mutex));
	
	//Exit
	pthread_mutex_unlock(&current_mutex);
	schedule(cpu_id);
	waiting[cpu_id] = 1;

    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
	 // mt_safe_usleep(1000000);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
	pcb_t *head = ready;
	pcb_t *temp = malloc(sizeof(pcb_t));
	
    //Entry section
	pthread_mutex_lock(&current_mutex);
	
	//Going to the end of the linked list 
	while(head->next != NULL){
		head = head->next;		
	}

	//Lock ready mutex
	pthread_mutex_lock(&ready_mutex);
	
	//Make a temp node to store at the end of ready
	temp = current[cpu_id];
	temp->next = NULL;
	temp->state = PROCESS_READY;
	head->next = temp;
	
	
	//Exit section
	pthread_mutex_unlock(&ready_mutex);
	pthread_mutex_unlock(&current_mutex);
	
	//Place into waiting queue and schedule a new process
	//add_to_waiting(current[cpu_id]);
	schedule(cpu_id);
	
	
	
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    //Critical Section
	pthread_mutex_lock(&current_mutex);
	
	//Yield process and schedule another
	if(current[cpu_id] != NULL){
		current[cpu_id]->state = PROCESS_WAITING;
	}
	
	//Exit Section
	pthread_mutex_unlock(&current_mutex);
	//add_to_waiting(current[cpu_id]);
	schedule(cpu_id);
	
	
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
	pthread_mutex_lock(&current_mutex);
	
	current[cpu_id]->state = PROCESS_TERMINATED;
	
	pthread_mutex_unlock(&current_mutex);
	schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *      to preempt the CPU with the lowest priority process to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
	pcb_t *head = ready;
	
	//Set process to ready
	process->state = PROCESS_READY;

	//going to the end of the linked list and adding process to
	if(head != NULL){
	
		while(head->next != NULL){
			head = head->next;		
		}
	}
	
	//Insert into ready
	pthread_mutex_lock(&ready_mutex);
	if(head == NULL){
		head = process;
	}
	else{
		head->next = process;
	}
	pthread_cond_signal(&ready_cond);
	pthread_mutex_unlock(&ready_mutex);
	
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */

int main(int argc, char *argv[]){
    int cpu_count;

	for(int i = 0; i <16; i++){
		waiting[i] = 0;
	}
	
    /* Parse command-line arguments */
    if (argc != 2)
    {
        fprintf(stderr, "CS 425 Project 3 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n");

        return -1;
    }
    cpu_count = atoi(argv[1]);

    /* FIX ME - Add support for -r and -p parameters*/
	if(argc > 2){
	
		if(strcmp(argv[2], "-r") == 0){
			rr = 1;
			if(argv[3] != NULL){
				preempt_time = (int)argv[3];
			}
		}
		else{
			preempt_time = -1;
			}
	}
	
    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
	
	/* Allocate other mutexes and condition variables as well */
	pthread_mutex_init(&ready_mutex, NULL);
	pthread_cond_init(&ready_cond, NULL);
	
    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}


