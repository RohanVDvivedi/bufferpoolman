#include<bufferpool.h>
#include<io_dispatcher.h>

static void* io_page_replace_task(bufferpool* buffp)
{
	// get the page reqest that is most crucial to fulfill
	page_request* page_req_to_fulfill = get_highest_priority_page_request_to_fulfill(buffp->rq_prioritizer);

	if(page_req_to_fulfill == NULL)
	{
		return NULL;
	}

	uint32_t page_id = page_req_to_fulfill->page_id;

	// initialize a dummy page entry, and perform read from disk io on it, without acquiring any locks
	// since it is a local variable, we can perform io, without taking any locks
	// the new_page_memory is the variable that will hold the page memory frame read from the disk file
	page_entry dummy_page_ent;
	reset_page_to(&dummy_page_ent, page_id, page_id * buffp->number_of_blocks_per_page, buffp->number_of_blocks_per_page, allocate_page_frame(buffp->pfa_p));
	read_page_from_disk(&dummy_page_ent, buffp->db_file);

	page_entry* page_ent = NULL;

	while(page_ent == NULL)
	{
		wait_if_lru_is_empty(buffp->lru_p);

		int is_page_valid = 0;

		while(is_page_valid == 0)
		{
			// get the page entry, that is best fit for replacement
			page_ent = get_swapable_page(buffp->lru_p);

			if(page_ent == NULL)
			{
				break;
			}
			else
			{
				pthread_mutex_lock(&(page_ent->page_entry_lock));

				// even though a page_entry may be provided as being fit for replacement, we need to ensure that 
				if(page_ent->pinned_by_count == 0)
				{
					if(page_ent->page_memory != NULL)
					{
					// clean the page entry here, before you discard it from hashmaps,
					// this will ensure that the page that is being evicted has reached to disk
					// before someone comes along and tries to read it again
						// if the page_entry is dirty, then write it to disk and clear the dirty bit
						if(page_ent->is_dirty)
						{
							acquire_read_lock(page_ent);
								write_page_to_disk(page_ent, buffp->db_file);
							release_read_lock(page_ent);

							// since the cleanup is performed, the page is now not dirty
							page_ent->is_dirty = 0;
						}

						if(discard_page_request_if_not_referenced(buffp->rq_tracker, page_ent->page_id) == 1)
						{
							discard_page_entry(buffp->pg_tbl, page_ent);
							break;
						}
						else
						{// if the page request corresponding to the page entry is being used,
						// so it was anyway going to be discarded from the lru,
						// this is the reason why we would not worry about inserting it back to lru
							pthread_mutex_unlock(&(page_ent->page_entry_lock));
							page_ent = NULL;
							continue;
						}
					}
					else
					{
						break;
					}
				}
				else
				{
					pthread_mutex_unlock(&(page_ent->page_entry_lock));
					page_ent = NULL;
					continue;
				}
			}
		}
	}

	if(page_ent->page_id != page_id || page_ent->page_memory == NULL)
	{
		acquire_write_lock(page_ent);

			// release current page frame memory
			if(page_ent->page_memory != NULL)
				free_page_frame(buffp->pfa_p, page_ent->page_memory);
			// above you can skip the NULLing of the page memory variable since it is anyway going to be replaced

			// update the page_id, start_block_id, number_of_blocks and 
			// and the page memory that is already read from the disk,
			// note : remember the read io was performed on the new_page_memory, by the dummy_page_ent
			// and now by replacing the page_memory the page_entry now contains new valid required data
			reset_page_to(page_ent, page_id, page_id * buffp->number_of_blocks_per_page, buffp->number_of_blocks_per_page, dummy_page_ent.page_memory);

		release_write_lock(page_ent);

		// no compression support yet
		page_ent->is_compressed = 0;

		// also reinitialize the usage count
		page_ent->usage_count = 0;
		// and update the last_io timestamp, acknowledging when was the io performed
		setToCurrentUnixTimestamp(page_ent->unix_timestamp_since_last_disk_io_in_ms);
	}

	pthread_mutex_unlock(&(page_ent->page_entry_lock));

	fulfill_requested_page_entry_for_page_request(page_req_to_fulfill, page_ent);
	
	return NULL;
}

