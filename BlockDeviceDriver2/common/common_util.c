/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common_util.c
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "discoC_sys_def.h"
#include "common_util.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
long __must_check IS_ERR_OR_NULL (__force const void *ptr)
{
    return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}
#endif

/*
 * __dms_list_add_chk: check whether arguments is NULL when add entry to list
 * @new: new entry to be added
 * @prev: prev entry of location to be added
 * @next: next entry of location to be added
 *
 * Return: whether any pointer is NULL
 *         0: success
 *        ~0: execute post handling fail
 */
static int32_t __dms_list_add_chk (struct list_head *new,
        struct list_head *prev, struct list_head *next)
{
#ifdef LINUX_LIST_DEBUG
    if (unlikely(next->prev != prev)) {
        printk(KERN_ERR "__dms_list_add_chk corruption. "
                "next->prev should be prev (%p), but was %p. (next=%p).\n",
                prev, next->prev, next);
        return -1;
    }

    if (unlikely(prev->next != next)) {
        printk(KERN_ERR "__dms_list_add_chk corruption. "
                "prev->next should be next (%p), but was %p. (prev=%p).\n",
                next, prev->next, prev);
        return -2;
    }
#endif

    return 0;
}

/*
 * dms_list_add: A wrapper of list_add, check whether link list broken before add
 * @new: new entry to be added
 * @head: head of link list
 *
 * Return: add success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
int32_t dms_list_add (struct list_head *new, struct list_head *head)
{
    int32_t ret;

    if (unlikely(ret = __dms_list_add_chk(new, head, head->next))) {
        return ret;
    }

    list_add(new, head);

    return ret;
}

/*
 * dms_list_add_tail: A wrapper of list_add_tail, check whether link list broken before add
 * @new: new entry to be added
 * @head: head of link list
 *
 * Return: add success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
int32_t dms_list_add_tail (struct list_head *new, struct list_head *head)
{
    int32_t ret;

    if (unlikely(ret = __dms_list_add_chk(new, head->prev, head))) {
        return ret;
    }

    list_add_tail(new, head);

    return ret;
}

/*
 * __dms_list_del_chk: check whether arguments is NULL when del entry from list
 * @entry: target entry to be delete
 *
 * Return: delete success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
static int32_t __dms_list_del_chk (struct list_head *entry)
{
#ifdef LINUX_LIST_DEBUG
    if (unlikely(entry->prev->next != entry)) {
        printk(KERN_ERR
                "list_del corruption. prev->next should be %p, but was %p\n",
                entry, entry->prev->next);
        return -1;
    }

    if (unlikely(entry->next->prev != entry)) {
        printk(KERN_ERR
                "list_del corruption. next->prev should be %p, but was %p\n",
                entry, entry->next->prev);
        return -2;
    }
#endif

    return 0;
}

/*
 * dms_list_del: A wrapper of list_del, check whether link list broken before del
 * @entry: target entry to be delete
 *
 * Return: delete success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
int32_t dms_list_del (struct list_head *entry)
{
    int32_t ret;

    if (unlikely(ret = __dms_list_del_chk(entry))) {
        return ret;
    }

    list_del(entry);

    return ret;
}

/*
 * __dms_list_move_tail_chk: check whether arguments is NULL when move entry from list to another list
 * @new: target entry to be move
 * @head: new list to be added
 *
 * Return: move success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
static int32_t __dms_list_move_tail_chk (struct list_head *new, struct list_head *head)
{
#ifdef LINUX_LIST_DEBUG
    if (__dms_list_del_chk(new)) {
        printk(KERN_ERR "list_del corruption when list move tail");
        return -1;
    }

    if (__dms_list_add_chk(new, head->prev, head)) {
        printk(KERN_ERR "list_add_tail corruption when list move tail");
        return -2;
    }
#endif

    return 0;
}

/*
 * dms_list_move_tail: A wrapper of list_move_tail,
 *                     check whether link list broken before move
 * @new: target entry to be move
 * @head: new list to be added
 *
 * Return: move success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
int32_t dms_list_move_tail (struct list_head *new, struct list_head *head)
{
    int32_t ret;

    if (unlikely(ret = __dms_list_move_tail_chk(new, head))) {
        return ret;
    }

    list_move_tail(new, head);

    return ret;
}

/*
 * __dms_list_move_head_chk: check whether arguments is NULL when move entry from list to another list
 * @new: target entry to be move
 * @head: new list to be added
 *
 * Return: move success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
static int32_t __dms_list_move_head_chk (struct list_head *new, struct list_head *head)
{
#ifdef LINUX_LIST_DEBUG
    if (__dms_list_del_chk(new)) {
        printk(KERN_ERR "list_del corruption when list move tail");
        return -1;
    }

    if (__dms_list_add_chk(new, head, head->next)) {
        printk(KERN_ERR "list_add_tail corruption when list move tail");
        return -2;
    }
#endif

    return 0;
}

/*
 * dms_list_move_head: A wrapper of del and add to head,
 * @new: target entry to be move
 * @head: new list to be added
 *
 * Return: move success or fail
 *         0: success
 *        ~0: execute post handling fail
 */
