#include <stdbool.h>

#if !defined(LIST_TAG) || !defined(LIST_ITEM_TYPE)
#error Missing type or tag definition
#endif

#define LIST_CONCAT(tag, method) tag##_##method
#define LIST_METHOD2(tag, method) LIST_CONCAT(tag, method)
#define LIST_METHOD(method) LIST_METHOD2(LIST_TAG, method)

#define LIST_NODE_TAG LIST_METHOD(node)

struct LIST_NODE_TAG {
    LIST_ITEM_TYPE item;
    struct LIST_NODE_TAG *next;
    struct LIST_NODE_TAG *prev;
};

typedef struct LIST_TAG {
    struct LIST_NODE_TAG *head;
    struct LIST_NODE_TAG *tail;
    size_t num_nodes;
} LIST_TAG;

void LIST_METHOD(init)(LIST_TAG *const list);
void LIST_METHOD(free)(LIST_TAG list);
void LIST_METHOD(push_front)(LIST_TAG *const list, LIST_ITEM_TYPE item);
void LIST_METHOD(push_back)(LIST_TAG *const list, LIST_ITEM_TYPE item);
bool LIST_METHOD(insert)(LIST_TAG *const list, size_t location, LIST_ITEM_TYPE item);
bool LIST_METHOD(pop_front)(LIST_TAG *const list, LIST_ITEM_TYPE *o_item);
bool LIST_METHOD(pop_back)(LIST_TAG *const list, LIST_ITEM_TYPE *o_item);
bool LIST_METHOD(remove)(LIST_TAG *const list, size_t location, LIST_ITEM_TYPE *o_item);

#undef LIST_TAG
#undef LIST_ITEM_TYPE
#undef LIST_CONCAT
#undef LIST_METHOD2
#undef LIST_METHOD
#undef LIST_HEAD_NAME