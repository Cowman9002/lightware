#include <malloc.h>
#include <stdbool.h>

// #define LIST_TAG IntList
// #define LIST_ITEM_TYPE int

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

void LIST_METHOD(init)(LIST_TAG *const list) {
    list->head      = NULL;
    list->tail      = NULL;
    list->num_nodes = 0;
}

void LIST_METHOD(free)(LIST_TAG list) {
    LIST_NODE_TAG *node = list.head;
    LIST_NODE_TAG *node2;

    while (node != NULL) {
#ifdef LIST_ITEM_FREE_FUNC
        LIST_ITEM_FREE_FUNC(node->item);
#endif
        node2 = node;
        node  = node->next;
        free(node2);
    }
}

void LIST_METHOD(push_front)(LIST_TAG *const list, LIST_ITEM_TYPE item) {
    ++list->num_nodes;
    LIST_NODE_TAG *new_node = (LIST_NODE_TAG *)malloc(sizeof(*list->head));
    *new_node               = (LIST_NODE_TAG){ .item = item, .next = NULL, .prev = NULL };

    if (list->head == NULL && list->tail == NULL) {
        list->head = new_node;
        list->tail = list->head;
    } else {
        new_node->next   = list->head;
        list->head->prev = new_node;
        list->head       = new_node;
    }
}

void LIST_METHOD(push_back)(LIST_TAG *const list, LIST_ITEM_TYPE item) {
    ++list->num_nodes;
    LIST_NODE_TAG *new_node = (LIST_NODE_TAG *)malloc(sizeof(*list->head));
    *new_node               = (LIST_NODE_TAG){ .item = item, .next = NULL, .prev = NULL };

    if (list->head == NULL && list->tail == NULL) {
        list->head = new_node;
        list->tail = list->head;
    } else {
        list->tail->next = new_node;
        new_node->prev   = list->tail;
        list->tail       = new_node;
    }
}

bool LIST_METHOD(insert)(LIST_TAG *const list, size_t location, LIST_ITEM_TYPE item) {
    if (location == 0) {
        LIST_METHOD(push_front)
        (list, item);
    } else if (location == list->num_nodes) {
        LIST_METHOD(push_back)
        (list, item);
    } else if (location > list->num_nodes) {
        return false;
    } else {
        ++list->num_nodes;
        LIST_NODE_TAG *new_node = (LIST_NODE_TAG *)malloc(sizeof(*list->head));
        *new_node               = (LIST_NODE_TAG){ .item = item, .next = NULL, .prev = NULL };

        LIST_NODE_TAG *attach_point = list->head;
        --location;
        for (; location > 0; --location) {
            attach_point = attach_point->next;
        }

        new_node->next           = attach_point->next;
        attach_point->next->prev = new_node;
        attach_point->next       = new_node;
    }

    return true;
}

bool LIST_METHOD(pop_front)(LIST_TAG *const list, LIST_ITEM_TYPE *o_item) {
    if (list->head == NULL) {
        return false;
    } else {
        if (o_item != NULL) {
            *o_item = list->head->item;
        }

        LIST_NODE_TAG *old_head = list->head;
        list->head              = list->head->next;
        if (list->head != NULL) {
            list->head->prev = NULL;
        } else {
            list->tail = NULL;
        }
        free(old_head);

        --list->num_nodes;

        return true;
    }
}

bool LIST_METHOD(pop_back)(LIST_TAG *const list, LIST_ITEM_TYPE *o_item) {
    if (list->tail == NULL) {
        return false;
    } else {
        if (o_item != NULL) {
            *o_item = list->tail->item;
        }

        LIST_NODE_TAG *old_tail = list->tail;
        list->tail              = list->tail->prev;
        if (list->tail != NULL) {
            list->tail->next = NULL;
        } else {
            list->head = NULL;
        }
        free(old_tail);

        --list->num_nodes;

        return true;
    }
}

bool LIST_METHOD(remove_at)(LIST_TAG *const list, size_t location, LIST_ITEM_TYPE *o_item) {
    if (location == 0) {
        return LIST_METHOD(pop_front)(list, o_item);
    } else if (location == list->num_nodes) {
        return LIST_METHOD(pop_back)(list, o_item);
    } else if (location > list->num_nodes) {
        return false;
    } else {
        LIST_NODE_TAG *remove_point = list->head->next;
        --location;
        for (; location > 0; --location) {
            remove_point = remove_point->next;
        }

        if (o_item != NULL) {
            *o_item = remove_point->item;
        }

        remove_point->prev->next = remove_point->next;
        remove_point->next->prev = remove_point->prev;
        free(remove_point);

        --list->num_nodes;
        return true;
    }
}

bool LIST_METHOD(remove)(LIST_TAG *const list, LIST_ITEM_TYPE *item) {
    if(list->num_nodes == 0) return false;

    if (&list->head->item == item) {
        return LIST_METHOD(pop_front)(list, NULL);
    } else if (&list->tail->item == item) {
        return LIST_METHOD(pop_back)(list, NULL);
    }

    LIST_NODE_TAG *remove_point = list->head->next;
    for (; remove_point != NULL; remove_point = remove_point->next) {
        if (&remove_point->item == item) break;
    }

    if (remove_point != NULL) {
        remove_point->prev->next = remove_point->next;
        remove_point->next->prev = remove_point->prev;
        free(remove_point);
        --list->num_nodes;
        return true;
    }

    return false;
}

unsigned LIST_METHOD(find_index)(LIST_TAG *const list, LIST_ITEM_TYPE *item) {
    if(list->num_nodes == 0) return 0;

    if (&list->head->item == item) {
        return 0;
    } else if (&list->tail->item == item) {
        return list->num_nodes - 1;
    }

    LIST_NODE_TAG *node = list->head->next;
    for (unsigned i = 0; node != NULL; node = node->next, ++i) {
        if (&node->item == item) {
            return i;
        }
    }

    return list->num_nodes;
}

#undef LIST_TAG
#undef LIST_ITEM_TYPE
#undef LIST_CONCAT
#undef LIST_METHOD2
#undef LIST_METHOD
#undef LIST_HEAD_NAME

#ifdef LIST_ITEM_FREE_FUNC
#undef LIST_ITEM_FREE_FUNC
#endif