int32_t dms_list_move_head (struct list_head *new, struct list_head *head)
{
    int32_t ret;

    if (unlikely(ret = __dms_list_move_head_chk(new, head))) {
        return ret;
    }

    list_del(new);
    list_add(new, head);

    return ret;
}

/*
 * cmp_triple_int: compare 3 integers and return compare result
 * @num1: 1st number to compare
 * @num2: 2nd number to compare
 * @num3: 3rd number to compare
 *
 * Return: compare result
 *         cmp_1st_Min: 1st number is min value
 *         cmp_2nd_Min: 2nd number is min value
 *         cmp_3rd_Min: 3th number is min value
 */
triple_cmp_type_t cmp_triple_int (int32_t num1, int32_t num2, int32_t num3)
{
    triple_cmp_type_t res;

    res = cmp_1st_Min;

    if (num1 >= num2) {
        res = (num2 >= num3 ? cmp_3rd_Min : cmp_2nd_Min);
    } else {
        res = (num1 >= num3 ? cmp_3rd_Min : cmp_1st_Min);
    }

    return res;
}

/*
 * head_sect_skip: counting how many continuous bit = 1 in a byte from lsb
 * @mask_val: byte value to check
 *
 * Return: number of bit = 1
 */
int32_t head_sect_skip (uint8_t mask_val)
{
    int32_t num_of_sec = 0;
    uint64_t mask_bit_start;

    for (mask_bit_start = 0; mask_bit_start < BIT_LEN_PER_1_MASK;
            mask_bit_start++) {
        if (test_bit(mask_bit_start,
                (const volatile void*)&mask_val)) {
            break;
        } else {
            num_of_sec++;
        }
    }

    return num_of_sec;
}

/*
 * head_sect_skip: counting how many continuous bit = 1 in a mask from msb
 * @mask_val: byte value to check
 *
 * Return: number of bit = 1
 */
int32_t tail_sect_skip (uint8_t mask_val)
{
    int32_t num_of_sec = 0;
    uint64_t mask_bit_start;

    for (mask_bit_start = BIT_LEN_PER_1_MASK - 1; mask_bit_start >= 0;
            mask_bit_start--) {
        if (test_bit(mask_bit_start,
                (const volatile void*)&mask_val)) {
            break;
        } else {
            num_of_sec++;
        }
    }

    return num_of_sec;
}

/*
 * do_divide: check divisor whether = 0 before perform divide
 * @dividend: dividend value
 * @divisor: divisor value
 *
 * Return:  0: when divided by 0
 *         ~0: divide result
 */
uint64_t do_divide (uint64_t dividend, uint64_t divisor)
{
    if (divisor <= 0) {
        return 0;
    }

    return dividend / divisor;
}

/*
 * test_and_set_request_state: test and set request state with atomic operation
 * @req_state: target request state
 * @bit:   which state bit
 * @value: target value
 *
 * Return:  state old value
 */
bool test_and_set_request_state (req_state_trace_t *req_state, uint32_t bit,
                              bool value)
{
    bool val;

    val = false;
    if (value) {
        val = test_and_set_bit(bit, (volatile void *)(&(req_state->state)));
    } else {
        val = test_and_clear_bit(bit, (volatile void *)(&(req_state->state)));
    }

    if (val) {
        val = true;
    }

    return val;
}

/*
 * test_req_state: check request state with atomic operation
 * @req_state: target request state
 * @bit:   which state bit
 *
 * Return:  state value
 * NOTE: Caller make sure req_state no NULL
 */
bool test_req_state (req_state_trace_t *req_state, uint32_t bit)
{
    bool val = false;

    val = test_bit(bit, (const volatile void *)(&(req_state->state)));

    if (val) {
        /*
         * Lego: Don't know why the value will be -1 if test value from
         * set_dnr_state, set_nnr_state, set_ior_state
         */
        val = true;
    }

    return val;
}

/*
 * set_request_state: set request state with atomic operation
 * @req_state: target request state
 * @bit:   which state bit
 * @value: which value to set
 *
 * NOTE: Caller make sure req_state no NULL
 */
