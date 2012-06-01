/*
 * This code, like Prof. Bernstein's original code,
 * is released into the public domain.
 */

#ifndef CRITBIT_H_
#define CRITBIT_H_

#include <stddef.h>

#ifndef CRITBIT_UNUSED
#ifndef __unused
#define CRITBIT_UNUSED
#else
#define CRITBIT_UNUSED		__unused
#endif
#endif

#ifndef __XCONCAT
#define __XCONCAT1(x,y)		x ## y
#define __XCONCAT(x,y)		__XCONCAT1(x,y)
#endif

__BEGIN_DECLS

struct critbit_key;
struct critbit_node;
struct critbit_ref;
struct critbit_tree;

typedef void critbit_node_free_t(void *arg, void *node);

struct critbit_tree {
	struct critbit_ref	*ct_root;
	size_t			ct_keylen;
	void			*ct_free_arg;
	critbit_node_free_t	*ct_node_free;
};

void critbit_init(struct critbit_tree *t, critbit_node_free_t *nfree,
    void *freearg, size_t keylen);

int critbit_empty(struct critbit_tree *t);

size_t critbit_node_size(void);

void critbit_buf_init(struct critbit_tree *t, critbit_node_free_t *nfree,
    void *freearg, size_t keylen);

void *critbit_buf_get(struct critbit_tree *t, const void *key);

void *critbit_buf_insert(struct critbit_tree *t,
    struct critbit_node *newnode, const void *key);

void *critbit_buf_remove(struct critbit_tree *t, const void *key);

void critbit_str_init(struct critbit_tree *t,
    critbit_node_free_t *nfree, void *freearg);

void *critbit_str_get(struct critbit_tree *t, const char *key);

void *critbit_str_insert(struct critbit_tree *t,
    struct critbit_node *newnode, const char **key);

void *critbit_str_remove(struct critbit_tree *t, const char *key);

#define CRITBIT_HEAD(name)						\
struct name##_critbit_head

#define CRITBIT_HEAD_PROTOTYPE(name)					\
CRITBIT_HEAD(name) {							\
	struct critbit_tree treehead;					\
}

#define CRITBIT_PROTOTYPE_INLINE(name, type, keytype)			\
CRITBIT_PROTOTYPE_INTERNAL(name, type, keytype,				\
    CRITBIT_UNUSED static __inline)

#define CRITBIT_GENERATE_INLINE(name, type, keytype, field)		\
CRITBIT_GENERATE_INTERNAL(name, type, keytype, field,			\
    CRITBIT_UNUSED static __inline)

#define CRITBIT_PROTOTYPE_STATIC(name, type, keytype)			\
CRITBIT_PROTOTYPE_INTERNAL(name, type, keytype, CRITBIT_UNUSED static)

#define CRITBIT_GENERATE_STATIC(name, type, keytype, field)		\
CRITBIT_GENERATE_INTERNAL(name, type, keytype, field, CRITBIT_UNUSED static)

#define CRITBIT_PROTOTYPE_INTERNAL(name, type, keytype, attr)		\
CRITBIT_HEAD_PROTOTYPE(name);						\
attr size_t name##_critbit_keylen(void);				\
attr struct type *name##_critbit_get(CRITBIT_HEAD(name) *head,		\
    CRITBIT_KEYTYPE_##keytype key);					\
attr struct type *name##_critbit_insert(CRITBIT_HEAD(name) *head,	\
    struct critbit_node *newnode, struct type *entry);			\
attr struct type *name##_critbit_remove(CRITBIT_HEAD(name) *head,	\
    CRITBIT_KEYTYPE_##keytype key);

#define CRITBIT_GENERATE_INTERNAL(name, type, keytype, field, attr)	\
attr size_t								\
name##_critbit_keylen(void)						\
{									\
	/* ignored by critbit_str_* */					\
	return (sizeof(CRITBIT_KEYTYPE_##keytype));			\
}									\
									\
attr struct type *							\
name##_critbit_get(CRITBIT_HEAD(name) *head,				\
    CRITBIT_KEYTYPE_##keytype key)					\
{									\
	void *r = CRITBIT_METHOD(keytype, get)(&head->treehead,		\
	    CRITBIT_KEYREF_##keytype(key));				\
	return (CRITBIT_CAST(type, field, r));				\
}									\
									\
attr struct type *name##_critbit_insert(CRITBIT_HEAD(name) *head,	\
    struct critbit_node *newnode, struct type *entry)			\
{									\
	void *r = CRITBIT_METHOD(keytype,insert)(&head->treehead,	\
	    newnode, &(entry->field));					\
	return (CRITBIT_CAST(type, field, r));				\
}									\
									\
attr struct type *name##_critbit_remove(CRITBIT_HEAD(name) *head,	\
    CRITBIT_KEYTYPE_##keytype key)					\
{									\
	void *r = CRITBIT_METHOD(keytype,remove)(&head->treehead,	\
	    CRITBIT_KEYREF_##keytype(key));				\
	return (CRITBIT_CAST(type, field, r));				\
}

#define CRITBIT_METHOD(keytype, method)					\
__XCONCAT(__XCONCAT(critbit_,keytype),_##method)

#define CRITBIT_CAST(type, field, val)					\
(val == NULL ? NULL : (struct type *)(void *)(((char *)val -		\
    offsetof(struct type, field))))

#define CRITBIT_INIT(name, head, nfree, freearg)			\
critbit_init(&((head)->treehead), (nfree), (freearg), name##_critbit_keylen())

#define critbit_int32			critbit_buf
#define critbit_int64			critbit_buf
#define critbit_intptr			critbit_buf
#define critbit_ptr			critbit_buf

#define CRITBIT_KEYREF_buf(a)		(a)
#define CRITBIT_KEYREF_str(a)		(a)
#define CRITBIT_KEYREF_scalar(a)	(&(a))
#define CRITBIT_KEYREF_int32(a)		CRITBIT_KEYREF_scalar(a)
#define CRITBIT_KEYREF_int64(a)		CRITBIT_KEYREF_scalar(a)
#define CRITBIT_KEYREF_intptr(a)	CRITBIT_KEYREF_scalar(a)
#define CRITBIT_KEYREF_ptr(a)		CRITBIT_KEYREF_scalar((intptr_t)(a))

#define CRITBIT_KEYTYPE_buf		const void *
#define CRITBIT_KEYTYPE_str		const char *
#define CRITBIT_KEYTYPE_int32		int32_t
#define CRITBIT_KEYTYPE_int64		int64_t
#define CRITBIT_KEYTYPE_intptr		intptr_t
#define CRITBIT_KEYTYPE_ptr		const void *

#define CRITBIT_GET(name, tree, key)					\
name##_critbit_get((tree), (key))

#define CRITBIT_INSERT(name, tree, newnode, key)			\
name##_critbit_insert((tree), (newnode), (key))

#define CRITBIT_REMOVE(name, tree, key)					\
name##_critbit_remove((tree), (key))

__END_DECLS

#endif
