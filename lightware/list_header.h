#include <stdbool.h>
#include <stddef.h>

#if !defined(LIST_TAG) || !defined(LIST_ITEM_TYPE)
#error Missing type or tag definition
#endif

#define LIST_CONCAT_STRUCT(tag, op) tag####op
#define LIST_STRUCT2(tag, op) LIST_CONCAT_STRUCT(tag, op)
#define LIST_STRUCT(op) LIST_STRUCT2(LIST_TAG, op)

#define LIST_CONCAT(tag, method) tag##_##method
#define LIST_METHOD2(tag, method) LIST_CONCAT(tag, method)
#define LIST_METHOD(method) LIST_METHOD2(LIST_TAG, method)

#define LIST_NODE_TAG LIST_STRUCT(Node)

typedef struct LIST_NODE_TAG {
    LIST_ITEM_TYPE item;
    struct LIST_NODE_TAG *next;
    struct LIST_NODE_TAG *prev;
}LIST_NODE_TAG;

typedef struct LIST_TAG {
    struct LIST_NODE_TAG *head;
    struct LIST_NODE_TAG *tail;
    size_t num_nodes;
} LIST_TAG;

LIGHTWARE_API void LIST_METHOD(init)(LIST_TAG *const list);
LIGHTWARE_API void LIST_METHOD(free)(LIST_TAG list);
LIGHTWARE_API void LIST_METHOD(push_front)(LIST_TAG *const list, LIST_ITEM_TYPE item);
LIGHTWARE_API void LIST_METHOD(push_back)(LIST_TAG *const list, LIST_ITEM_TYPE item);
LIGHTWARE_API bool LIST_METHOD(insert)(LIST_TAG *const list, size_t location, LIST_ITEM_TYPE item);
LIGHTWARE_API bool LIST_METHOD(pop_front)(LIST_TAG *const list, LIST_ITEM_TYPE *o_item);
LIGHTWARE_API bool LIST_METHOD(pop_back)(LIST_TAG *const list, LIST_ITEM_TYPE *o_item);
LIGHTWARE_API bool LIST_METHOD(remove_at)(LIST_TAG *const list, size_t location, LIST_ITEM_TYPE *o_item);
LIGHTWARE_API bool LIST_METHOD(remove)(LIST_TAG *const list, LIST_ITEM_TYPE *item);
LIGHTWARE_API size_t LIST_METHOD(find_index)(LIST_TAG *const list, LIST_ITEM_TYPE *item);
LIGHTWARE_API LIST_ITEM_TYPE *LIST_METHOD(get)(LIST_TAG *const list, size_t location);

#undef LIST_TAG
#undef LIST_ITEM_TYPE
#undef LIST_CONCAT
#undef LIST_METHOD2
#undef LIST_METHOD
#undef LIST_HEAD_NAME