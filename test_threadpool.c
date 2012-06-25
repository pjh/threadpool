/* Peter Hornyack
 * 2012-06-25
 * University of Washington
 */

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "queue.h"
#include "threadpool.h"
#include "threadpool_macros.h"

bool use_nvm = true;

void short_task(void *arg) {
	unsigned int digit, value;
	unsigned long long sum;

	value = (unsigned int)arg;  //32-bit on burrard
	printf("starting short task, value is: %u\n", value);

	sum = 0;
	while (true) {
		digit = value % 10;
		sum += digit;
		value = value / 10;  //shift right
		if (value <= 0) {
			break;
		}
	}

	printf("finished short task, sum: %llu\n", sum);
	return;
}

void long_task(void *arg) {
	int i, j, inner, outer;
	unsigned long long sum;

	/* One thousand million is one billion, which is approx. one second
	 * on burrard... so ten seconds is ten thousand million? Actually,
	 * turns out to be about twice as long as that. */
	inner = 1000000;
	outer = 2000;
	sum = (unsigned long long)pthread_self();
	printf("starting long task, initial value: %llu\n", sum);

	for (i = 0; i < outer; i++) {
		for (j = 0; j < inner; j++) {
			sum += j;
		}
	}

	printf("long task complete, calculated sum: %llu\n", sum);
	return;
}

#if 0
void stress_test_queue()
{
	int i, j, ret;
	unsigned long long idx, count;
	unsigned long tid;
	vector *v;
	void *e;
	long int rand_int;
	ssize_t size;
	char *final;

	if (APPENDS_MAX <= APPENDS_MIN) {
		v_die("invalid APPENDS_MAX %u and APPENDS_MIN %u\n", APPENDS_MAX,
				APPENDS_MIN);
	}
	if (DELETES_MAX <= DELETES_MIN) {
		v_die("invalid DELETES_MAX %u and DELETES_MIN %u\n", DELETES_MAX,
				DELETES_MIN);
	}

	srandom((unsigned int)time(NULL));
	tid = pthread_self();
	size = ELT_SIZE_MIN;
	ret = vector_alloc(&v);

	for (i = 0; i < LOOPS; i++) {
		/* First, do some appends: */
		rand_int = random();
		rand_int = (rand_int % (APPENDS_MAX - APPENDS_MIN)) + APPENDS_MIN;
		v_debug("appending %ld elements of size %u to vector:\n",
				rand_int, size);
		for (j = rand_int; j > 0; j--) {
			e = malloc(size);
			((char *)e)[0] = 'A' + ((i+rand_int-j)%26);
			((char *)e)[1] = '\0';
			ret = vector_append(v, e);
		}
		v_debug("appended %ld elements, now count=%llu\n", rand_int, vector_count(v));

		/* Then, do some deletes: */
		rand_int = random();
		rand_int = (rand_int % (DELETES_MAX - DELETES_MIN)) + DELETES_MIN;
		v_debug("deleting up to %ld values from vector\n", rand_int);
		for (j = rand_int; j > 0; j--) {
			count = vector_count(v);
			if (count == 0) {
				break;
			}
			idx = (i + j) % count;
			ret = vector_delete(v, idx, &e);
			v_debug("deleted element %s at idx=%llu, now freeing element\n",
					(char *)e, idx);
			free(e);
		}
		v_debug("deleted %ld elements, now count=%llu\n", rand_int - j,
				vector_count(v));

		/* Loop again: */
		size = size + ELT_SIZE_DIFF;
		if (size > ELT_SIZE_MAX) {
			size = ELT_SIZE_MIN;
		}
	}

	/* Print final contents of vector: */
	count = vector_count(v);
	v_debug("final vector count=%llu\n", count);
	final = malloc(count + 1);
	for (i = 0; i < count; i++) {
		ret = vector_get(v, i, &e);
		final[i] = ((char *)e)[0];
	}
	final[count] = '\0';
	v_debug("final contents of vector: %s\n", final);
	free(final);

	/* Free vector: */
	vector_free_contents(v);
	vector_free(v);
}
#endif

