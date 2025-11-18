/* SPDX-License-Identifier: MIT */

/* StACSOS - Kernel
 *
 * Copyright (c) University of St Andrews 2024, 2025
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#include <stacsos/kernel/debug.h>
#include <stacsos/kernel/mem/page-allocator-buddy.h>
#include <stacsos/kernel/mem/page.h>
#include <stacsos/memops.h>

using namespace stacsos;
using namespace stacsos::kernel;
using namespace stacsos::kernel::mem;

#define UNIT64_MAX 18446744073709551615

//////////////////////
// Public functions //
//////////////////////

/**
 * @brief Dumps out (via the debugging routines) the current state of the buddy page allocator's free lists
 */
void page_allocator_buddy::dump() const
{
	// Print out a header, so we can quickly identify this output in the debug stream.
	dprintf("*** buddy page allocator - free list ***\n");

	// Loop over each order that our allocator is responsible for, from zero up to *and
	// including* LastOrder.
	for (int order = 0; order <= LastOrder; order++) {
		// Print out the order number (with a leading zero, so that it's nicely aligned)
		dprintf("[%02u] ", order);

		// Get the pointer to the first free page in the free list.
		page *current_free_page = free_list_[order];

		// While there /is/ currently a free page in the list...
		while (current_free_page) {
			// Print out the extents of this page, i.e. its base address (at byte granularity), up to and including the last
			// valid address.  Remember: these are PHYSICAL addresses.
			dprintf("%lx--%lx ", current_free_page->base_address(), (current_free_page->base_address() + (pages_per_block(order) << PAGE_BITS)) - 1);

			// Advance to the next page, by interpreting the free page as holding metadata, and reading
			// the appropriate field.
			current_free_page = ((page_metadata *)(current_free_page->base_address_ptr()))->next_free;
		}

		// New line for the next order.
		dprintf("\n");
	}
}

/**
 * @brief Inserts pages that are known to be free into the buddy allocator.
 *
 * @param range_start page&. The first page in the range.
 * @param page_count u64. The number of pages in the range.
 */
void page_allocator_buddy::insert_free_pages(page &range_start, u64 page_count) { 

	int order = 0;
	u64 lsb = 1; //lowest significant bit
	u64 pfn = range_start.pfn();
	u64 max_block_size = 1 << LastOrder;

	// prevent any overflow
	assert(pfn < (UNIT64_MAX - page_count));

	// all blocks of order n start at a pfn with n trailing 0s
	// e.g. pfn is 0110, 1 trailing zero so start of order 1 block.
	// After inserting into order next block to insert will always have more trailing 0s
	// e.g pfn 0110 inserted into order 1, next free block starts at pfn (0110 + 0010) = 1000
	//     with an order 3 insertion to follow
	while (page_count >= lsb && order < LastOrder) { 

		// if current bit is 1. (starting from the lowest bit)
		if (lsb & pfn) {
			free_pages(page::get_from_pfn(pfn), order);
			page_count -= lsb;
			pfn += lsb;
		}

		// move to next bit and move to next order. (order will match leading zeros of pfn) 
		lsb <<= 1;
		order++;
	}
	
	// if blocks fit in orders higher than allowed, add them as highest order instead.
	while (page_count > max_block_size) {
		insert_free_block(LastOrder, page::get_from_pfn(pfn)); //dont need to use free_pages because merge is not possible
		page_count -= max_block_size;
		pfn += max_block_size;
	}

	// Insert the remaining pages.
	// orders informed by bits in remaining page count.
	// e.g. page count remaining is 0100'0101, insert blocks of order 6, 2 then 0.
	while (lsb > 0) {
		
		// from previous while loops lsb is greater than page_count.
		lsb >>= 1;
		order--;
		if (lsb & page_count) {
			free_pages(page::get_from_pfn(pfn), order);
			pfn += lsb;
		}
	}
}

/**
 * @brief remove a block from a chosen order to be allocated. if no blocks in that order, split higher orders.
 *
 * @param order int.
 * @param flags page_allocation_flags. if zero then will zero all bits in page.
 * @return page*. start of allocated block. Nullptr if no pages could be allocated. 
 */
