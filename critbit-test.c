/*
 * This code, like Prof. Bernstein's original code,
 * is released into the public domain.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "critbit.h"

#define RB_COMPACT
#include "critbit-test-rb.h"
#include "critbit-test-tree.h"
#include "critbit-test-data.h"

#ifndef __unused
#define __unused
#endif

static const char *elems[] = {
	"a", "aa", "b", "bb", "ab", "ba", "aba", "bab", NULL
};

struct element {
	int pad;
	const char *k;
	int64_t kint;
	RB_ENTRY(element) rbentry;
	rb_node(struct element) nrb_link;
};

CRITBIT_HEAD_PROTOTYPE(eltree);
CRITBIT_GENERATE_STATIC(eltree, element, str, k);

CRITBIT_HEAD_PROTOTYPE(elinttree);
CRITBIT_GENERATE_STATIC(elinttree, element, int64, kint);

static int
rbtree_cmp(struct element *a, struct element *b)
{
	return strcmp(a->k, b->k);
}

static int
rbinttree_cmp(struct element *a, struct element *b)
{
	return a->kint - b->kint;
}

RB_HEAD(rbtree, element);
RB_GENERATE_STATIC(rbtree, element, rbentry, rbtree_cmp);

RB_HEAD(rbinttree, element);
RB_GENERATE_STATIC(rbinttree, element, rbentry, rbinttree_cmp);

typedef rbt(struct element) nrb_tree;
rb_gen(static __unused, nrb_, nrb_tree, struct element, nrb_link, rbinttree_cmp);

static void
std_free(void *arg __unused, void *node)
{
	free(node);
}

static void
no_free(void *arg __unused, void *node __unused)
{
}

static struct element *
el_alloc(void)
{
	return malloc(sizeof(struct element));
}

static void
test_contains(void)
{
	CRITBIT_HEAD(eltree) tree;
	struct element *el;
	struct critbit_node *nnode;
	int i;

	CRITBIT_INIT(eltree, &tree, std_free, NULL);

	for (i = 0; elems[i]; ++i) {
		el = el_alloc();
		el->k = elems[i];
		nnode = malloc(critbit_node_size());
		CRITBIT_INSERT(eltree, &tree, nnode, el);
	}

	for (i = 0; elems[i]; ++i) {
		el = CRITBIT_GET(eltree, &tree, elems[i]);
		if (el == NULL)
			abort();
	}
}

static void
test_delete(void)
{
	CRITBIT_HEAD(eltree) tree;
	struct element *el;
	struct critbit_node *nnode;
	int i, j;

	CRITBIT_INIT(eltree, &tree, std_free, NULL);

	for (i = 1; elems[i]; ++i) {

		for (j = 0; j < i; ++j) {
			el = el_alloc();
			el->k = elems[j];
			nnode = malloc(critbit_node_size());
			el = CRITBIT_INSERT(eltree, &tree, nnode, el);
			if (el != NULL)
				abort();
		}
		for (j = 0; j < i; ++j) {
			if (CRITBIT_GET(eltree, &tree, elems[j]) == NULL)
				abort();
		}
		for (j = 0; j < i; ++j) {
			el = CRITBIT_REMOVE(eltree, &tree, elems[j]);
			if (el == NULL)
				abort();
			free(el);
		}
		for (j = 0; j < i; ++j) {
			if (CRITBIT_GET(eltree, &tree, elems[j]) != NULL)
				abort();
		}
	}
}

const int loopcnt_init = 1000;
const int loopcnt_int_init = 2000;

static void
benchmark_result(const char *name, intmax_t n, struct timeval *tstart, struct timeval *tend)
{
        double t;

        t = tend->tv_sec - tstart->tv_sec;
        t += (double)(tend->tv_usec - tstart->tv_usec) / (double)1000000;
        printf("%16s: %jd iterations in %lf seconds; %lf iterations/s\n",
            name, n, t, (double)n/t);
}

static void
test_benchmark_critbit_int(void)
{
	CRITBIT_HEAD(elinttree) tree;
	struct element *el, *xel;
	char *xnode, *xnode_saved;
	int i, j, sz = critbit_node_size();

	xnode_saved = xnode = malloc(sz * loopcnt_int_init);
	xel = malloc(sizeof(*el) * loopcnt_int_init);

	struct timeval tstart, tend;
        gettimeofday(&tstart, NULL);

	for (i = 1; i < loopcnt_int_init; ++i) {

		CRITBIT_INIT(elinttree, &tree, no_free, NULL);
		xnode = xnode_saved;

		for (j = 0; j < i; ++j) {
			el = &xel[j];
			el->kint = j;
			el = CRITBIT_INSERT(elinttree, &tree,
			    (struct critbit_node *)xnode, el);
			xnode += sz;
			if (el != NULL)
				abort();
		}
		for (j = 0; j < i; ++j) {
			if (CRITBIT_GET(elinttree, &tree, j) == NULL)
				abort();
			if (CRITBIT_GET(elinttree, &tree, j) == NULL)
				abort();
			if (CRITBIT_GET(elinttree, &tree, j) == NULL)
				abort();
			if (CRITBIT_GET(elinttree, &tree, j) == NULL)
				abort();
		}
	}

        gettimeofday(&tend, NULL);

	benchmark_result("critbit int", loopcnt_int_init, &tstart, &tend);
}

static __inline uint32_t
hashint(uintptr_t h)
{
	h = (~h) + (h << 18);
	h = h ^ (h >> 31);
	h = h * 21;
	h = h ^ (h >> 11);
	h = h + (h << 6);
	h = h ^ (h >> 22);
	return (h);
}

static void
test_benchmark_critbit_hash_int(void)
{
	CRITBIT_HEAD(elinttree) tree;
	struct element *el, *xel;
	char *xnode, *xnode_saved;
	int i, j, *k, sz = critbit_node_size();

	xnode_saved = xnode = malloc(sz * loopcnt_int_init);
	xel = malloc(sizeof(*el) * loopcnt_int_init);
	k = malloc(sizeof(int) * loopcnt_int_init);

	struct timeval tstart, tend;
        gettimeofday(&tstart, NULL);

#define KEY(a) k[(a)]

	for (i = 1; i < loopcnt_int_init; ++i) {

		CRITBIT_INIT(elinttree, &tree, no_free, NULL);
		xnode = xnode_saved;

		for (j = 0; j < i; ++j) {
			el = &xel[j];
			k[j] = hashint(j);
			el->kint = KEY(j);
			el = CRITBIT_INSERT(elinttree, &tree,
			    (struct critbit_node *)xnode, el);
			xnode += sz;
			if (el != NULL)
				abort();
		}
		for (j = 0; j < i; ++j) {
			if (CRITBIT_GET(elinttree, &tree, KEY(j)) == NULL)
				abort();
			if (CRITBIT_GET(elinttree, &tree, KEY(j)) == NULL)
				abort();
			if (CRITBIT_GET(elinttree, &tree, KEY(j)) == NULL)
				abort();
			if (CRITBIT_GET(elinttree, &tree, KEY(j)) == NULL)
				abort();
		}
	}

        gettimeofday(&tend, NULL);

	benchmark_result("critbit hash int", loopcnt_int_init, &tstart, &tend);
}

static void
test_benchmark_rbtree_int(void)
{
	struct rbinttree tree;
	struct element find, *el, *xel;
	int i, j;

	xel = malloc(sizeof(*el) * loopcnt_int_init);

	struct timeval tstart, tend;
        gettimeofday(&tstart, NULL);

	for (i = 1; i < loopcnt_int_init; ++i) {

		RB_INIT(&tree);

		for (j = 0; j < i; ++j) {
			el = &xel[j];
			el->kint = j;
			el = RB_INSERT(rbinttree, &tree, el);
			if (el != NULL)
				abort();
		}
		for (j = 0; j < i; ++j) {
			find.kint = j;
			if (RB_FIND(rbinttree, &tree, &find) == NULL)
				abort();
			find.kint = j;
			if (RB_FIND(rbinttree, &tree, &find) == NULL)
				abort();
			find.kint = j;
			if (RB_FIND(rbinttree, &tree, &find) == NULL)
				abort();
			find.kint = j;
			if (RB_FIND(rbinttree, &tree, &find) == NULL)
				abort();
		}
	}

        gettimeofday(&tend, NULL);

	benchmark_result("rbtree int", loopcnt_int_init, &tstart, &tend);
}

static void
test_benchmark_nrbtree_int(void)
{
	nrb_tree tree;
	struct element find, *el, *xel;
	int i, j;

	xel = malloc(sizeof(*el) * loopcnt_int_init);

	struct timeval tstart, tend;
        gettimeofday(&tstart, NULL);

	for (i = 1; i < loopcnt_int_init; ++i) {

		nrb_new(&tree);

		for (j = 0; j < i; ++j) {
			el = &xel[j];
			el->kint = j;
			nrb_insert(&tree, el);
		}
		for (j = 0; j < i; ++j) {
			find.kint = j;
			if (nrb_search(&tree, &find) == NULL)
				abort();
			find.kint = j;
			if (nrb_search(&tree, &find) == NULL)
				abort();
			find.kint = j;
			if (nrb_search(&tree, &find) == NULL)
				abort();
			find.kint = j;
			if (nrb_search(&tree, &find) == NULL)
				abort();
		}
	}

        gettimeofday(&tend, NULL);

	benchmark_result("nrbtree int", loopcnt_int_init, &tstart, &tend);
}

static void
test_benchmark_critbit(void)
{
	CRITBIT_HEAD(eltree) tree;
	struct element *el, *test_data_el;
	struct critbit_node *nnode;
	int cnt;
	int i;

	CRITBIT_INIT(eltree, &tree, no_free, NULL);

	for (cnt = 0; test_data[cnt]; ) {
		cnt++;
	}
	test_data_el = malloc(cnt * sizeof(*test_data_el));

	int loopcnt = loopcnt_init;
	struct timeval tstart, tend;

        gettimeofday(&tstart, NULL);

again:
	for (i = 0; i < cnt; ++i) {
		el = &test_data_el[i];
		el->k = test_data[i];
		nnode = malloc(critbit_node_size());
		CRITBIT_INSERT(eltree, &tree, nnode, el);
	}

	for (i = cnt - 1; i >= 0; i--) {
		el = CRITBIT_GET(eltree, &tree, test_data[i]);
		if (el == NULL)
			abort();
	}

	for (i = 3; i < cnt; i += 5) {
		el = CRITBIT_REMOVE(eltree, &tree, test_data[i]);
		if (el == NULL)
			abort();
	}

	for (i = 2; i < cnt; i += 3) {
		el = CRITBIT_GET(eltree, &tree, test_data[i]);
	}

	for (i = 0; i < cnt; i ++) {
		el = CRITBIT_REMOVE(eltree, &tree, test_data[i]);
	}

	if (loopcnt-- > 0)
		goto again;
        gettimeofday(&tend, NULL);

	benchmark_result("critbit", loopcnt_init, &tstart, &tend);
}

static void
test_benchmark_rbtree(void)
{
	struct rbtree tree;
	struct element find, *el, *test_data_el;
	int cnt;
	int i;

	RB_INIT(&tree);

	for (cnt = 0; test_data[cnt]; ) {
		cnt++;
	}
	test_data_el = malloc(cnt * sizeof(*test_data_el));

	int loopcnt = loopcnt_init;
	struct timeval tstart, tend;

        gettimeofday(&tstart, NULL);

again:
	for (i = 0; i < cnt; ++i) {
		el = &test_data_el[i];
		el->k = test_data[i];
		RB_INSERT(rbtree, &tree, el);
	}

	for (i = cnt - 1; i >= 0; i--) {
		find.k = test_data[i];
		el = RB_FIND(rbtree, &tree, &find);
		if (el == NULL)
			abort();
	}

	for (i = 3; i < cnt; i += 5) {
		find.k = test_data[i];
		el = RB_FIND(rbtree, &tree, &find);
		if (el == NULL)
			abort();
		el = RB_REMOVE(rbtree, &tree, el);
	}

	for (i = 2; i < cnt; i += 3) {
		find.k = test_data[i];
		el = RB_FIND(rbtree, &tree, &find);
	}

	for (i = 0; i < cnt; i ++) {
		find.k = test_data[i];
		el = RB_FIND(rbtree, &tree, &find);
		if (el != NULL)
			RB_REMOVE(rbtree, &tree, el);
	}

	if (loopcnt-- > 0)
		goto again;
        gettimeofday(&tend, NULL);

	benchmark_result("rbtree", loopcnt_init, &tstart, &tend);
}

#if 0
static int
allprefixed_cb(const char *elem, void *arg) {
  set<string> *a = (set<string> *) arg;
  a->insert(elem);

  return 1;
}

static void
test_allprefixed() {
  critbit0_tree tree = {0};

  static const char *elems[] = {"a", "aa", "aaz", "abz", "bba", "bbc", "bbd", NULL};

  for (unsigned i = 0; elems[i]; ++i) critbit0_insert(&tree, elems[i]);

  set<string> a;

  critbit0_allprefixed(&tree, "a", allprefixed_cb, &a);
  if (a.size() != 4 ||
      a.find("a") == a.end() ||
      a.find("aa") == a.end() ||
      a.find("aaz") == a.end() ||
      a.find("abz") == a.end()) {
    abort();
  }
  a.clear();

  critbit0_allprefixed(&tree, "aa", allprefixed_cb, &a);
  if (a.size() != 2 ||
      a.find("aa") == a.end() ||
      a.find("aaz") == a.end()) {
    abort();
  }
  a.clear();

  critbit0_clear(&tree);
}
#endif

int
main(void)
{
	test_contains();
	test_delete();
	test_benchmark_critbit_int();
	test_benchmark_critbit_hash_int();
	test_benchmark_rbtree_int();
	test_benchmark_nrbtree_int();
	test_benchmark_critbit();
	test_benchmark_rbtree();
#if 0
	// test_allprefixed();
#endif

	return 0;
}
