#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "runtime0.h"
#include "map.h"

// approximatelly doubling primes; made with Mathematica command: Table[Prime[Floor[(1.7)^n]], {n, 3, 24}]
// prefixed with zero for the empty case.
static int doubling_primes[] = {0, 7, 19, 43, 89, 179, 347, 647, 1229, 2297, 4243, 7829, 14347, 26017, 47149, 84947, 152443, 273253, 488399, 869927, 1547173, 2745121, 4861607};

int get_doubling_prime_greater_or_equal_to(int x) {
    for (int i = 0; i < sizeof(doubling_primes) / sizeof(int); i++) {
        if (doubling_primes[i] >= x) {
            return doubling_primes[i];
        }
    }
    // ran out of primes in the table!
    // return something sensible, at least make it odd
    return x | 1;
}

/******************************************************************************/
/* map                                                                        */

void mp_map_init(mp_map_t *map, int n) {
    map->alloc = get_doubling_prime_greater_or_equal_to(n + 1);
    map->used = 0;
    map->all_keys_are_qstrs = 1;
    map->table = m_new0(mp_map_elem_t, map->alloc);
}

mp_map_t *mp_map_new(int n) {
    mp_map_t *map = m_new(mp_map_t, 1);
    mp_map_init(map, n);
    return map;
}

// Differentiate from mp_map_clear() - semantics is different
void mp_map_deinit(mp_map_t *map) {
    m_del(mp_map_elem_t, map->table, map->alloc);
    map->used = map->alloc = 0;
}

void mp_map_free(mp_map_t *map) {
    mp_map_deinit(map);
    m_del_obj(mp_map_t, map);
}

void mp_map_clear(mp_map_t *map) {
    map->used = 0;
    map->all_keys_are_qstrs = 1;
    machine_uint_t a = map->alloc;
    map->alloc = 0;
    map->table = m_renew(mp_map_elem_t, map->table, a, map->alloc);
    mp_map_elem_t nul = {NULL, NULL};
    for (uint i=0; i<map->alloc; i++) {
        map->table[i] = nul;
    }
}

static void mp_map_rehash(mp_map_t *map) {
    int old_alloc = map->alloc;
    mp_map_elem_t *old_table = map->table;
    map->alloc = get_doubling_prime_greater_or_equal_to(map->alloc + 1);
    map->used = 0;
    map->all_keys_are_qstrs = 1;
    map->table = m_new0(mp_map_elem_t, map->alloc);
    for (int i = 0; i < old_alloc; i++) {
        if (old_table[i].key != NULL) {
            mp_map_lookup(map, old_table[i].key, MP_MAP_LOOKUP_ADD_IF_NOT_FOUND)->value = old_table[i].value;
        }
    }
    m_del(mp_map_elem_t, old_table, old_alloc);
}

mp_map_elem_t* mp_map_lookup(mp_map_t *map, mp_obj_t index, mp_map_lookup_kind_t lookup_kind) {
    machine_uint_t hash;
    hash = mp_obj_hash(index);
    if (map->alloc == 0) {
        if (lookup_kind == MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
            mp_map_rehash(map);
        } else {
            return NULL;
        }
    }
    uint pos = hash % map->alloc;
    for (;;) {
        mp_map_elem_t *elem = &map->table[pos];
        if (elem->key == NULL) {
            // not in table
            if (lookup_kind == MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
                if (map->used + 1 >= map->alloc) {
                    // not enough room in table, rehash it
                    mp_map_rehash(map);
                    // restart the search for the new element
                    pos = hash % map->alloc;
                } else {
                    map->used += 1;
                    elem->key = index;
                    if (!MP_OBJ_IS_QSTR(index)) {
                        map->all_keys_are_qstrs = 0;
                    }
                    return elem;
                }
            } else {
                return NULL;
            }
        } else if (elem->key == index || (!map->all_keys_are_qstrs && mp_obj_equal(elem->key, index))) {
            // found it
            /* it seems CPython does not replace the index; try x={True:'true'};x[1]='one';x
            if (add_if_not_found) {
                elem->key = index;
            }
            */
            if (lookup_kind == MP_MAP_LOOKUP_REMOVE_IF_FOUND) {
                map->used--;
                // this leaks this memory (but see dict_get_helper)
                mp_map_elem_t *retval = m_new(mp_map_elem_t, 1);
                retval->key = elem->key;
                retval->value = elem->value;
                elem->key = NULL;
                elem->value = NULL;
                return retval;
            }
            return elem;
        } else {
            // not yet found, keep searching in this table
            pos = (pos + 1) % map->alloc;
        }
    }
}

