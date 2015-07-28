
#include <timer.h>
#include <cpu_manipulate.h>
#include <port.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <systime.h>
#include <new>

namespace timer {

namespace {

struct timer_callback_link_t {
	/* all measured in tick, not second. */
	uint64_t interval, trigger_count, count_down;
	timer_callback_link_t *prev, *next;
	fn_timer_callback_t lpfn_timer_callback;
	bool deleted;
} *timer_head;

uint64_t system_tick;

void handler(irq::irq_context_t * /*context_ptr*/) {
	static uint32_t ticks = 0;

	system_tick += 1;

	timer_callback_link_t *cur = timer_head;
	
	while (cur != NULL) {
		if (cur->interval != TIMER_TICK_ONE_SHOT) {
			cur->count_down -= 1;
		}
		
		bool to_remove = (cur->deleted || cur->interval == TIMER_TICK_ONE_SHOT);
		bool to_call = !cur->deleted && (cur->count_down == 0 || cur->interval == TIMER_TICK_ONE_SHOT);
		
		if (to_call) {
			cur->lpfn_timer_callback(cur->trigger_count, (handle_t) cur);
			cur->trigger_count += 1;
			cur->count_down = cur->interval;
		}
		if (to_remove) {
			if (timer_head == cur) {
				timer_head = cur->next;
			}
			if (cur->prev != NULL) {
				cur->prev->next = cur->next;
			}
			if (cur->next != NULL) {
				cur->next->prev = cur->prev;
			}
			timer_callback_link_t *next = cur->next;
			delete cur;
			cur = next;
		} else {
			cur = cur->next;
		}
	}
	
	if (++ticks >= TIMER_TICK_PER_SECOND) {
		// printf("One second has passed.\n");
		ticks = 0;
	}
}

}

void initialize(void) {
	system_tick = 0;

	/* clear timer link head. */
	timer_head = NULL;
	
	/* default tick rate, 18 ticks = 1 second. */
	port::outb(PORT_PIT_CMD, 0x36);
	port::outb(PORT_PIT_CHANNEL0, 0);
	port::outb(PORT_PIT_CHANNEL0, 0);

	/* install and enable corespond IRQ. */
	irq::install(0, handler);
	irq::enable(0);
}

uint64_t get_system_tick(void) {
	return system_tick;
}

handle_t add(uint64_t interval, fn_timer_callback_t lpfn_callback) {
	cpu::int_guard guard;
	timer_callback_link_t *ptr = new timer_callback_link_t();
	
	if (ptr == NULL) {
		return TIMER_INVALID_HANDLE;
	}
	
	ptr->deleted = false;
	ptr->prev = NULL;
	ptr->next = timer_head;
	if (timer_head != NULL) {
		timer_head->prev = ptr;
	}
	ptr->interval = interval;
	ptr->trigger_count = 0;
	ptr->count_down = interval;
	ptr->lpfn_timer_callback = lpfn_callback;
	timer_head = ptr;
	
	return (handle_t) ptr;
}

bool remove(handle_t ptr) {
	timer_callback_link_t *timer_ptr = (timer_callback_link_t *) ptr;
	
	timer_ptr->deleted = true;
	return true;
}

} /* timer */ 
