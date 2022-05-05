#include "libflat.h"

struct FLCONTROL FLCTRL_ARR[MAX_FLCTRL_RECURSION_DEPTH];
int FLCTRL_INDEX = -1;

static struct blstream* create_binary_stream_element(size_t size) {
	struct blstream* n = calloc(1,sizeof(struct blstream));
	assert(n!=0);
	void* m = calloc(1,size);
	assert(m!=0);
	n->data = m;
	n->size = size;
	return n;
}

struct blstream* binary_stream_append(const void* data, size_t size) {
	struct blstream* v = create_binary_stream_element(size);
	assert(v!=0);
	memcpy(v->data,data,size);
    if (!FLCTRL.bhead) {
        FLCTRL.bhead = v;
        FLCTRL.btail = v;
    }
    else {
        v->prev = FLCTRL.btail;
        FLCTRL.btail->next = v;
        FLCTRL.btail = FLCTRL.btail->next;
    }
    return v;
}


struct blstream* binary_stream_append_reserve(size_t size) {
	struct blstream* v = calloc(1,sizeof(struct blstream));
	assert(v!=0);
	v->data = 0;
	v->size = size;
	if (!FLCTRL.bhead) {
        FLCTRL.bhead = v;
        FLCTRL.btail = v;
    }
    else {
        v->prev = FLCTRL.btail;
        FLCTRL.btail->next = v;
        FLCTRL.btail = FLCTRL.btail->next;
    }
    return v;
}

struct blstream* binary_stream_update(const void* data, size_t size, struct blstream* where) {
	void* m = calloc(1,size);
	assert(m!=0);
	where->data = m;
	memcpy(where->data,data,size);
	return where;
}


struct blstream* binary_stream_insert_front(const void* data, size_t size, struct blstream* where) {
	struct blstream* v = create_binary_stream_element(size);
	assert(v!=0);
	memcpy(v->data,data,size);
	v->next = where;
	v->prev = where->prev;
	if (where->prev) {
		where->prev->next = v;
	}
	else {
		FLCTRL.bhead = v;
	}
	where->prev = v;
	return v;
}

struct blstream* binary_stream_insert_front_reserve(size_t size, struct blstream* where) {
	struct blstream* v = calloc(1,sizeof(struct blstream));
	assert(v!=0);
	v->data = 0;
	v->size = size;
	v->next = where;
	v->prev = where->prev;
	if (where->prev) {
		where->prev->next = v;
	}
	else {
		FLCTRL.bhead = v;
	}
	where->prev = v;
	return v;
}

struct blstream* binary_stream_insert_back(const void* data, size_t size, struct blstream* where) {
	struct blstream* v = create_binary_stream_element(size);
	assert(v!=0);
	memcpy(v->data,data,size);
	v->next = where->next;
	v->prev = where;
	if (where->next) {
		where->next->prev = v;
	}
	else {
		FLCTRL.btail = v;
	}
	where->next = v;
	return v;
}

struct blstream* binary_stream_insert_back_reserve(size_t size, struct blstream* where) {
	struct blstream* v = calloc(1,sizeof(struct blstream));
	assert(v!=0);
	v->data = 0;
	v->size = size;
	v->next = where->next;
	v->prev = where;
	if (where->next) {
		where->next->prev = v;
	}
	else {
		FLCTRL.btail = v;
	}
	where->next = v;
	return v;
}

void binary_stream_calculate_index() {
	struct blstream* p = FLCTRL.bhead;
	size_t index=0;
    while(p) {
    	struct blstream* cp = p;
    	p = p->next;
    	size_t align=0;

    	if (cp->alignment) {
    		unsigned char* padding = malloc(cp->alignment);
    		memset(padding,0,cp->alignment);
    		if (index==0) {
    			align=cp->alignment;
    		}
    		else if (index%cp->alignment) {
    			align=cp->alignment-(index%cp->alignment);
    		}
    		struct blstream* v = binary_stream_insert_front(padding,align,cp);
    		v->index = index;
    		index+=v->size;
    		free(padding);
    	}

    	cp->index = index;
    	index+=cp->size;
    }
}

void binary_stream_destroy() {
	FLCTRL.btail = FLCTRL.bhead;
    while(FLCTRL.btail) {
    	struct blstream* p = FLCTRL.btail;
    	FLCTRL.btail = FLCTRL.btail->next;
    	free(p->data);
    	free(p);
    }
}

static void binary_stream_element_print(struct blstream* p) {
	size_t i;
	printf("(%zu)(%zu){%zu}{%p}[ ",p->index,p->alignment,p->size,(void*)p);
	for (i=0; i<p->size; ++i) {
		printf("%02x ",((unsigned char*)(p->data))[i]);
	}
	printf("]\n");
}

static size_t binary_stream_element_write(struct blstream* p, FILE* f) {
	return fwrite((unsigned char*)(p->data),1,p->size,f);
}

void binary_stream_print() {

	struct blstream* cp = FLCTRL.bhead;
	size_t total_size = 0;
    while(cp) {
    	struct blstream* p = cp;
    	cp = cp->next;
    	binary_stream_element_print(p);
    	total_size+=p->size;
    }
    printf("@ Total size: %zu\n",total_size);
}

size_t binary_stream_write(FILE* f) {
	struct blstream* cp = FLCTRL.bhead;
	size_t written=0;
    while(cp) {
    	struct blstream* p = cp;
    	cp = cp->next;
    	written+=binary_stream_element_write(p,f);
    }
    return written;
}