void mp_map_walk(mp_map_t *map, bool (*fn)(void *param, mp_obj_t key, mp_obj_t value), void *param) {
    uint pos;
    if (!map) {
        return;
    }
    for (pos = 0; pos < map->alloc; pos++) {
        mp_map_elem_t *elem = &map->table[pos];
        if (elem->key != NULL) {
            if (!fn(param, elem->key, elem->value)) {
                return;
            }
        }
    }
}

/******************************************************************************/
/* set                                                                        */

void mp_set_init(mp_set_t *set, int n) {
    set->alloc = get_doubling_prime_greater_or_equal_to(n + 1);
    set->used = 0;
    set->table = m_new0(mp_obj_t, set->alloc);
}

static void mp_set_rehash(mp_set_t *set) {
    int old_alloc = set->alloc;
    mp_obj_t *old_table = set->table;
    set->alloc = get_doubling_prime_greater_or_equal_to(set->alloc + 1);
    set->used = 0;
    set->table = m_new0(mp_obj_t, set->alloc);
    for (int i = 0; i < old_alloc; i++) {
        if (old_table[i] != NULL) {
            mp_set_lookup(set, old_table[i], true);
        }
    }
    m_del(mp_obj_t, old_table, old_alloc);
}

mp_obj_t mp_set_lookup(mp_set_t *set, mp_obj_t index, mp_map_lookup_kind_t lookup_kind) {
    int hash;
    int pos;
    if (set->alloc == 0) {
        if (lookup_kind & MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
            mp_set_rehash(set);
        } else {
            return NULL;
        }
    }
    if (lookup_kind & MP_MAP_LOOKUP_FIRST) {
        hash = 0;
        pos = 0;
    } else {
        hash = mp_obj_hash(index);;
        pos = hash % set->alloc;
    }
    for (;;) {
        mp_obj_t elem = set->table[pos];
        if (elem == MP_OBJ_NULL) {
            // not in table
            if (lookup_kind & MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
                if (set->used + 1 >= set->alloc) {
                    // not enough room in table, rehash it
                    mp_set_rehash(set);
                    // restart the search for the new element
                    pos = hash % set->alloc;
                } else {
                    set->used += 1;
                    set->table[pos] = index;
                    return index;
                }
            } else if (lookup_kind & MP_MAP_LOOKUP_FIRST) {
                pos++;
            } else {
                return MP_OBJ_NULL;
            }
        } else if (lookup_kind & MP_MAP_LOOKUP_FIRST || mp_obj_equal(elem, index)) {
            // found it
            if (lookup_kind & MP_MAP_LOOKUP_REMOVE_IF_FOUND) {
                set->used--;
                set->table[pos] = NULL;
            }
            return elem;
        } else {
            // not yet found, keep searching in this table
            pos = (pos + 1) % set->alloc;
        }
    }
}

void mp_set_clear(mp_set_t *set) {
    set->used = 0;
    machine_uint_t a = set->alloc;
    set->alloc = 0;
    set->table = m_renew(mp_obj_t, set->table, a, set->alloc);
    for (uint i=0; i<set->alloc; i++) {
        set->table[i] = NULL;
    }
}