page *page_allocator_buddy::allocate_pages(int order, page_allocation_flags flags) {

	// fail if not in range 
	if (order < 0 || order > LastOrder) {
		return nullptr;
	}

	// if no blocks are present in order requested, split blocks of higher orders until there is
	page* chosen = iterative_split(order);
	
	// if no block of requested order could be made.
	if (!chosen) {
		return nullptr;
	}
	
	// allocate the page.
	remove_free_block(order, *chosen);
	
	// if flag set, zero page
	if ((flags & page_allocation_flags::zero) == page_allocation_flags::zero) {
 		   memops::pzero(chosen->base_address_ptr(), (1 << order));
  	}
	
	return chosen;
}

/**
 * @brief safeley add pages that are now free to the buddy allocator.
 *
 * @param block_start page&. start of block.
 * @param order int.
 */
void page_allocator_buddy::free_pages(page &block_start, int order) {

	// assert order is in range
	assert(order >= 0 && order <= LastOrder);

	insert_free_block(order, block_start);

	// if inserted block has a free buddy, merge them. Repeat in next order until no more merges can be made
	iterative_merge(order, &block_start);
}

///////////////////////
// Private Functions //
///////////////////////

/**
 * @brief merge block with its buddy, then try again in the next order. Repeating until no more merges can be made.
 * \n calling this whenever a block is added to the allocator ensures all blocks remain merged where possible.
 * 
 * @param order int
 * @param buddy_ptr page*. the page being merged.
 */
void page_allocator_buddy::iterative_merge(int order, page *buddy_ptr) { 

	// no need to assert order is in range as merge_buddies() does this

	while (buddy_ptr && order < LastOrder) {
		buddy_ptr = merge_buddies(order, *buddy_ptr);
		order++;
	}
}

/**
 * @brief split a block of nearest higher order until there is a block of the requested order.
 * \n call this whenever a page of specific order is needed and that order is potentially empty.

 * @param target_order int.
 * @return page*. page of order requested. Nullptr if no page found.
 */
page* page_allocator_buddy::iterative_split(int target_order) {

	// assert order is in range
	assert(target_order >= 0 && target_order <= LastOrder);

	// if block in requested order already exists, just use that block
	if (free_list_[target_order]) {
		return free_list_[target_order];
	}

	// move up to next order to look there, repeat until a block is found	
	int order = target_order + 1;
	while (order <= LastOrder && !free_list_[order]) {
		order++;
	}

	// if no block is found:
	if (order > LastOrder) {
		return nullptr;
	}

	// split blocks down orders until there is a block of the requested order
	while (order > target_order) {
		split_block(order, *free_list_[order]);
		order--;
	}

	return free_list_[target_order];
}

/**
 * @brief check if a free block's buddy is also free. If it is, merge the two blocks into the next order up. 
 *
 * @param order int.
 * @param buddy page&. page to be merged.
 * @return page*. the new block. Nullptr if no buddy found.
 */
page* page_allocator_buddy::merge_buddies(int order, page &buddy) {

	// assert order in range
	assert(order >= 0 && order < LastOrder);

	// find blocks buddy using bitwise operations.
	page &buddy2 = get_buddy(order, buddy);

	// identify which block comes first, as this will inform the base of the larger merged block.
	page &first = (buddy.pfn() < buddy2.pfn()) ? buddy : buddy2; // logical operators used here for compact code
	page &second = (buddy.pfn() < buddy2.pfn()) ? buddy2 : buddy;

	// if the first buddy does not point to second buddy, one or both are not free.
	if (get_next_free(&first) != &second) {
		return nullptr;
	}

	// remove the two halfs and insert one whole block into the order above.
	remove_buddies(order, first);
	insert_free_block(order + 1, first);

	// return start of new block, also shows merge was successful.
	return &first;
}

/**
 * @brief remove given block from its order and insert two blocks into order below.
 *
 * @param order int.
 * @param block_start page&.
 */
void page_allocator_buddy::split_block(int order, page &block_start) { 

	// assert order in range
	assert(order > 0 && order <= LastOrder);

	// get block starting halfway through the current block, essentially splits the block in two
	page &next_block = page::get_from_pfn(block_start.pfn() + pages_per_block(order - 1));

	// remove the split block from its order and insert halfs into order below.
	remove_free_block(order, block_start);
	insert_buddies(order - 1, block_start, next_block);
}