size_t binary_stream_size() {
	
	struct blstream* cp = FLCTRL.bhead;
	size_t total_size = 0;
    while(cp) {
    	struct blstream* p = cp;
    	cp = cp->next;
    	total_size+=p->size;
    }
    return total_size;
}

void binary_stream_update_pointers() {
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	int count=0;
	while(p) {
	    struct fixup_set_node* node = (struct fixup_set_node*)p;
	    if ((node->ptr)&&(!(((unsigned long)node->ptr)&1))) {
			void* newptr = (unsigned char*)node->ptr->node->storage->index+node->ptr->offset;
			if (FLCTRL.debug_flag&1) printf("@ ptr update at ((%p)%p:%zu) : %p => %p\n",node->inode,(void*)node->inode->start,node->offset,
					newptr,(void*)(((unsigned char*)node->inode->storage->data)+node->offset));
			memcpy(&((unsigned char*)node->inode->storage->data)[node->offset],&newptr,sizeof(void*));
	    }
	    p = rb_next(p);
	    count++;
    }
}

void bqueue_init(struct bqueue* q, size_t block_size) {

    q->block_size = block_size;
    q->size = 0;
    q->front_block = malloc(block_size+sizeof(struct queue_block*));
    q->front_block->next = 0;
    q->back_block = q->front_block;
    q->front_index=0;
    q->back_index=0;
}

void bqueue_destroy(struct bqueue* q) {

    struct queue_block* back = q->back_block;
    while(back) {
        struct queue_block* tmp = back;
        back = back->next;
        free(tmp);
    }
}

int bqueue_empty(struct bqueue* q) {

    return q->size == 0;
}

void bqueue_push_back(struct bqueue* q, const void* m, size_t s) {

    size_t copied = 0;
    while(s>0) {
        size_t avail_size = q->block_size-q->front_index;
        size_t copy_size = (s>avail_size)?(avail_size):(s);
        memcpy(q->front_block->data+q->front_index,m+copied,copy_size);
        copied+=copy_size;
        if (s>=avail_size) {
            s=s-avail_size;
            struct queue_block* new_block = malloc(q->block_size+sizeof(struct queue_block*));
            new_block->next = 0;
            q->front_block->next = new_block;
            q->front_block = new_block;
        }
        else s=0;
        q->front_index = (q->front_index+copy_size)%q->block_size;
    }
    q->size+=copied;
}

void bqueue_pop_front(struct bqueue* q, void* m, size_t s) {

    assert(q->size>=s && "bqueue underflow");
    size_t copied = 0;

    while(s>0) {
        size_t avail_size = q->block_size-q->back_index;
        size_t copy_size = (s>avail_size)?(avail_size):(s);
        memcpy(m+copied,q->back_block->data+q->back_index,copy_size);
        copied+=copy_size;
        if (s>=avail_size) {
            s=s-avail_size;
            struct queue_block* tmp = q->back_block;
            q->back_block = q->back_block->next;
            free(tmp);
        }
        else s=0;
        q->back_index = (q->back_index+copy_size)%q->block_size;
    }
    q->size-=copied;
}

#define ADDR_KEY(p)	((((p)->inode)?((p)->inode->start):0) + (p)->offset)

static struct fixup_set_node* create_fixup_set_node_element(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr) {
	struct fixup_set_node* n = calloc(1,sizeof(struct fixup_set_node));
	assert(n!=0);
	n->inode = node;
	n->offset = offset;
	n->ptr = ptr;
	return n;
}

struct fixup_set_node *fixup_set_search(uintptr_t v) {
	struct rb_node *node = FLCTRL.fixup_set_root.rb_node;

	while (node) {
		struct fixup_set_node *data = container_of(node, struct fixup_set_node, node);

		if (v < ADDR_KEY(data)) {
			node = node->rb_left;
		}
		else if (v > ADDR_KEY(data)) {
			node = node->rb_right;
		}
		else {
			LIBFLAT_DBGM("fixup_set_search(%lx): (%p:%zu,%p)\n",v,data->inode,data->offset,data->ptr);
			return data;
		}
	}

	LIBFLAT_DBGM("fixup_set_search(%lx): 0\n",v);
	return 0;
}

int fixup_set_reserve_address(uintptr_t addr) {

	LIBFLAT_DBGM("fixup_set_reserve_address(%lx)\n",addr);

	struct fixup_set_node* inode = fixup_set_search(addr);

	if (inode) {
		return 0;
	}

	struct fixup_set_node *data = create_fixup_set_node_element(0,addr,0);
	struct rb_node **new = &(FLCTRL.fixup_set_root.rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct fixup_set_node *this = container_of(*new, struct fixup_set_node, node);

		parent = *new;
		if (data->offset < ADDR_KEY(this))
			new = &((*new)->rb_left);
		else if (data->offset > ADDR_KEY(this))
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &FLCTRL.fixup_set_root);

	return 1;
}