void set_request_state (req_state_trace_t *req_state, uint32_t bit,
                              bool value)
{
    if (value) {
        set_bit(bit, (volatile void *)(&(req_state->state)));
    } else {
        clear_bit(bit, (volatile void *)(&(req_state->state)));
    }
}

/*
 * test_req_state_lock: check request state with atomic operation and lock protection
 * @req_state: target request state
 * @bit:   which state bit
 *
 * Return:  state value
 * NOTE: Caller make sure req_state no NULL
 */
bool test_req_state_lock (req_state_trace_t *req_state,
                                        uint32_t bit)
{
    bool val = false;

    spin_lock(&(req_state->state_lock));
    val = test_bit(bit, (const volatile void *)(&(req_state->state)));
    spin_unlock(&(req_state->state_lock));

    if (val) {
        /*
         * Lego: Don't know why the value will be -1 if test value from
         * set_dnr_state, set_nnr_state, set_ior_state
         */
        val = true;
    }

    return val;
}

/*
 * set_req_state_lock: set request state with atomic operation and lock protection
 * @req_state: target request state
 * @bit:   which state bit
 * @value: which value to set
 *
 * NOTE: Caller make sure req_state no NULL
 */
void set_req_state_lock (req_state_trace_t *req_state,
                                        uint32_t bit, bool value)
{
    spin_lock(&(req_state->state_lock));
    if (value) {
        set_bit(bit, (volatile void *)(&(req_state->state)));
    } else {
        clear_bit(bit, (volatile void *)(&(req_state->state)));
    }
    spin_unlock(&(req_state->state_lock));
}

/*
 * clear_request_state: clear request state with lock protection
 * @req_state: target request state
 *
 * NOTE: Caller make sure req_state no NULL
 */
void clear_request_state (req_state_trace_t *req_state)
{
    spin_lock(&(req_state->state_lock));
    req_state->state = 0;
    spin_unlock(&(req_state->state_lock));
}

/*
 * init_request_state: init request state
 * @req_state: target request state
 *
 * NOTE: Caller make sure req_state no NULL
 */
void init_request_state (req_state_trace_t *req_state)
{
    spin_lock_init(&req_state->state_lock);
    req_state->state = 0;
}

/*
 * compose_len_off: compose length and offset into 1 64-bit integer
 * @len: length in datanode physical location - rlo (rbid, length, offset)
 * @offset: offset in datanode physical location - rlo (rbid, length, offset)
 *
 * Return: compose results
 */
uint64_t compose_len_off (int32_t len, int32_t offset)
{
    uint64_t val = 0;

    val = ((uint64_t)len) << 40 | ((uint64_t)offset);
    return val;
}

/*
 * compose_triple: compose rbid, length and offset into 1 64-bit integer
 * @rbid: rbid in datanode physical location - rlo (rbid, length, offset)
 * @len: length in datanode physical location - rlo (rbid, length, offset)
 * @offset: offset in datanode physical location - rlo (rbid, length, offset)
 *
 * NOTE: offset: bit 0 ~ 19
 *       length: bit 20 ~ 31
 *       rbid:   bit 32 ~ 63
 * Return: compose results
 */
uint64_t compose_triple (int32_t rbid, int32_t len, int32_t offset)
{
    uint64_t val = 0;

    val = ((uint64_t)rbid) << 32 | ((uint64_t)len) << 20 | offset;
    return val;
}

/*
 * datanode path format: <Mount Point>/<Datanode ID>-<LUN>/
 *                       <(Local ID & 0xffc00)>>10>/<RBID>-<Local ID>.dat
 *   ex: /usr/cloudos/data/dms/dms-data/lun/carrier3/15-3/0/1009778692-4.dat
 *       <Mount Point> 			: /usr/cloudos/data/dms/dms-data/lun/carrier3
 *       <Datanode ID>-<LUN> 	: 15-3
 *       <(Local ID & 0xffc00)>>10> : 0
 *   	 <RBID>-<Local ID>.dat	: 1009778692-4.dat
 * RBID: DNID 6 bits / LUN 6 bits / LocalID 20 bits
 * RB's base unit: HBID 8 bytes + data-payload 4096 bytes
 */
void decompose_triple (uint64_t val, int32_t *rbid, int32_t *len,
        int32_t *offset)
{
    uint64_t tmp;
    tmp = val;
    *rbid = (int32_t) (val >> 32);
    tmp <<= 32;
    *len = (int32_t) (tmp >> 32) >> 20;
    *offset = (int32_t) (val & 0x00000000000fffffLL);
}