typedef struct cleanup_params cleanup_params;
struct cleanup_params
{
	int is_on_heap_memory;
	bufferpool* buffp;
	page_entry* page_ent;
};

static void* io_clean_up_task(cleanup_params* cp)
{
	page_entry* page_ent = cp->page_ent;
	bufferpool* buffp = cp->buffp;
	if(cp->is_on_heap_memory)
		free(cp);

	if(page_ent != NULL)
	{
		pthread_mutex_lock(&(page_ent->page_entry_lock));

			// clean up for the page, only if it is dirty
			if(page_ent->is_dirty)
			{
				acquire_read_lock(page_ent);
					write_page_to_disk(page_ent, buffp->db_file);
				release_read_lock(page_ent);

				// since the cleanup is performed, the page is now not dirty
				page_ent->is_dirty = 0;

				// update the last_io timestamp, acknowledging when was the io performed
				setToCurrentUnixTimestamp(page_ent->unix_timestamp_since_last_disk_io_in_ms);
			}

			// whether cleanup was performed or not, the page_entry is now not in queue, because the cleanup task is complete
			// there is a possibility that for some reason the page_entry was found already clean, and so the clean up action was not performed
			page_ent->is_queued_for_cleanup = 0;

		pthread_mutex_unlock(&(page_ent->page_entry_lock));
	}

	return NULL;
}

void queue_job_for_page_request(bufferpool* buffp)
{
	submit_function(buffp->io_dispatcher, (void*(*)(void*))io_page_replace_task, buffp);
}

void queue_page_entry_clean_up_if_dirty(bufferpool* buffp, page_entry* page_ent)
{
	pthread_mutex_lock(&(page_ent->page_entry_lock));
		if(page_ent->is_dirty && !page_ent->is_queued_for_cleanup)
		{
			cleanup_params* cp = malloc(sizeof(cleanup_params));
			(*cp) = (cleanup_params){.is_on_heap_memory = 1, .buffp = buffp, .page_ent = page_ent};
			submit_function(buffp->io_dispatcher, (void* (*)(void*))io_clean_up_task, cp);
			page_ent->is_queued_for_cleanup = 1;
		}
	pthread_mutex_unlock(&(page_ent->page_entry_lock));
}

void queue_and_wait_for_page_entry_clean_up_if_dirty(bufferpool* buffp, page_entry* page_ent)
{
	int cleanup_job_queued = 0;
	job cleanup_job;
	cleanup_params cp = {.is_on_heap_memory = 0, .buffp = buffp, .page_ent = page_ent};

	pthread_mutex_lock(&(page_ent->page_entry_lock));
		if(page_ent->is_dirty && !page_ent->is_queued_for_cleanup)
		{
			initialize_job(&cleanup_job, (void*(*)(void*))io_clean_up_task, &cp);
			submit_job(buffp->io_dispatcher, &cleanup_job);
			page_ent->is_queued_for_cleanup = 1;
			cleanup_job_queued = 1;
		}
	pthread_mutex_unlock(&(page_ent->page_entry_lock));

	if(cleanup_job_queued)
	{
		get_result(&cleanup_job);

		// take lock to reposition the page entry in lru, so as to make the lsu know that this dirty page was cleaned
		pthread_mutex_lock(&(page_ent->page_entry_lock));
			// if the page is not pinned, i.e. it is not in use by anyone, we simple insert it it lru
			// and mark that it has not been used since long
			// only unpinned pages must be inserted to the LRU
			if(page_ent->pinned_by_count == 0)
			{
				// this function handles reinserts on its own, so no need to worry about that
				mark_as_not_yet_used(buffp->lru_p, page_ent);
			}
		pthread_mutex_unlock(&(page_ent->page_entry_lock));
	}
}