int fixup_set_reserve(struct interval_tree_node* node, size_t offset) {

	LIBFLAT_DBGM("fixup_set_reserve(%lx,%zu)\n",(uintptr_t)node,offset);

	if (node==0) {
		return 0;
	}

	struct fixup_set_node* inode = fixup_set_search(node->start+offset);

	if (inode) {
		return 0;
	}

	struct fixup_set_node *data = create_fixup_set_node_element(node,offset,0);
	struct rb_node **new = &(FLCTRL.fixup_set_root.rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct fixup_set_node *this = container_of(*new, struct fixup_set_node, node);

		parent = *new;
		if (ADDR_KEY(data) < ADDR_KEY(this))
			new = &((*new)->rb_left);
		else if (ADDR_KEY(data) > ADDR_KEY(this))
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &FLCTRL.fixup_set_root);

	return 1;
}

int fixup_set_update(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr) {

	LIBFLAT_DBGM("fixup_set_update(%lx[%lx:%zu],%zu,%lx)\n",(uintptr_t)node,
			(node)?(node->start):0,(node)?(node->last-node->start+1):0,
			offset,(uintptr_t)ptr);

	if (node==0) {
		free(ptr);
		return 0;
	}

	struct fixup_set_node* inode = fixup_set_search(node->start+offset);

	if (!inode) {
		return 0;
	}

	if (!inode->inode) {
		assert((node->start + offset)==inode->offset && "fixup_set_update: node address not matching reserved offset");
		rb_erase(&inode->node, &FLCTRL.fixup_set_root);
		return fixup_set_insert(node,offset,ptr);
	}

	inode->ptr = ptr;
	return 1;
}

int fixup_set_insert(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr) {

	LIBFLAT_DBGM("fixup_set_insert(%lx[%lx:%zu],%zu,%lx)\n",(uintptr_t)node,
			(node)?(node->start):0,(node)?(node->last-node->start+1):0,
			offset,(uintptr_t)ptr);

	if (node==0) {
		free(ptr);
		return 0;
	}

	struct fixup_set_node* inode = fixup_set_search(node->start+offset);

	if (inode && inode->inode) {
		assert((inode->ptr->node->start+inode->ptr->offset)==(ptr->node->start+ptr->offset)
				&& "fixup_set_insert: Multiple pointer mismatch for the same storage");
		free(ptr);
		return 0;
	}

	if (inode) {
		return fixup_set_update(node, offset, ptr);
	}

	struct fixup_set_node *data = create_fixup_set_node_element(node,offset,ptr);
	struct rb_node **new = &(FLCTRL.fixup_set_root.rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct fixup_set_node *this = container_of(*new, struct fixup_set_node, node);

		parent = *new;
		if (ADDR_KEY(data) < ADDR_KEY(this))
			new = &((*new)->rb_left);
		else if (ADDR_KEY(data) > ADDR_KEY(this))
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &FLCTRL.fixup_set_root);

	return 1;
}