void test_queue(void) {
	int ret;
	queue *q;
	void *e, *e1, *e2, *e3, *e4;
	unsigned int uret;

	e1 = malloc(1);
	e2 = malloc(22);
	e3 = malloc(333);
	e4 = malloc(4444);
	if (!e1 || !e2 || !e3 || !e4) {
		tp_die("some malloc failed\n");
	}

	ret = queue_create(&q, use_nvm);
	tp_testcase_int("queue_create", 0, ret);
	if (ret != 0) {
		tp_die("queue_create() returned error=%d\n", ret);
	}

	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue empty queue", 1, ret);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 0, uret);
	ret = queue_enqueue(q, e1, free);
	tp_testcase_int("enqueue", 0, ret);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 1, uret);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e1, e);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 0, uret);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue empty", 1, ret);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 0, uret);
	ret = queue_enqueue(q, e1, free);
	tp_testcase_int("enqueue", 0, ret);
	queue_destroy(q);
	tp_testcase_int("destroy non-empty queue", 0, 0);

	e1 = malloc(11111);  //should have been freed by queue_destroy()
	ret = queue_create(&q, use_nvm);
	tp_testcase_int("queue_create", 0, ret);
	if (ret != 0) {
		tp_die("queue_create() returned error=%d\n", ret);
	}
	ret = queue_enqueue(q, e1, free);
	tp_testcase_int("enqueue e1", 0, ret);
	ret = queue_enqueue(q, e2, free);
	tp_testcase_int("enqueue e2", 0, ret);
	ret = queue_enqueue(q, e3, free);
	tp_testcase_int("enqueue e3", 0, ret);
	ret = queue_enqueue(q, e4, free);
	tp_testcase_int("enqueue e4", 0, ret);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 4, uret);
	tp_testcase_int("is_empty", queue_is_empty(q) ? 1 : 0, 0);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e1);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 3, uret);
	tp_testcase_int("is_empty", queue_is_empty(q) ? 1 : 0, 0);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e2);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 2, uret);
	tp_testcase_int("is_empty", queue_is_empty(q) ? 1 : 0, 0);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e3);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 1, uret);
	tp_testcase_int("is_empty", queue_is_empty(q) ? 1 : 0, 0);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e4);
	uret = queue_length(q);
	tp_testcase_uint("queue length", 0, uret);
	tp_testcase_int("is_empty", queue_is_empty(q) ? 1 : 0, 1);

	ret = queue_enqueue(q, e4, free);
	tp_testcase_int("enqueue e4", 0, ret);
	ret = queue_enqueue(q, e3, free);
	tp_testcase_int("enqueue e3", 0, ret);
	ret = queue_enqueue(q, e2, free);
	tp_testcase_int("enqueue e2", 0, ret);
	ret = queue_enqueue(q, e1, free);
	tp_testcase_int("enqueue e1", 0, ret);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e4);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e3);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e2);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 0, ret);
	tp_testcase_ptr("dequeue", e, e1);
	ret = queue_dequeue(q, &e);
	tp_testcase_int("dequeue", 1, ret);
	queue_destroy(q);
	tp_testcase_int("destroy empty queue", 0, 0);

	free(e1);
	free(e2);
	free(e3);
	free(e4);

	return;
}

void test_threadpool(void) {
	int ret;
	unsigned int uret;
	bool wait = true;
	threadpool *tp;

	/* Create and destroy empty threadpool: */
	ret = threadpool_create(&tp, 0, use_nvm);
	tp_testcase_int("threadpool create", 0, ret);
	if (ret != 0) {
		tp_die("threadpool_create() failed\n");
	}
	uret = threadpool_get_worker_count(tp);
	tp_testcase_uint("worker count", 0, uret);
	threadpool_destroy(tp, wait);
	tp_testcase_int("threadpool destroy empty", 0, 0);

	/* Create empty threadpool, add a worker and destroy: */
	ret = threadpool_create(&tp, 0, use_nvm);
	tp_testcase_int("threadpool create", 0, ret);
	if (ret != 0) {
		tp_die("threadpool_create() failed\n");
	}
	uret = threadpool_add_worker(tp);
	tp_testcase_uint("add worker", 1, uret);
	threadpool_destroy(tp, wait);
	tp_testcase_int("threadpool destroy 1", 0, 0);

	/* Create empty threadpool, add a worker, remove a worker and destroy: */
	ret = threadpool_create(&tp, 0, use_nvm);
	tp_testcase_int("threadpool create", 0, ret);
	if (ret != 0) {
		tp_die("threadpool_create() failed\n");
	}
	uret = threadpool_add_worker(tp);
	tp_testcase_uint("add worker", 1, uret);
	uret = threadpool_get_worker_count(tp);
	tp_testcase_uint("worker count", 1, uret);
	uret = threadpool_get_task_count(tp);
	tp_testcase_int("task count", 0, uret);
	uret = threadpool_remove_worker(tp, wait);
	tp_testcase_uint("remove worker", 0, uret);
	uret = threadpool_get_worker_count(tp);
	tp_testcase_uint("worker count", 0, uret);
	threadpool_destroy(tp, wait);
	tp_testcase_int("threadpool destroy 0", 0, 0);

	/* Two short tasks: */
	ret = threadpool_create(&tp, 2, use_nvm);
	tp_testcase_int("threadpool create", 0, ret);
	if (ret != 0) {
		tp_die("threadpool_create() failed\n");
	}
	uret = threadpool_get_worker_count(tp);
	tp_testcase_uint("worker count", 2, uret);
	ret = threadpool_add_task(tp, short_task, (void *)4123);
	tp_testcase_int("add task", 0, ret);
	ret = threadpool_add_task(tp, short_task, (void *)5142382);
	tp_testcase_int("add task", 0, ret);
	uret = threadpool_get_task_count(tp);
	tp_testcase_int("task count", 2, uret);  //NOTE: not guaranteed!!
	tp_test("waiting for tasks to complete: sleeping...\n");
	sleep(2);
	uret = threadpool_get_task_count(tp);
	tp_testcase_int("task count", 0, uret);  //NOTE: not guaranteed!!

	/* Two long tasks: */
	ret = threadpool_add_task(tp, long_task, NULL);
	tp_testcase_int("add task", 0, ret);
	ret = threadpool_add_task(tp, long_task, NULL);
	tp_testcase_int("add task", 0, ret);
	uret = threadpool_get_task_count(tp);
	tp_testcase_int("task count", 2, uret);  //NOTE: not guaranteed!!
	tp_test("waiting for tasks to complete: sleeping...\n");
	sleep(8);
	uret = threadpool_get_task_count(tp);
	tp_testcase_int("task count", 0, uret);  //NOTE: not guaranteed!!

	/* More tasks than threads in pool: */
	ret = threadpool_add_task(tp, long_task, NULL);
	tp_testcase_int("add task", 0, ret);
	ret = threadpool_add_task(tp, short_task, (void *)4123);
	tp_testcase_int("add task", 0, ret);
	ret = threadpool_add_task(tp, short_task, (void *)5142382);
	tp_testcase_int("add task", 0, ret);
	ret = threadpool_add_task(tp, long_task, NULL);
	tp_testcase_int("add task", 0, ret);
	uret = threadpool_get_task_count(tp);
	tp_testcase_int("task count", 4, uret);  //NOTE: not guaranteed!!

	threadpool_destroy(tp, wait);
	tp_testcase_int("threadpool destroy", 0, 0);

	return;
}

