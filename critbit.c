/*
 * This code, like Prof. Bernstein's original code,
 * is released into the public domain.
 */

#include <sys/types.h>

#include <assert.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "critbit.h"

#ifdef CRITBIT_DEBUG
#define CRITBIT_ASSERT(a)		assert(a)
#else
#define CRITBIT_ASSERT(a)		(void)0
#endif

struct critbit_node {
	struct critbit_ref *child[2];
	uint32_t	byte;
	uint8_t		otherbits;
};

typedef size_t critbit_keylen_t(const uint8_t *a);

typedef const uint8_t *critbit_keybuf_t(const struct critbit_key *key);

typedef int critbit_keycmp_t(const uint8_t *a, const uint8_t *b, size_t blen);

size_t
critbit_node_size(void)
{
	return (sizeof(struct critbit_node));
}

static __inline void
critbit_node_free(struct critbit_tree *t, struct critbit_node *node)
{
	CRITBIT_ASSERT(t->ct_node_free != NULL);
	t->ct_node_free(t, node);
}

static __inline int
critbit_ref_is_internal(struct critbit_ref *ref)
{
	return (((intptr_t)ref) & 1);
}

static __inline struct critbit_node *
critbit_ref_get_node(struct critbit_ref *ref)
{
	CRITBIT_ASSERT(critbit_ref_is_internal(ref));

	return ((struct critbit_node *)(void *)(((uint8_t *)ref) - 1));
}

static __inline void
critbit_ref_set_node(struct critbit_ref **ref, struct critbit_node *node)
{
	*ref = (struct critbit_ref *)((uint8_t *)node + 1);
	CRITBIT_ASSERT(critbit_ref_is_internal(*ref));
}

static __inline void
critbit_ref_set_key(struct critbit_ref **ref, const struct critbit_key *key)
{
	*ref = (struct critbit_ref *)key;
	CRITBIT_ASSERT(!critbit_ref_is_internal(*ref));
}

static __inline struct critbit_key *
critbit_ref_get_key(struct critbit_ref *ref)
{
	struct critbit_key *key = (struct critbit_key *)ref;

	CRITBIT_ASSERT(!critbit_ref_is_internal(ref));

	return (key);
}

static __inline const uint8_t *
critbit_buf_keybuf(const struct critbit_key *key)
{
	return ((const uint8_t *)(key));
}

static __inline const uint8_t *
critbit_str_keybuf(const struct critbit_key *key)
{
	return (*(uint8_t **)(key));
}

static __inline size_t
critbit_buf_keylen(struct critbit_tree *t, const uint8_t *a CRITBIT_UNUSED)
{
	return (t->ct_keylen);
}

static __inline size_t
critbit_str_keylen(struct critbit_tree *t CRITBIT_UNUSED, const uint8_t *a)
{
	return (strlen((char *)a));
}

static __inline int
critbit_buf_keycmp(const uint8_t *a, const uint8_t *b, size_t blen)
{
	return (memcmp(a, b, blen));
}

static __inline int
critbit_str_keycmp(const uint8_t *a, const uint8_t *b, size_t blen)
{
	size_t alen;
	int rv;

	alen = strlen((char *)a);
	rv = alen - blen;
	if (rv != 0)
		return (rv);
	return (memcmp(a, b, blen));
}

void
critbit_init(struct critbit_tree *t, critbit_node_free_t *nfree,
    void *freearg, size_t keylen)
{
	t->ct_root = NULL;
	t->ct_keylen = keylen;
	t->ct_node_free = nfree;
	t->ct_free_arg = freearg;
}

static __inline struct critbit_key *
critbit_get_impl(struct critbit_tree *t, const void *key, size_t keylen,
    critbit_keycmp_t *keycmp, critbit_keybuf_t *keybuf)
{
	const uint8_t *ubytes = key;
	struct critbit_node *node;
	struct critbit_ref *ref;
	uint8_t c;

	ref = t->ct_root;
	if (ref == NULL)
		return (0);

	while (critbit_ref_is_internal(ref)) {
		node = critbit_ref_get_node(ref);

		c = 0;
		if (node->byte < keylen)
			c = ubytes[node->byte];

		const int direction = (1 + (node->otherbits | c)) >> 8;
		ref = node->child[direction];
	}

	if (keycmp(keybuf(critbit_ref_get_key(ref)), ubytes, keylen) == 0)
		return (critbit_ref_get_key(ref));

	return (NULL);
}

static __inline struct critbit_key *
critbit_insert_impl(struct critbit_tree *t, struct critbit_node *newnode,
    const struct critbit_key *key, size_t keylen, critbit_keybuf_t *keybuf)
{
	const uint8_t *const ubytes = keybuf(key);
	struct critbit_node *q;
	struct critbit_ref *p;
	const uint8_t *pkey;
	uint32_t newbyte;
	uint32_t newotherbits;
	uint8_t c;

	p = t->ct_root;
	if (p == NULL) {
		critbit_ref_set_key(&t->ct_root, key);
		critbit_node_free(t, newnode);
		return (NULL);
	}

	while (critbit_ref_is_internal(p)) {
		q = critbit_ref_get_node(p);

		c = 0;
		if (q->byte < keylen)
			c = ubytes[q->byte];

		const int direction = (1 + (q->otherbits | c)) >> 8;
		p = q->child[direction];
	}

	pkey = keybuf(critbit_ref_get_key(p));
	for (newbyte = 0; newbyte < keylen; ++newbyte) {
		if (pkey[newbyte] != ubytes[newbyte]) {
			newotherbits = pkey[newbyte] ^ ubytes[newbyte];
			goto different_byte_found;
		}
	}

	if (pkey[newbyte] != 0) {
		newotherbits = pkey[newbyte];
		goto different_byte_found;
	}

	critbit_node_free(t, newnode);
	return (critbit_ref_get_key(p));

different_byte_found:

	while ((newotherbits & (newotherbits - 1)) != 0)
		newotherbits &= newotherbits - 1;

	newotherbits ^= 255;
	const int newdirection = (1 + (newotherbits | pkey[newbyte])) >> 8;

	newnode->byte = newbyte;
	newnode->otherbits = newotherbits;
	critbit_ref_set_key(&newnode->child[1 - newdirection], key);

	struct critbit_ref **wherep = &t->ct_root;
	for (;;) {
		p = *wherep;
		if (!critbit_ref_is_internal(p))
			break;
		q = critbit_ref_get_node(p);
		if (q->byte > newbyte)
			break;
		if (q->byte == newbyte && q->otherbits > newotherbits)
			break;
		c = 0;
		if (q->byte < keylen)
			c = ubytes[q->byte];
		const int direction = (1 + (q->otherbits | c)) >> 8;
		wherep = q->child + direction;
	}

	newnode->child[newdirection] = *wherep;
	critbit_ref_set_node(wherep, newnode);

	return (NULL);
}