int fixup_set_insert_force_update(struct interval_tree_node* node, size_t offset, struct flatten_pointer* ptr) {

	LIBFLAT_DBGM("fixup_set_insert(%lx[%lx:%zu],%zu,%lx)\n",(uintptr_t)node,
			(node)?(node->start):0,(node)?(node->last-node->start+1):0,
			offset,(uintptr_t)ptr);

	if (node==0) {
		free(ptr);
		return 0;
	}

	struct fixup_set_node* inode = fixup_set_search(node->start+offset);

	if (inode && inode->inode) {
		uintptr_t inode_ptr;
		if (((unsigned long)inode->ptr)&1) {
			inode_ptr = ((unsigned long)inode->ptr)&(~1);
		}
		else {
			inode_ptr = inode->ptr->node->start + inode->ptr->offset;
		}
		if (inode_ptr!=ptr->node->start+ptr->offset) {
			return EINVAL;
		}
		else {
			free(ptr);
			return EEXIST;
		}
	}

	if (inode && !inode->inode) {
		return fixup_set_update(node, offset, ptr);
	}

	struct fixup_set_node *data = create_fixup_set_node_element(node,offset,ptr);
	struct rb_node **new = &(FLCTRL.fixup_set_root.rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct fixup_set_node *this = container_of(*new, struct fixup_set_node, node);

		parent = *new;
		if (ADDR_KEY(data) < ADDR_KEY(this))
			new = &((*new)->rb_left);
		else if (ADDR_KEY(data) > ADDR_KEY(this))
			new = &((*new)->rb_right);
		else {
			free((void*)(((uintptr_t)data->ptr)&(~1)));
			data->ptr = ptr;
			return EAGAIN;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &FLCTRL.fixup_set_root);

	return 1;
}

int fixup_set_insert_fptr(struct interval_tree_node* node, size_t offset, unsigned long fptr) {

	LIBFLAT_DBGM("fixup_set_insert_fptr(%lx[%lx:%zu],%zu,%lx)\n",(uintptr_t)node,
			(node)?(node->start):0,(node)?(node->last-node->start+1):0,
			offset,fptr);

	if (node==0) {
		return 0;
	}

	struct fixup_set_node* inode = fixup_set_search(node->start+offset);

	if (inode && inode->inode) {
		assert(((((unsigned long)inode->ptr)&(~1))==fptr) && "Multiple pointer mismatch for the same storage");
		return 0;
	}

	if (inode) {
		return fixup_set_update(node, offset, (struct flatten_pointer*)(fptr|1));
	}

	struct fixup_set_node *data = create_fixup_set_node_element(node,offset,(struct flatten_pointer*)(fptr|1));
	struct rb_node **new = &(FLCTRL.fixup_set_root.rb_node), *parent = 0;

	/* Figure out where to put new node */
	while (*new) {
		struct fixup_set_node *this = container_of(*new, struct fixup_set_node, node);

		parent = *new;
		if (ADDR_KEY(data) < ADDR_KEY(this))
			new = &((*new)->rb_left);
		else if (ADDR_KEY(data) > ADDR_KEY(this))
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &FLCTRL.fixup_set_root);

	return 1;
}

void fixup_set_print() {
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	printf("[\n");
	while(p) {
    	struct fixup_set_node* node = (struct fixup_set_node*)p;
    	if (node->ptr) {
    		if (((unsigned long)node->ptr)&1) {
    			uintptr_t newptr = ((unsigned long)node->ptr)&(~1);
    			uintptr_t origptr = node->inode->storage->index+node->offset;
    			printf(" %zu: (%p:%zu)->(F) | %zu <- %zu\n",
					node->inode->storage->index,
					node->inode,node->offset,
					origptr,newptr);
    		}
    		else {
				uintptr_t newptr = node->ptr->node->storage->index+node->ptr->offset;
				uintptr_t origptr = node->inode->storage->index+node->offset;
				printf(" %zu: (%p:%zu)->(%p:%zu) | %zu <- %zu\n",
						node->inode->storage->index,
						node->inode,node->offset,
						node->ptr->node,node->ptr->offset,
						origptr,newptr);
    		}
    	}
    	else if (node->inode) {
    		/* Reserved node but never filled */
			uintptr_t origptr = node->inode->storage->index+node->offset;
			printf(" %zu: (%p:%zu)-> 0 | %zu\n",
					node->inode->storage->index,
					node->inode,node->offset,
					origptr);
    	}
    	else {
    		/* Reserved for dummy pointer */
			printf(" (%p)-> 0 | \n",(void*)node->offset);
    	}

    	p = rb_next(p);
    }
    printf("]\n");
}

size_t fixup_set_write(FILE* f) {
	size_t written=0;
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	while(p) {
    	struct fixup_set_node* node = (struct fixup_set_node*)p;
    	if ((node->ptr)&&(!(((unsigned long)node->ptr)&1))) {
			size_t origptr = node->inode->storage->index+node->offset;
			size_t wr = fwrite(&origptr,sizeof(size_t),1,f);
			written+=wr*sizeof(size_t);
    	}
    	p = rb_next(p);
    }
    return written;
}

size_t fixup_set_fptr_write(FILE* f) {
	size_t written=0;
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	while(p) {
    	struct fixup_set_node* node = (struct fixup_set_node*)p;
    	if ((((unsigned long)node->ptr)&1)) {
			size_t origptr = node->inode->storage->index+node->offset;
			size_t wr = fwrite(&origptr,sizeof(size_t),1,f);
			written+=wr*sizeof(size_t);
    	}
    	p = rb_next(p);
    }
    return written;
}

size_t fixup_set_count() {
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	size_t count=0;
	while(p) {
		struct fixup_set_node* node = (struct fixup_set_node*)p;
		if ((node->ptr)&&(!(((unsigned long)node->ptr)&1))) {
			count++;
		}
    	p = rb_next(p);
    }
    return count;
}

size_t fixup_set_fptr_count() {
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	size_t count=0;
	while(p) {
		struct fixup_set_node* node = (struct fixup_set_node*)p;
		if ((((unsigned long)node->ptr)&1)) {
			count++;
		}
    	p = rb_next(p);
    }
    return count;
}

void fixup_set_destroy() {
	struct rb_node * p = rb_first(&FLCTRL.fixup_set_root);
	while(p) {
		struct fixup_set_node* node = (struct fixup_set_node*)p;
		rb_erase(p, &FLCTRL.fixup_set_root);
		p = rb_next(p);
		if (!(((unsigned long)node->ptr)&1)) {
			free(node->ptr);
		}
		free(node);
	};
}

void root_addr_append(uintptr_t root_addr) {
    struct root_addrnode* v = calloc(1,sizeof(struct root_addrnode));
    assert(v!=0);
    v->root_addr = root_addr;
    if (!FLCTRL.rhead) {
        FLCTRL.rhead = v;
        FLCTRL.rtail = v;
    }
    else {
        FLCTRL.rtail->next = v;
        FLCTRL.rtail = FLCTRL.rtail->next;
    }
}

size_t root_addr_count() {
	struct root_addrnode* p = FLCTRL.rhead;
	size_t count = 0;
    while(p) {
    	count++;
    	p = p->next;
    }
    return count;
}

void interval_tree_print(struct rb_root *root) {
	struct rb_node * p = rb_first(root);
	size_t total_size=0;
	while(p) {
		struct interval_tree_node* node = (struct interval_tree_node*)p;
		printf("(%p)[%p:%p](%zu){%p}\n",node,(void*)node->start,(void*)node->last,node->last-node->start+1,(void*)node->storage);
		total_size+=node->last-node->start+1;
		p = rb_next(p);
	};
	printf("@ Total size: %zu\n",total_size);
}

void interval_tree_destroy(struct rb_root *root) {
	struct interval_nodelist *h = 0, *i = 0;
	struct rb_node * p = rb_first(root);
	while(p) {
		struct interval_tree_node* node = (struct interval_tree_node*)p;
		interval_tree_remove(node,&FLCTRL.imap_root);
		struct interval_nodelist* v = calloc(1,sizeof(struct interval_nodelist));
		assert(v!=0);
	    v->node = node;
	    if (!h) {
	        h = v;
	        i = v;
	    }
	    else {
	        i->next = v;
	        i = i->next;
	    }
		p = rb_next(p);
	};
	while(h) {
    	struct interval_nodelist* p = h;
    	h = h->next;
    	free(p->node);
    	free(p);
    }
}

struct flatten_pointer* get_pointer_node(const void* _ptr) {

	assert(_ptr!=0);
	struct interval_tree_node *node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)_ptr, (uintptr_t)_ptr+sizeof(void*)-1);
	struct flatten_pointer* r = 0;
	if (node) {
		uintptr_t p = (uintptr_t)_ptr;
		struct interval_tree_node *prev;
		while(node) {
			if (node->start>p) {
				assert(node->storage!=0);
				struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));
				assert(nn!=0);
				nn->start = p;
				nn->last = node->start-1;
				nn->storage = binary_stream_insert_front((void*)p,node->start-p,node->storage);
				interval_tree_insert(nn, &FLCTRL.imap_root);
				if (r==0) {
					r = make_flatten_pointer(nn,0);
				}
			}
			else {
				if (r==0) {
					r = make_flatten_pointer(node,p-node->start);
				}
			}
			p = node->last+1;
			prev = node;
			node = interval_tree_iter_next(node, (uintptr_t)_ptr, (uintptr_t)_ptr+sizeof(void*)-1);
		}
		if ((uintptr_t)_ptr+sizeof(void*)>p) {
			assert(prev->storage!=0);
			struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));
			assert(nn!=0);
			nn->start = p;
			nn->last = (uintptr_t)_ptr+sizeof(void*)-1;
			nn->storage = binary_stream_insert_back((void*)p,(uintptr_t)_ptr+sizeof(void*)-p,prev->storage);
			interval_tree_insert(nn, &FLCTRL.imap_root);
		}
		return r;
	}
	else {
		node = calloc(1,sizeof(struct interval_tree_node));
		assert(node!=0);
		node->start = (uintptr_t)_ptr;
		node->last = (uintptr_t)_ptr + sizeof(void*)-1;
		struct rb_node* rb = interval_tree_insert(node, &FLCTRL.imap_root);
		struct rb_node* prev = rb_prev(rb);
		struct blstream* storage;
		if (prev) {
			storage = binary_stream_insert_back(_ptr,sizeof(void*),((struct interval_tree_node*)prev)->storage);
		}
		else {
			struct rb_node* next = rb_next(rb);
			if (next) {
				storage = binary_stream_insert_front(_ptr,sizeof(void*),((struct interval_tree_node*)next)->storage);
			}
			else {
				storage = binary_stream_append(_ptr,sizeof(void*));
			}
		}
		node->storage = storage;		
		return make_flatten_pointer(node,0);
	}
}

