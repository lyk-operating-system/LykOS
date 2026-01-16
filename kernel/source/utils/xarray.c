#include "utils/xarray.h"

#include "assert.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "utils/likely.h"

static inline xa_node_t *xa_node_new()
{
    xa_node_t *n = (xa_node_t *)heap_alloc(sizeof(xa_node_t));
    if (!n)
        return NULL;
    memset(n, 0, sizeof(xa_node_t));
    return n;
}

void *xa_get(const xarray_t *xa, size_t index)
{
    xa_node_t *n = xa->root;
    if (!n)
        return NULL;

    for (int lvl = XA_LEVELS - 1; lvl > 0; lvl--)
    {
        size_t shift = lvl * XA_SHIFT;
        size_t slot = ((index >> shift) & XA_MASK);

        n = (xa_node_t *)n->slots[slot];
        if (!n)
            return NULL;
    }

    // leaf

    return n->slots[index & XA_MASK];
}

bool xa_insert(xarray_t *xa, size_t index, void *value)
{
    if (unlikely(!xa->root))
    {
        xa->root = xa_node_new();
        if (!xa->root)
            return false;
    }

    xa_node_t *n = xa->root;

    for (int lvl = XA_LEVELS - 1; lvl > 0 ; lvl--)
    {
        size_t shift = lvl * XA_SHIFT;
        size_t slot = (index >> shift) & XA_MASK;

        xa_node_t *child = (xa_node_t *)n->slots[slot];
        if (!child)
        {
            child = xa_node_new();
            if (!child)
                return false;
            n->slots[slot] = child;
            n->not_null_count++;
        }
        n = child;
    }

    // leaf

    size_t slot = index & XA_MASK;
    if (n->slots[slot] == NULL && value != NULL)
        n->not_null_count++;
    if (n->slots[slot] != NULL && value == NULL)
        n->not_null_count--;
    n->slots[slot] = value;

    return true;
}

void *xa_remove(xarray_t *xa, size_t index)
{
    if (unlikely(!xa->root))
        return NULL;

    xa_node_t *path[XA_LEVELS];
    size_t slots[XA_LEVELS];

    xa_node_t *n = xa->root;
    path[XA_LEVELS - 1] = n;

    for (size_t lvl = XA_LEVELS - 1; lvl > 0 ; lvl--)
    {
        size_t shift = lvl * XA_SHIFT;
        size_t slot = (index >> shift) & XA_MASK;

        slots[lvl] = slot;
        n = (xa_node_t *)n->slots[slot];
        if (!n)
            return NULL;
        path[lvl - 1] = n;
    }

    // leaf

    size_t slot = index & XA_MASK;
    slots[0] = slot;

    void *target = n->slots[slot];
    if (!target)
        return NULL;

    n->slots[slot] = NULL;
    n->not_null_count--;

    // prune

    for (int lvl = 0; lvl < (int)XA_LEVELS - 1; lvl++)
    {
        xa_node_t *cur = path[lvl];
        if (cur->not_null_count != 0)
            break;

        xa_node_t *parent = path[lvl + 1];
        unsigned ps = slots[lvl + 1];

        parent->slots[ps] = NULL;
        parent->not_null_count--;

        heap_free(cur);
    }

    if (xa->root && xa->root->not_null_count == 0)
    {
        heap_free(xa->root);
        xa->root = NULL;
    }

    return target;
}

// Marks

bool xa_get_mark(xarray_t *xa, size_t index, xa_mark_t mark)
{
    ASSERT(xa->root && mark <= XA_MARK_2);

    xa_node_t *n = xa->root;

    for (int lvl = XA_LEVELS - 1; lvl > 0; lvl--)
    {
        size_t slot = (index >> (lvl * XA_SHIFT)) & XA_MASK;
        n = (xa_node_t *)n->slots[slot];

        ASSERT(n);
    }

    return (n->mark[mark] & (1ULL << (index & XA_MASK))) != 0;
}

void xa_set_mark(xarray_t *xa, size_t index, xa_mark_t mark)
{
    ASSERT(xa->root && mark <= XA_MARK_2);

    xa_node_t *path[XA_LEVELS];
    xa_node_t *curr = xa->root;

    // descend
    for (int lvl = XA_LEVELS - 1; lvl > 0; lvl--)
    {
        path[lvl] = curr;
        curr = (xa_node_t *)curr->slots[(index >> (lvl * XA_SHIFT)) & XA_MASK];

        ASSERT(curr); // Cannot mark a NULL entry
    }
    path[0] = curr;

    // ascend
    for (int lvl = 0; lvl < (int)XA_LEVELS; lvl++)
    {
        size_t slot = (index >> (lvl * XA_SHIFT)) & XA_MASK;
        path[lvl]->mark[mark] |= (1ULL << slot);
    }
}

void xa_clear_mark(xarray_t *xa, size_t index, xa_mark_t mark)
{
    ASSERT(xa->root && mark <= XA_MARK_2);

    xa_node_t *path[XA_LEVELS];
    xa_node_t *curr = xa->root;

    // descend
    for (int lvl = XA_LEVELS - 1; lvl > 0; lvl--)
    {
        path[lvl] = curr;
        curr = (xa_node_t *)curr->slots[(index >> (lvl * XA_SHIFT)) & XA_MASK];

        ASSERT(curr);
    }
    path[0] = curr;

    // ascend
    for (int lvl = 0; lvl < (int)XA_LEVELS; lvl++)
    {
        size_t slot = (index >> (lvl * XA_SHIFT)) & XA_MASK;
        path[lvl]->mark[mark] &= ~(1ULL << slot);

        // If this node still has other slots marked stop clearing.
        if (path[lvl]->mark[mark] != 0)
            break;
    }
}

void *xa_find(xarray_t *xa, size_t *index, size_t max, xa_mark_t mark)
{
    ASSERT(xa->root && mark <= XA_MARK_2 && *index <= max);

    size_t curr_idx = *index;

    while (curr_idx <= max)
    {
        xa_node_t *n = xa->root;
        bool branch_match = true;

        // descend
        for (int lvl = XA_LEVELS - 1; lvl >= 0; lvl--)
        {
            size_t shift = lvl * XA_SHIFT;
            size_t slot = (curr_idx >> shift) & XA_MASK;

            uint64_t available_marks = n->mark[mark] & (~0ULL << slot);
            if (available_marks == 0)
            {
                size_t step = 1ull << shift;
                curr_idx = (curr_idx & ~(step - 1)) + step;
                branch_match = false;
                break;
            }

            size_t next_slot = __builtin_ctzll(available_marks);
            if (next_slot > slot)
            {
                size_t step = 1ull << shift;
                curr_idx = (curr_idx & ~(step - 1)) + (next_slot * step);
                branch_match = false;
                break;
            }

            // If not at the leaf, move to the next level
            if (lvl > 0)
            {
                n = (xa_node_t *)n->slots[slot];
                if (!n)
                {
                    // safety check
                    size_t step = 1ULL << shift;
                    curr_idx = (curr_idx & ~(step - 1)) + step;
                    branch_match = false;
                    break;
                }
            }
        }

        // Found a leaf entry?
        if (branch_match && curr_idx <= max)
        {
            *index = curr_idx;
            return n->slots[curr_idx & XA_MASK];
        }

        // Check for overflow if max is the largest possible size_t
        if (curr_idx == 0)
            break;
    }

    return NULL;
}