/**
 * @brief inserts a free block into the buddy allocator.
 *
 * @param order int.
 * @param block_start base page of block to insert
 */
void page_allocator_buddy::insert_free_block(int order, page &block_start) {

	// get pointer to block metadata from the linked list
	page** slot = get_slot(order, block_start); //get_slot() asserts order in range

	// add this block to the list
	get_next_free(&block_start) = *slot;
	*slot = &block_start;
}

/**
 * @brief removes a free block from the buddy allocator
 *
 * @param order int.
 * @param block_start base page of block to insert
 */
void page_allocator_buddy::remove_free_block(int order, page &block_start) {

	// get pointer to block metadata pointing to this block
	page** candidate_slot = get_candidate_slot(order, block_start); //get_canditate_slot() asserts order in range

	// remove this block from the list 
	*candidate_slot = get_next_free(&block_start);
	get_next_free(&block_start) = nullptr;
}

/**
 * @brief insert two blocks that are buddies into the allocator.
 *
 * @param order int.
 * @param first_block page. The buddy with lower pfn
 * @param second_block page. the buddy with higher pfn
 * */
void page_allocator_buddy::insert_buddies(int order, page &first_block, page &second_block) {
	
	// get pointer to block metadata from linked list
	page** slot = get_slot(order, first_block);
	assert(block_aligned(order, second_block.pfn()));

	// add both blocks to the list
	get_next_free(&first_block) = &second_block;
	get_next_free(&second_block) = *slot;
	*slot = &first_block;
}

/**
 * @brief removes two buddies from the allocator
 * 
 * @param order int.
 * @param block_start page. the buddy with lower pfn.
*/
void page_allocator_buddy::remove_buddies(int order, page &block_start) {
	
	// get poiner to block metadata pointing to these blocks
	page** candidate_slot = get_candidate_slot(order, block_start);

	page* second_block = get_next_free(&block_start);
	assert(block_aligned(order, second_block->pfn()));

	// remove both blocks from the list
	*candidate_slot = get_next_free(second_block);
	get_next_free(second_block) = nullptr;
	get_next_free(&block_start) = nullptr;
}


/**
 * @brief gets the block metadata that should point to the block to be inserted.
 *
 * @param order int.
 * @param block_start page&.
 */
page** page_allocator_buddy::get_slot(int order, page &block_start)
{
	// assert order in range
	assert(order >= 0 && order <= LastOrder && "Order out of range for inserting free block");

	// assert block_start aligned to order
	assert(block_aligned(order, block_start.pfn()));
	page *target = &block_start;
	page **slot = &free_list_[order];
	while (*slot && *slot < target) {
		// slot = &((*slot)->next_free_);
		slot = &(get_next_free(*slot));
	}

	assert(*slot != target);

	return slot;
}

/**
 * @brief gets the block metadata that points to the chosen block.
 * is used for deletion, so that the previous block can be linked to the one after.
 *
 * @param order
 * @param block_start
 */
page** page_allocator_buddy::get_candidate_slot(int order, page &block_start)
{
	// assert order in range
	assert(order >= 0 && order <= LastOrder);

	// assert block_start aligned to order
	assert(block_aligned(order, block_start.pfn()));

	page *target = &block_start;
	page **candidate_slot = &free_list_[order];
	while (*candidate_slot && *candidate_slot != target) {
		candidate_slot = &(get_next_free(*candidate_slot)); // &((*candidate_slot)->next_free_);
	}

	// assert candidate block exists
	assert(*candidate_slot == target);

	return candidate_slot;
}

//TODO: make sure that logical opperators are my choice

//TODO: add in page states -  iterative functions for merging and splitting - a function that checks for a buddy. - decide on a standard for buddy parameters - maybe make itterative merging and splitting efficient - could change the list to unordered to better optimise insertion - O(1) instead of O(n), I wonder if there is also a way to make the merge O(1)

//REPORT: recusion not good idea - when am I freeing buddies - why I wrote extra code for splitting and merging - merging on free keeps memory fragmentation low, also, all it needs is a buddy lookup - get next physical blocl is ajust a wee efficiency - chose to itterative split too - why merge now returns page pointer, not even friend needs it - avoided recursion for inserting pages - take through steps of design for the adding of pages

//NEXT: go over and sort relations with header file, replace all occurances of metadata with function.