struct flatten_pointer* flatten_plain_type(const void* _ptr, size_t _sz) {

	assert(_sz>0);
	struct interval_tree_node *node = interval_tree_iter_first(&FLCTRL.imap_root, (uintptr_t)_ptr, (uintptr_t)_ptr+_sz-1);
	struct flatten_pointer* r = 0;
	if (node) {
		uintptr_t p = (uintptr_t)_ptr;
		struct interval_tree_node *prev;
		while(node) {
			if (node->start>p) {
				assert(node->storage!=0);
				struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));
				assert(nn!=0);
				nn->start = p;
				nn->last = node->start-1;
				nn->storage = binary_stream_insert_front((void*)p,node->start-p,node->storage);
				interval_tree_insert(nn, &FLCTRL.imap_root);
				if (r==0) {
					r = make_flatten_pointer(nn,0);
				}
			}
			else {
				if (r==0) {
					r = make_flatten_pointer(node,p-node->start);
				}
			}
			p = node->last+1;
			prev = node;
			node = interval_tree_iter_next(node, (uintptr_t)_ptr, (uintptr_t)_ptr+_sz-1);
		}
		if ((uintptr_t)_ptr+_sz>p) {
			assert(prev->storage!=0);
			struct interval_tree_node* nn = calloc(1,sizeof(struct interval_tree_node));
			assert(nn!=0);
			nn->start = p;
			nn->last = (uintptr_t)_ptr+_sz-1;
			nn->storage = binary_stream_insert_back((void*)p,(uintptr_t)_ptr+_sz-p,prev->storage);
			interval_tree_insert(nn, &FLCTRL.imap_root);
		}
		return r;
	}
	else {
		node = calloc(1,sizeof(struct interval_tree_node));
		assert(node!=0);
		node->start = (uintptr_t)_ptr;
		node->last = (uintptr_t)_ptr + _sz-1;
		struct blstream* storage;
		struct rb_node* rb = interval_tree_insert(node, &FLCTRL.imap_root);
		struct rb_node* prev = rb_prev(rb);
		if (prev) {
			storage = binary_stream_insert_back(_ptr,_sz,((struct interval_tree_node*)prev)->storage);
		}
		else {
			struct rb_node* next = rb_next(rb);
			if (next) {
				storage = binary_stream_insert_front(_ptr,_sz,((struct interval_tree_node*)next)->storage);
			}
			else {
				storage = binary_stream_append(_ptr,_sz);
			}
		}
		node->storage = storage;		
		return make_flatten_pointer(node,0);
	}
}

