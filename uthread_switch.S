	.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register; it is caller saved.
 * 
 * The job of uthread_switch is to save the current
 * thread state into the structure pointed to by current_thread, restore next_thread's
 * state, and make current_thread point to where next_thread was pointing to, so
 * that when uthread_switch returns next_thread is running and is
 * the current_thread.
 */
	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */

	pushal
	movl current_thread, %eax
	movl %esp, (%eax)

	movl next_thread, %eax
	movl (%eax), %esp
	popal

	movl next_thread, %eax
	movl %eax, current_thread
	
	movl $0, next_thread
	ret				/* pop return address from stack */