int main(int argc, char *argv[])
{
	test_queue();
	//stress_test_queue();

	test_threadpool();
#if 0
	short_task((void *)1);
	short_task((void *)12);
	short_task((void *)123);
	short_task((void *)1234);
	short_task((void *)51234);
	long_task(NULL);
#endif

	printf("\nDon't forget to run this test file under valgrind too!\n");

	return 0;
#if 0
	int i, ret;
	unsigned long long count;
	unsigned long tid;
	vector *v;
	void *e;

	stress_test();
	v_print("stress test complete\n");

	tid = pthread_self();
	v_print("sizeof(void *)=%u, sizeof(unsigned int)=%u, "
			"sizeof(unsigned long int)=%u, sizeof(unsigned long)=%u, "
			"sizeof(unsigned long long)=%u\n",
			sizeof(void *), sizeof(unsigned int), sizeof(unsigned long int),
			sizeof(unsigned long), sizeof(unsigned long long));
	v_print("value of null-zero = %p\n", (void *)'\0');

	ret = vector_alloc(&v);
	v_testcase_int(tid, "vector_alloc", 0, ret);

	/* TODO: how/where are these strings allocated? Only have local scope
	 * (this main() function), right?
	 */
	ret = vector_append(v, "emil");
	v_testcase_int(tid, "vector_append", 0, ret);
	ret = vector_append(v, "hannes");
	v_testcase_int(tid, "vector_append", 0, ret);
	ret = vector_append(v, "lydia");
	v_testcase_int(tid, "vector_append", 0, ret);
	ret = vector_append(v, "olle");
	v_testcase_int(tid, "vector_append", 0, ret);
	ret = vector_append(v, "erik");
	v_testcase_int(tid, "vector_append", 0, ret);

	v_test("first round:\n");
	count = vector_count(v);
	for (i = 0; i < count; i++) {
		ret = vector_get(v, i, &e);
		v_testcase_int(tid, "vector_get", 0, ret);
		v_test("got element: %s\n", (char *)e);
	}

	ret = vector_delete(v, 1, &e);  //don't free e, statically allocated
	v_testcase_int(tid, "vector_delete", 0, ret);
	v_testcase_string(tid, "vector_delete", "hannes", (char *)e);
	ret = vector_delete(v, 3, &e);  //don't free e, statically allocated
	v_testcase_int(tid, "vector_delete", 0, ret);
	v_testcase_string(tid, "vector_delete", "erik", (char *)e);

	v_test("second round:\n");
	count = vector_count(v);
	for (i = 0; i < count; i++) {
		ret = vector_get(v, i, &e);
		v_testcase_int(tid, "vector_get", 0, ret);
		v_test("got element: %s\n", (char *)e);
	}

	ret = vector_delete(v, 3, &e);
	v_testcase_int(tid, "vector_delete", -1, ret);
	ret = vector_delete(v, 2, &e);  //don't free e, statically allocated
	v_testcase_int(tid, "vector_delete", 0, ret);
	v_testcase_string(tid, "vector_delete", "olle", (char *)e);
	ret = vector_delete(v, 0, &e);  //don't free e, statically allocated
	v_testcase_int(tid, "vector_delete", 0, ret);
	v_testcase_string(tid, "vector_delete", "emil", (char *)e);
	ret = vector_delete(v, 0, &e);  //don't free e, statically allocated
	v_testcase_int(tid, "vector_delete", 0, ret);
	v_testcase_string(tid, "vector_delete", "lydia", (char *)e);

	vector_free(v);

	return 0;
#endif
}