void fix_unflatten_memory(struct flatten_header* hdr, void* memory) {
	size_t i;
	void* mem = (unsigned char*)memory+hdr->ptr_count*sizeof(size_t);
	for (i=0; i<hdr->ptr_count; ++i) {
		size_t fix_loc = *((size_t*)memory+i);
		uintptr_t ptr = (uintptr_t)( *((void**)((unsigned char*)mem+fix_loc)) );
		/* Make the fix */
		*((void**)((unsigned char*)mem+fix_loc)) = (unsigned char*)mem - hdr->fix_base + ptr;
	}
}

void flatten_init() {
    ++FLCTRL_INDEX;
    FLCTRL.bhead = 0;
    FLCTRL.btail = 0;
    FLCTRL.fixup_set_root.rb_node = 0;
    FLCTRL.imap_root.rb_node = 0;
    FLCTRL.rhead = 0;
    FLCTRL.rtail = 0;
    FLCTRL.mem = 0;
    FLCTRL.last_accessed_root=0;
    FLCTRL.debug_flag=0;
    FLCTRL.option=0;
    FLCTRL.map_size = 0;
}

int flatten_write(FILE* ff) {
	size_t written = 0;
	binary_stream_calculate_index();
    binary_stream_update_pointers();
    if (FLCTRL.debug_flag>=2) {
    	flatten_debug_info();
    }
    FLCTRL.HDR.memory_size = binary_stream_size();
    FLCTRL.HDR.ptr_count = fixup_set_count();
    FLCTRL.HDR.fptr_count = fixup_set_fptr_count();
    FLCTRL.HDR.root_addr_count = root_addr_count();
    FLCTRL.HDR.fix_base = 0;
    FLCTRL.HDR.magic = FLATTEN_MAGIC;
    size_t wr = fwrite(&FLCTRL.HDR,sizeof(struct flatten_header),1,ff);
    if (wr!=1)
    	return -1;
    written+=sizeof(struct flatten_header);
    struct root_addrnode* p = FLCTRL.rhead;
    while(p) {
        size_t root_addr_offset;
        if (p->root_addr) {
            struct interval_tree_node *node = PTRNODE(p->root_addr);
            LIBFLAT_DBGM("@ root_addr: %p node: %p\n",(void*)p->root_addr,node);
            assert(node!=0);
            root_addr_offset = node->storage->index + (p->root_addr-node->start);
    	}
    	else {
    		root_addr_offset = (size_t)-1;
    	}
    	size_t wr = fwrite(&root_addr_offset,sizeof(size_t),1,ff);
    	if (wr!=1) return -1; else written+=wr*sizeof(size_t);
    	p = p->next;
    }
    wr = fixup_set_write(ff);
    if (wr!=FLCTRL.HDR.ptr_count*sizeof(size_t)) return -1; else written+=wr;
    wr = fixup_set_fptr_write(ff);
    if (wr!=FLCTRL.HDR.fptr_count*sizeof(size_t)) return -1; else written+=wr;
    wr = binary_stream_write(ff);
    if (wr!=FLCTRL.HDR.memory_size) return -1; else written+=wr;
    if ((FLCTRL.option&option_silent)==0) {
		printf("# Flattening done. Summary:\n  Memory size: %zu bytes\n  Linked %zu pointers\n  Written %zu bytes\n",
				FLCTRL.HDR.memory_size, FLCTRL.HDR.ptr_count, written);
    }
    return 0;
}

void flatten_debug_info() {
	binary_stream_print();
    interval_tree_print(&FLCTRL.imap_root);
    fixup_set_print();
}

void flatten_fini() {
	binary_stream_destroy();
    interval_tree_destroy(&FLCTRL.imap_root);
    fixup_set_destroy();
    FLCTRL.rtail = FLCTRL.rhead;
    while(FLCTRL.rtail) {
    	struct root_addrnode* p = FLCTRL.rtail;
    	FLCTRL.rtail = FLCTRL.rtail->next;
    	free(p);
    }
    --FLCTRL_INDEX;
}    

void unflatten_init() {
	++FLCTRL_INDEX;
    FLCTRL.bhead = 0;
    FLCTRL.btail = 0;
    FLCTRL.fixup_set_root.rb_node = 0;
    FLCTRL.imap_root.rb_node = 0;
    FLCTRL.rhead = 0;
    FLCTRL.rtail = 0;
    FLCTRL.mem = 0;
    FLCTRL.last_accessed_root=0;
    FLCTRL.debug_flag=0;
    FLCTRL.option=0;
    FLCTRL.map_size = 0;
}

#include <sys/mman.h>
#include <sys/stat.h>