static __inline struct critbit_key *
critbit_remove_impl(struct critbit_tree *t, const void *key, size_t keylen,
    critbit_keycmp_t *keycmp, critbit_keybuf_t *keybuf)
{
	const uint8_t *ubytes = key;
	struct critbit_ref *p = t->ct_root;
	struct critbit_node *q = NULL;
	struct critbit_ref **wherep = &t->ct_root;
	struct critbit_ref **whereq = NULL;
	int direction = 0;

	if (p == NULL)
		return (NULL);

	while (critbit_ref_is_internal(p)) {
		whereq = wherep;
		q = critbit_ref_get_node(p);
		uint8_t c = 0;
		if (q->byte < keylen)
			c = ubytes[q->byte];
		direction = (1 + (q->otherbits | c)) >> 8;
		wherep = q->child + direction;
		p = *wherep;
	}

	if (keycmp(keybuf(critbit_ref_get_key(p)), ubytes, keylen) != 0)
		return (NULL);

	/* Remove p */

	if (whereq == NULL) {
		t->ct_root = NULL;
		return (critbit_ref_get_key(p));
	}

	*whereq = q->child[1 - direction];
	critbit_node_free(t, q);

	return (critbit_ref_get_key(p));
}

void *
critbit_buf_get(struct critbit_tree *t, const void *key)
{
	return (critbit_get_impl(t, key, critbit_buf_keylen(t, key),
	    critbit_buf_keycmp, critbit_buf_keybuf));
}

void *
critbit_buf_insert(struct critbit_tree *t,
    struct critbit_node *newnode, const void *key)
{
	return (critbit_insert_impl(t, newnode, (const struct critbit_key *)key,
	    critbit_buf_keylen(t, NULL), critbit_buf_keybuf));
}

void *
critbit_buf_remove(struct critbit_tree *t, const void *key)
{
	return (critbit_remove_impl(t, key, critbit_buf_keylen(t, key),
	    critbit_buf_keycmp, critbit_buf_keybuf));
}

void *
critbit_str_get(struct critbit_tree *t, const char *key)
{
	return (critbit_get_impl(t, key,
	    critbit_str_keylen(t, (const uint8_t *)key),
	    critbit_str_keycmp, critbit_str_keybuf));
}

void *
critbit_str_insert(struct critbit_tree *t,
    struct critbit_node *newnode, const char **key)
{
	return (critbit_insert_impl(t, newnode, (const struct critbit_key *)key,
	    critbit_str_keylen(t, (const uint8_t *)*key), critbit_str_keybuf));
}

void *
critbit_str_remove(struct critbit_tree *t, const char *key)
{
	return (critbit_remove_impl(t, key,
	    critbit_str_keylen(t, (const uint8_t *)key),
	    critbit_str_keycmp, critbit_str_keybuf));
}

#if 0
static void
traverse(void *top)
{
	uint8_t *p = top;

	if (1 & (intptr_t)p) {
		struct critbit_node *q = (void *)(p - 1);
		traverse(q->child[0]);
		traverse(q->child[1]);
		free(q);
	} else {
		free(p);
	}

}

void
critbit0_clear(struct critbit_tree *t)
{
	if (t->ct_root)
		traverse(t->ct_root);
	t->ct_root = NULL;
}

static int
allprefixed_traverse(uint8_t *top,
    int(*handle)(const char *, void *), void *arg)
{
	if (1 & (intptr_t)top) {
		struct critbit_node *q = (void *)(top - 1);
		for (int direction = 0; direction < 2; ++direction)
			switch (allprefixed_traverse(q->child[direction], handle, arg)) {
			case 1:
				break;
			case 0:
				return 0;
			default:
				return -1;
			}
		return 1;
	}

	return handle((const char *)top, arg);
}

int
critbit0_allprefixed(struct critbit_tree *t, const char *prefix,
    int(*handle)(const char *, void *), void *arg)
{
	const uint8_t *ubytes = (const void *)prefix;
	const size_t keylen = strlen(prefix);
	struct critbit_node *q;
	uint8_t *p = t->ct_root;
	uint8_t *top = p;

	if (p == NULL)
		return 1;

	while (1 & (intptr_t)p) {
		struct critbit_node *q = (void *)(p - 1);
		uint8_t c = 0;
		if (q->byte < keylen)
			c = ubytes[q->byte];
		const int direction = (1 + (q->otherbits | c)) >> 8;
		p = q->child[direction];
		if (q->byte < keylen)
			top = p;
	}

	for (size_t i = 0; i < keylen; ++i) {
		if (p[i] != ubytes[i])
			return 1;
	}


	return allprefixed_traverse(top, handle, arg);
}
#endif