/*
 * ccma_inet_ntoa: translate a ipv4 string into a 4 bytes integer
 * @ipaddr: ipv4 address string
 *
 * Return: ipv4 address in integer representation
 */
uint32_t ccma_inet_ntoa (int8_t *ipaddr)
{
    int32_t a, b, c, d;
    int8_t addr[4];

    sscanf( ipaddr, "%d.%d.%d.%d", &a, &b, &c, &d );
    addr[0] = a;
    addr[1] = b;
    addr[2] = c;
    addr[3] = d;
    return *((uint32_t *)addr);
}

/*
 * ccma_inet_aton: translate a 4 bytes integer into ipv4 addree string
 * @ccma_inet_aton: target 4 bytes integer to be translated
 * @buf: string buffer to be return to caller
 * @len: buffer length
 *
 */
void ccma_inet_aton (uint32_t ipaddr, int8_t *buf, int32_t len)
{
    memset(buf, '\0', sizeof(int8_t)*len);
    //sprintf(buf, "%d.%d.%d.%d", ((ipaddr & 0xff000000)>> 24), ((ipaddr & 0x00ff0000) >> 16), ((ipaddr & 0x0000ff00) >> 8), (ipaddr & 0xff));

    sprintf(buf, "%d.%d.%d.%d", (ipaddr) & 0xFF,
             (ipaddr >> 8) & 0xFF,
             (ipaddr >> 16) & 0xFF,
             (ipaddr >> 24) & 0xFF);
    //dms_printk(LOG_LVL_DEBUG, "ccma_inet_aton: %s\n", buf);
}


#if 0
static void clear_cache (void)
{
    mm_segment_t oldfs;
    int8_t __user buf[1];
    int8_t fname[32];
    struct file *fp = NULL;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    memcpy(fname, "/proc/sys/vm/drop_caches\0", 25);
    fp = filp_open(fname, O_RDWR | O_CREAT, 0);
    fp->f_pos = 0;
    buf[0] = '3';
    //  fp->f_op->read(fp,buf,4, &fp->f_pos);
    fp->f_op->write(fp, buf, 1, &fp->f_pos);

    filp_close(fp, NULL);
    dms_printk(LOG_LVL_DEBUG, "CACHE: clear cache %d %c\n", buf[0], buf[0]);
    set_fs(oldfs);
}
#endif

#ifdef PAYLOAD_ENABLE
#define PAYLOAD_TABLE_SIZE  10001

/* TODO this lock is too big */
static rwlock_t payload_table_lock;
static struct hlist_head payload_table[PAYLOAD_TABLE_SIZE];
static spinlock_t payload_lru_lock;
static struct list_head payload_lru_list;

static struct kmem_cache *payload_node_pool = NULL;
#endif

#ifdef PAYLOAD_ENABLE
    rwlock_init( &payload_table_lock );
    spin_lock_init( &payload_lru_lock );
    INIT_LIST_HEAD( &payload_lru_list );

    for ( i = 0; i < PAYLOAD_TABLE_SIZE; ++i ) {
        INIT_HLIST_HEAD( &payload_table[i] );
    }

    payload_node_pool = kmem_cache_create_common( "DMS-client/payload_node",
                        sizeof( struct payload_node ),
                        0, 0, NULL, NULL );

    if ( !payload_node_pool ) {
        goto free_tree_leaf_pool;
    }

#endif


#ifdef PAYLOAD_ENABLE
free_tree_leaf_pool:
    kmem_cache_destroy( tree_leaf_pool );
    return -ENOMEM;
#endif

#ifdef PAYLOAD_ENABLE
    kmem_cache_destroy( payload_node_pool );
#endif
#ifdef PAYLOAD_ENABLE
    c_ops->Invalidate_Payload_Cache_Items   = NULL;
    c_ops->Invalidate_Payload_Cache_Item    = payload_invalidate;
    c_ops->Update_Payload_Cache_Item    = payload_add;
    c_ops->Get_Payload_Cache_Item       = payload_find;
    c_ops->Put_Payload_Cache_Item       = payload_put;
#endif
#ifdef PAYLOAD_ENABLE
static void __payload_touch( struct payload_node *p )
{
    spin_lock( &payload_lru_lock );
    list_del( &p->list );
    list_add( &p->list, &payload_lru_list );
    spin_unlock( &payload_lru_lock );
}