int unflatten_map_rdonly(int fd, off_t offset) {

	TIME_MARK_START(unfl_b);

	struct stat st;
	fstat(fd, &st);
	const char *mem;
	mem = mmap(0, offset+sizeof(struct flatten_header), PROT_READ, MAP_SHARED, fd, 0);
	assert(mem!=MAP_FAILED);
	memcpy(&FLCTRL.HDR,mem+offset,sizeof(struct flatten_header));
	munmap((void*)mem,offset+sizeof(struct flatten_header));
	size_t memsz = offset+sizeof(struct flatten_header) + sizeof(size_t)*FLCTRL.HDR.root_addr_count +
			FLCTRL.HDR.memory_size+FLCTRL.HDR.ptr_count*sizeof(size_t);
	assert(st.st_size>=(long)memsz);
	size_t hdr_offset = sizeof(struct flatten_header) + sizeof(size_t)*FLCTRL.HDR.root_addr_count;
	size_t mem_offset = hdr_offset + FLCTRL.HDR.ptr_count*sizeof(size_t);
	if (FLCTRL.HDR.fix_base) {
		/* Try to map memory at the same address as it was mapped last time */
		void* reqaddr = (void*)(FLCTRL.HDR.fix_base - mem_offset - offset);
		mem = mmap(reqaddr, memsz, PROT_READ, MAP_SHARED, fd, 0);
		if ((mem!=MAP_FAILED)&&(mem!=reqaddr)) {
			/* mmap succeeded but the address is different */
			munmap((void*)mem,memsz);
			return -2;
		}
		else if (mem!=MAP_FAILED) {
			/* mem is mapped at requested address. Complete mapping. */
			FLCTRL.map_size = memsz;
			FLCTRL.map_mem = (void*)mem;
			mem+=offset+sizeof(struct flatten_header);
			if (FLCTRL.HDR.magic!=FLATTEN_MAGIC) {
				fprintf(stderr,"Invalid magic while reading flattened image\n");
				return -1;
			}
			size_t i;
			for (i=0; i<FLCTRL.HDR.root_addr_count; ++i) {
				size_t root_addr_offset;
				memcpy(&root_addr_offset,mem,sizeof(size_t));
				mem+=sizeof(size_t);
				root_addr_append(root_addr_offset);
			}
			FLCTRL.mem = (void*)mem;
			if ((FLCTRL.option&option_silent)==0) {
				printf("# Unflattening done. Summary:\n");
				TIME_CHECK_FMT(unfl_b,map_e,"  Image mapping time: %fs\n");
			}
			TIME_MARK_START(fix_b);
			if ((FLCTRL.option&option_silent)==0) {
				TIME_CHECK_FMT(unfl_b,fix_e,"  Total time: %fs\n");
				printf("  Total bytes mapped: %zu\n",memsz);
			}

			return 0;
		}
		else {
			/* mmap failed */
			return -3;
		}
	}

	return -1;
}

int unflatten_map(int fd, off_t offset) {
	return unflatten_map_internal(fd,offset,MAP_SHARED);
}
int unflatten_map_private(int fd, off_t offset) {
	return unflatten_map_internal(fd,offset,MAP_PRIVATE);
}

