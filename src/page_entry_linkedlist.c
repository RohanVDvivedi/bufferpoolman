#include<page_entry_linkedlist.h>

page_entry_linkedlist* get_page_entry_linkedlist(uint32_t page_entry_count, uint32_t page_size_in_bytes, void* first_page_memory_address)
{
	page_entry_linkedlist* pel_p = (page_entry_linkedlist*) malloc(sizeof(page_entry_linkedlist));
	pel_p->page_entries = get_linkedlist(SIMPLE, NULL);
	pel_p->node_mapping = get_page_memory_mapper(first_page_memory_address, page_size_in_bytes, page_entry_count);
}

int is_empty_page_entry_linkedlist(page_entry_linkedlist* pel_p)
{
	return ((pel_p->page_entries->head == NULL) && (pel_p->page_entries->tail == NULL));
}

int is_absent_in_page_entry_linkedlist(page_entry_linkedlist* pel_p, page_entry* page_ent)
{
	return (get_by_page_entry(pel_p->node_mapping, page_ent) == NULL);
}

int is_present_in_page_entry_linkedlist(page_entry_linkedlist* pel_p, page_entry* page_ent)
{
	return (get_by_page_entry(pel_p->node_mapping, page_ent) != NULL);
}

void insert_head_page_entry_linkedlist(page_entry_linkedlist* pel_p, page_entry* page_ent)
{
	if(is_absent_in_page_entry_linkedlist(pel_p, page_ent))
	{
		insert_head(pel_p->page_entries, page_ent);
		set_by_page_entry(pel_p->node_mapping, page_ent, pel_p->page_entries->head);
	}
}

void insert_tail_page_entry_linkedlist(page_entry_linkedlist* pel_p, page_entry* page_ent)
{
	if(is_absent_in_page_entry_linkedlist(pel_p, page_ent))
	{
		insert_tail(pel_p->page_entries, page_ent);
		set_by_page_entry(pel_p->node_mapping, page_ent, pel_p->page_entries->tail);
	}
}

page_entry* pop_head_page_entry_linkedlist(page_entry_linkedlist* pel_p)
{
	page_entry* page_ent = NULL;
	if(!is_empty_page_entry_linkedlist(pel_p))
	{
		page_ent = (page_entry*) get_head_data(pel_p->page_entries);
		remove_from_page_entry_linkedlist(pel_p, page_ent);
	}
	return page_ent;
}

page_entry* pop_tail_page_entry_linkedlist(page_entry_linkedlist* pel_p)
{
	page_entry* page_ent = NULL;
	if(!is_empty_page_entry_linkedlist(pel_p))
	{
		page_ent = (page_entry*) get_tail_data(pel_p->page_entries);
		remove_from_page_entry_linkedlist(pel_p, page_ent);
	}
	return page_ent;
}

int remove_from_page_entry_linkedlist(page_entry_linkedlist* pel_p, page_entry* page_ent)
{
	if(is_present_in_page_entry_linkedlist(pel_p, page_ent))
	{
		remove_node(pel_p->page_entries, get_by_page_entry(pel_p->node_mapping, page_ent));
		set_by_page_entry(pel_p->node_mapping, page_ent, NULL);
	}
}

void delete_page_entry_linkedlist(page_entry_linkedlist* pel_p)
{
	delete_linkedlist(pel_p->page_entries);
	delete_page_memory_mapper(pel_p->node_mapping);
	free(pel_p);
}