static void *__payload_find( unsigned long long pbID )
{
    struct hlist_head *head;
    struct hlist_node *node;
    struct payload_node *p = NULL;


    // TODO better hash function needed here.
    // TODO % doesn't work on 64-bit integer, change back!
    head = &payload_table[( ( unsigned long )pbID ) % PAYLOAD_TABLE_SIZE];

    read_lock( &payload_table_lock );
    hlist_for_each_entry( p, node, head, hlist ) {
        if ( p->pbID == pbID ) {
            goto unlock;
        }
    }
unlock:
    read_unlock( &payload_table_lock );

    if ( likely( p != NULL && p->pbID == pbID ) ) {
        __payload_touch( p );
        return p;
    }

    return NULL;
}

static int payload_find( unsigned long long vid,
                         unsigned long long start,
                         int count,
                         struct payload **pl_array[] )
{
    int i, off = 0;

    for ( i = 0; i < count; ++i ) {
        void *data = __payload_find( i + start );

        if ( !data ) {
            continue;
        }

        pl_array[off++] = data;
    }

    return off;
}

static void __payload_invalidate( unsigned long long pbID )
{
    struct hlist_head *head;
    struct hlist_node *node, *tmp;
    struct payload_node *p = NULL;

    // TODO better hash function needed here.
    // TODO % doesn't work on 64-bit integer, change back!
    head = &payload_table[( ( unsigned long )pbID ) % PAYLOAD_TABLE_SIZE];

    write_lock( &payload_table_lock );
    hlist_for_each_entry_safe( p, node, tmp, head, hlist ) {
        if ( p->pbID == pbID ) {
            hlist_del( node );
            goto unlock;
        }
    }
unlock:
    write_unlock( &payload_table_lock );

    if ( p != NULL && p->pbID == pbID ) {
        // TODO how to free p->data?
        kmem_cache_free( payload_node_pool, p );
    }
}

static int payload_invalidate( unsigned long long vid,
                               unsigned long long start,
                               int count )
{
    int i;

    for ( i = 0; i < count; ++i ) {
        __payload_invalidate( i + start );
    }

    return 0;
}

static int payload_add( unsigned long long vid,
                        unsigned long long pbID,
                        int nr,
                        struct payload **data )
{
    struct hlist_head *head;
    struct hlist_node *node;
    struct payload_node **p, *pp;
    int i, ret = -ENOMEM;

    p = kzalloc( nr * sizeof( struct payload_node * ), GFP_KERNEL );

    if ( unlikely( p == NULL ) ) {
        return -ENOMEM;
    }

    for ( i = 0; i < nr; ++i ) {
        p[i] = kmem_cache_alloc( payload_node_pool, GFP_KERNEL );

        if ( unlikely( p[i] == NULL ) ) {
            goto free_array;
        }

        INIT_HLIST_NODE( &p[i]->hlist );
        INIT_LIST_HEAD( &p[i]->list );
        p[i]->data = data[i];
    }

    write_lock( &payload_table_lock );

    for ( i = 0; i < nr; ++i ) {
        // TODO better hash function needed here.
        head = &payload_table[( pbID + i ) % PAYLOAD_TABLE_SIZE];
        hlist_for_each_entry( pp, node, head, hlist ) {
            if ( pp->pbID == ( pbID + i ) ) {
                pp->data = data[i];
                kmem_cache_free( payload_node_pool, p[i] );
                continue;
            }
        }
        hlist_add_head( &p[i]->hlist, head );
    }

    write_unlock( &payload_table_lock );
    ret = 0;
free_array:
    kfree( p );
    p = NULL;
    return ret;
}

static int payload_update( unsigned long long pbID, void *data )
{
    struct hlist_head *head;
    struct hlist_node *node;
    struct payload_node *p;
    int ret = 0;

    p = kmem_cache_alloc( payload_node_pool, GFP_KERNEL );

    if ( unlikely( p == NULL ) ) {
        return -ENOMEM;
    }

    // TODO better hash function needed here.
    head = &payload_table[pbID % PAYLOAD_TABLE_SIZE];

    write_lock( &payload_table_lock );
    hlist_for_each_entry( p, node, head, hlist ) {
        if ( p->pbID == pbID ) {
            // TODO how to free p->data?
            p->data = data;
            goto unlock;
        }
    }
    ret = -ENOENT;
unlock:
    write_unlock( &payload_table_lock );
    return ret;
}

int ( *Invalidate_Payload_Cache_Item_By_Type )( char type,
        unsigned long long volumeID,
        unsigned long long start_LBID,
        int nr_LBIDs,
        int( *check_condition )(
            struct payload *,
            void * )
                                              );

static int payload_put( unsigned long long volumeID,
                        unsigned long long start,
                        int count,
                        struct payload **pl_array[] )
{
    return 0;
}
#endif