int unflatten_map_internal(int fd, off_t offset, int map_flags) {

	TIME_MARK_START(unfl_b);

	struct stat st;
	fstat(fd, &st);
	const char *mem;
	mem = mmap(0, offset+sizeof(struct flatten_header), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	assert(mem!=MAP_FAILED);
	memcpy(&FLCTRL.HDR,mem+offset,sizeof(struct flatten_header));
	munmap((void*)mem,offset+sizeof(struct flatten_header));
	size_t memsz = offset+sizeof(struct flatten_header) + sizeof(size_t)*FLCTRL.HDR.root_addr_count +
			FLCTRL.HDR.memory_size+FLCTRL.HDR.ptr_count*sizeof(size_t);
	assert(st.st_size>=(long)memsz);
	size_t hdr_offset = sizeof(struct flatten_header) + sizeof(size_t)*FLCTRL.HDR.root_addr_count;
	size_t mem_offset = hdr_offset + FLCTRL.HDR.ptr_count*sizeof(size_t);
	void* reqaddr = 0;
	if (FLCTRL.HDR.fix_base) {
		/* Try to map memory at the same address as it was mapped last time */
		reqaddr = (void*)(FLCTRL.HDR.fix_base - mem_offset - offset);
		mem = mmap(reqaddr, memsz, PROT_READ, MAP_SHARED, fd, 0);
		if ((mem!=MAP_FAILED)&&(mem!=reqaddr)) {
			/* mmap succeeded but the address is different */
			munmap((void*)mem,memsz);
			reqaddr = 0;
		}
		else if (mem!=MAP_FAILED) {
			/* mem is mapped at requested address. Nothing to do anymore. */
		}
		else {
			/* mmap failed */
			reqaddr = 0;
		}
	}
	if (reqaddr==0) {
		mem = mmap(0, memsz, PROT_READ|PROT_WRITE, map_flags, fd, 0);
		assert(mem!=MAP_FAILED);
	}
	FLCTRL.map_size = memsz;
	FLCTRL.map_mem = (void*)mem;
	mem+=offset+sizeof(struct flatten_header);
	if (FLCTRL.HDR.magic!=FLATTEN_MAGIC) {
		fprintf(stderr,"Invalid magic while reading flattened image\n");
		return -1;
	}
	size_t i;
	for (i=0; i<FLCTRL.HDR.root_addr_count; ++i) {
		size_t root_addr_offset;
		memcpy(&root_addr_offset,mem,sizeof(size_t));
		mem+=sizeof(size_t);
		root_addr_append(root_addr_offset);
	}
	FLCTRL.mem = (void*)mem;
	if ((FLCTRL.option&option_silent)==0) {
		printf("# Unflattening done. Summary:\n");
		TIME_CHECK_FMT(unfl_b,map_e,"  Image mapping time: %fs\n");
	}
	TIME_MARK_START(fix_b);
	if (reqaddr==0) {
		fix_unflatten_memory(&FLCTRL.HDR,FLCTRL.mem);
		((struct flatten_header*)(mem-hdr_offset))->fix_base = (uintptr_t)mem + FLCTRL.HDR.ptr_count*sizeof(size_t);
	}
	if ((FLCTRL.option&option_silent)==0) {
		if (reqaddr==0) {
			TIME_CHECK_FMT(fix_b,fix_e,"  Fixing memory time: %fs\n");
		}
		TIME_CHECK_FMT(unfl_b,fix_e,"  Total time: %fs\n");
		printf("  Total bytes mapped: %zu\n",memsz);
	}

	return 0;
}

int unflatten_read(FILE* f) {

	TIME_MARK_START(unfl_b);
	size_t readin = 0;

	size_t rd = fread(&FLCTRL.HDR,sizeof(struct flatten_header),1,f);
	if (rd!=1) return -1; else readin+=sizeof(struct flatten_header);
	if (FLCTRL.HDR.magic!=FLATTEN_MAGIC) {
		fprintf(stderr,"Invalid magic while reading flattened image\n");
		return -1;
	}
	size_t i;
	for (i=0; i<FLCTRL.HDR.root_addr_count; ++i) {
		size_t root_addr_offset;
		size_t rd = fread(&root_addr_offset,sizeof(size_t),1,f);
		if (rd!=1) return -1; else readin+=sizeof(size_t);
		root_addr_append(root_addr_offset);
	}
	LIBFLAT_DBGM("@ ptr count: %zu\n",FLCTRL.HDR.ptr_count);
	LIBFLAT_DBGM("@ root_addr count: %zu\n",FLCTRL.HDR.root_addr_count);
	size_t memsz = FLCTRL.HDR.memory_size+FLCTRL.HDR.ptr_count*sizeof(size_t);
	FLCTRL.mem = malloc(memsz);
	LIBFLAT_DBGM("@ flatten memory: %p:%p:%zu\n",FLCTRL.mem,FLCTRL.mem+memsz-1,memsz);
	assert(FLCTRL.mem);
	rd = fread(FLCTRL.mem,1,memsz,f);
	if (rd!=memsz) return -1; else readin+=rd;
	if ((FLCTRL.option&option_silent)==0) {
		printf("# Unflattening done. Summary:\n");
		TIME_CHECK_FMT(unfl_b,read_e,"  Image read time: %fs\n");
	}
	TIME_MARK_START(fix_b);
	fix_unflatten_memory(&FLCTRL.HDR,FLCTRL.mem);
	if ((FLCTRL.option&option_silent)==0) {
		TIME_CHECK_FMT(fix_b,fix_e,"  Fixing memory time: %fs\n");
		TIME_CHECK_FMT(unfl_b,fix_e,"  Total time: %fs\n");
		printf("  Total bytes read: %zu\n",readin);
	}

	return 0;
}

void unflatten_fini() {
	FLCTRL.rtail = FLCTRL.rhead;
    while(FLCTRL.rtail) {
    	struct root_addrnode* p = FLCTRL.rtail;
    	FLCTRL.rtail = FLCTRL.rtail->next;
    	free(p);
    }
    if (FLCTRL.map_size>0) {
    	assert(munmap(FLCTRL.map_mem,FLCTRL.map_size)==0);
    }
    else {
    	free(FLCTRL.mem);
    }
    --FLCTRL_INDEX;
}

void* root_pointer_next() {
	
	assert(FLCTRL.rhead!=0);

	if (FLCTRL.last_accessed_root==0) {
		FLCTRL.last_accessed_root = FLCTRL.rhead;
	}
	else {
		if (FLCTRL.last_accessed_root->next) {
			FLCTRL.last_accessed_root = FLCTRL.last_accessed_root->next;
		}
		else {
			assert(0);
		}
	}

	if (FLCTRL.last_accessed_root->root_addr==(size_t)-1) {
		return 0;
	}
	else {
		return (FLATTEN_MEMORY_START+FLCTRL.last_accessed_root->root_addr);
	}
}

void* root_pointer_seq(size_t index) {

	assert(FLCTRL.rhead!=0);

	FLCTRL.last_accessed_root = FLCTRL.rhead;

	size_t i=0;
	for (i=0; i<index; ++i) {
		if (FLCTRL.last_accessed_root->next) {
			FLCTRL.last_accessed_root = FLCTRL.last_accessed_root->next;
		}
		else {
			assert(0);
		}
	}

	if (FLCTRL.last_accessed_root->root_addr==(size_t)-1) {
		return 0;
	}
	else {
		return (FLATTEN_MEMORY_START+FLCTRL.last_accessed_root->root_addr);
	}
}

void flatten_set_debug_flag(int flag) {
	FLCTRL.debug_flag = flag;
}

void flatten_set_option(int option) {
	FLCTRL.option |= option;
}

void flatten_clear_option(int option) {
	FLCTRL.option &= ~option;
}

void flatten_debug_memory() {
	unsigned char* m = FLATTEN_MEMORY_START;
	printf("(%p)\n",m);
	printf("[ ");
	for (;m<FLATTEN_MEMORY_END;) {
		printf("%02x ",*m);
		++m;
		if ((m-FLATTEN_MEMORY_START)%32==0)
			printf("\n  ");
	}
	printf("]\n");
}
