/* c-basic-offset: 2; tab-width: 2; indent-tabs-mode: t
 * vi: set noexpandtab:
 * :noTabs=false:
 *
 * Originally based on some code from UW's CSE 451:
 *   http://www.cs.washington.edu/education/courses/cse451/12sp/projects/project0.txt
 *   http://www.cs.washington.edu/education/courses/cse451/12sp/projects/project2.html
 */

#include <stdlib.h>
#include "queue.h"
#include "threadpool_macros.h"
#include "../kp_common.h"
#include "../kp_recovery.h"
  //unfortunate...

typedef struct _element element;
struct _element {
	void *data;
	queue_element_free *free_fn;
	element *next;
};

struct _queue {
	element *head;
	element *tail;
	pthread_mutex_t *lock;
	unsigned int len;
	bool use_nvm;
};

#if 0
struct _queue_elem {
	sthread_t sth;
	struct _queue_elem *next;
};
typedef struct _queue_elem* queue_elem_t;
#endif

#if 0
static queue_elem_t free_list;

#ifdef USE_PTHREADS
/* We need to lock the free_list, but can't depend on the user
 * having implemented locks. So we use pthread locks when available,
 * and depend on the non-preemptive nature of user threads otherwise.
 */
static pthread_mutex_t free_list_lock;

#define LOCK_FREE_LIST pthread_mutex_lock(&free_list_lock)
#define UNLOCK_FREE_LIST pthread_mutex_unlock(&free_list_lock)

#else /* USE_PTHREADS */

#define LOCK_FREE_LIST ((void)0)
#define UNLOCK_FREE_LIST ((void)0)

#endif /* USE_PTHREADS */
#endif

#if 0
struct _queue {
	queue_elem_t head;
	queue_elem_t tail;
	int size;
};
#endif

int queue_create(queue **q, bool use_nvm) {
	int ret;

	if (!q) {
		tp_error("got a NULL argument: q=%p\n", q);
		return -1;
	}
	tp_debug("allocating a new queue\n");

	//*q = (queue *)malloc(sizeof(queue));
	kp_kpalloc((void **)q, sizeof(queue), use_nvm);
	if (!(*q)) {
		tp_error("malloc(queue) failed\n");
		return -1;
	}

	(*q)->head = NULL;
	(*q)->tail = NULL;
	(*q)->len = 0;
	(*q)->use_nvm = use_nvm;

	ret = kp_mutex_create("queue lock", &((*q)->lock));
	if (ret != 0) {
		tp_error("mutex creation failed\n");
		kp_free((void **)q, use_nvm);
		return -1;
	};

	tp_debug("allocated new queue, use_nvm = %s\n", use_nvm ? "true" : "false");
	return 0;
}

void queue_destroy(queue *q) {
	element *head;
	element *next;

	if (!q) {
		tp_error("got a NULL argument: q=%p\n", q);
		return;
	}
	tp_debug("destroying the queue and its elements (if any)\n");

	/* Free the queue's elements, if any: */
	head = q->head;
	while (head) {
#ifdef TP_ASSERT
		if (q->len == 0) {
			tp_die("unexpected: queue length hit 0 while destroying "
					"elements\n");
		}
		if (q->len == 1) {
			if (head != q->tail) {
				tp_die("unexpected: head %p != q->tail %p\n", head, q->tail);
			}
		}
#endif
		next = head->next;
		if (head->free_fn) {
			head->free_fn(head->data);
		}
		kp_free((void **)(&head), q->use_nvm);
		q->len -= 1;
		head = next;
	}

	/* Free the queue itself: */
	kp_mutex_destroy("queue lock", &(q->lock));
	kp_free((void **)(&q), q->use_nvm);

	tp_debug("destroyed the queue\n");
}

int queue_enqueue(queue *q, void *e, queue_element_free free_fn) {
	element *elem;

	if (!q) {
		tp_error("got a NULL argument: q=%p\n", q);
		return -1;
	}

	/* Allocate new element and initialize it: */
	kp_kpalloc((void **)(&elem), sizeof(element), q->use_nvm);
	if (!elem) {
		tp_error("malloc(element) failed\n");
		return -1;
	}
	elem->data = e;
	elem->free_fn = free_fn;
	elem->next = NULL;

	/* Enqueue onto end of queue: if it's the first element, then make
	 * it the head, otherwise make it the current tail's next element.
	 * Then make it the new tail.
	 */
	if (q->tail != NULL) {
		q->tail->next = elem;
	} else {
#ifdef TP_ASSERT
		if (q->head != NULL) {
			tp_die("unexpected: head is not NULL\n");
		}
		if (q->len != 0) {
			tp_die("unexpected: length is %u, not 0\n", q->len);
		}
#endif
		q->head = elem;
	}
	q->tail = elem;
	q->len += 1;

	tp_debug("enqueued new element with data=%p, free_fn=%p, next=%p; "
			"queue length is now %u\n", elem->data, elem->free_fn,
			elem->next, q->len);
	return 0;
}

int queue_dequeue(queue *q, void **e) {
	element *head;

	if (!q || !e) {
		tp_error("got a NULL argument: q=%p, e=%p\n", q, e);
		return -1;
	}

	if (q->len == 0) {
		tp_debug("queue has no elements, returning 1\n");
		return 1;
	}
#ifdef TP_ASSERT
	if (!(q->head) || !(q->tail)) {
		tp_die("unexpected NULL pointer: q->head=%p, q->tail=%p\n",
				q->head, q->tail);
	}
#endif

	/* Remember, the "head" of the queue is the "front" of the queue - the
	 * element that we're dequeueing. Set the element pointer that will be
	 * returned, then set the new head to be the current head's next element.
	 * If we're dequeueing the last/only element in the queue, then both head
	 * and tail are set to NULL.
	 */
	head = q->head;
	*e = head->data;
	if (head->next == NULL) {
#ifdef TP_ASSERT
		if (q->len != 1) {
			tp_die("unexpected: queue length is %u, should be 1\n", q->len);
		}
		if (head != q->tail) {
			tp_die("unexpected: queue head %p and tail %p should be the "
					"same\n", head, q->tail);
		}
#endif
		q->tail = NULL;
	}
	q->head = head->next;
	q->len -= 1;

	/* Don't forget to free the element that we allocated: */
	kp_free((void **)(&head), q->use_nvm);

	tp_debug("returning element at %p, queue length is now %u\n", *e, q->len);
	return 0;
}

unsigned int queue_length(queue *q) {
	return q->len;
}

bool queue_is_empty(queue *q) {
	return (q->len == 0);
}

/*
 * Editor modelines  -  http://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 2
 * tab-width: 2
 * indent-tabs-mode: t
 * End:
 *
 * vi: set noexpandtab:
 * :noTabs=false:
 */
