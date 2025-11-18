/* SPDX-License-Identifier: MIT */

/* StACSOS - Kernel
 *
 * Copyright (c) University of St Andrews 2024
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#include <stacsos/kernel/sched/alg/rr.h>

// *** COURSEWORK NOTE *** //
// This will be where you are implementing your round-robin scheduling algorithm.
// Please edit this file in any way you see fit.  You may also edit the rr.h
// header file.

using namespace stacsos::kernel::sched;
using namespace stacsos::kernel::sched::alg;

void round_robin::add_to_runqueue(tcb &tcb) {

    runqueue.enqueue(&tcb);
}

void round_robin::remove_from_runqueue(tcb &tcb) {

    // if target is current task, wait for it to stop running to 
    // avoid unclean interrupt
    if (&tcb == current_task.task) {
        current_task.to_remove = true;
    }

    else {
		runqueue.remove(&tcb);
	}
}

tcb* round_robin::select_next_task(tcb *current) {

    //no need to manage multiple tasks if there's only one. (the runqueue is empty)
    if (runqueue.empty()) {

        if (current_task.to_remove) {
            current_task.task = nullptr;
            current_task.to_remove = false;
        }

        return current_task.task;
    }

    //dont enqueue the last task if it was set to be removed
    if (!current_task.to_remove && current_task.task != nullptr) {
        runqueue.enqueue(current_task.task);
    }

    current_task.to_remove = false;
    current_task.task = runqueue.pop();
    return current_task.task;
}