/* SPDX-License-Identifier: MIT */

/* StACSOS - Kernel
 *
 * Copyright (c) University of St Andrews 2024
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#pragma once

#include <stacsos/kernel/mem/page-allocator.h>

namespace stacsos::kernel::mem {
class page_allocator_buddy : public page_allocator {
public:
	page_allocator_buddy(memory_manager &mm)
		: page_allocator(mm)
	{
		for (int i = 0; i <= LastOrder; i++) {
			free_list_[i] = nullptr;
		}
	}

	virtual void insert_free_pages(page &range_start, u64 page_count) override;

	virtual page *allocate_pages(int order, page_allocation_flags flags = page_allocation_flags::none) override;
	virtual void free_pages(page &base, int order) override;

	virtual void dump() const override;

private:
	// Represents the contents of a free page, that can hold useful metadata.
	struct page_metadata {
		page *next_free;
	};

	static const int LastOrder = 16;

	page *free_list_[LastOrder + 1];
	u64 total_free_; //TODO: not used.

	void remove_buddies(int order, page &block_start);
	void insert_buddies(int order, page &first_block, page &second_block);

	constexpr u64 pages_per_block(int order) const { return 1 << order; }

	constexpr bool block_aligned(int order, u64 pfn) { return !(pfn & (pages_per_block(order) - 1)); }

	// returns a referance to the pages metadata so it can be read and modified.
	page*& get_next_free(page* block_start) { return ((page_metadata *)(block_start->base_address_ptr()))->next_free; }

	// to find a blocks buddy, its pfn of the block can be xored with its block size. 
	page& get_buddy(int order, page &buddy) { return page::get_from_pfn(buddy.pfn() ^ pages_per_block(order)); }

	void insert_free_block(int order, page &block_start);
	void remove_free_block(int order, page &block_start);

	page** get_candidate_slot(int order, page &block_start);
	page** get_slot(int order, page &block_start);

	void split_block(int order, page &block_start);
	page* merge_buddies(int order, page &buddy); // returns page* in case merge failed.

	page* iterative_split(int target_order);
	void iterative_merge(int order, page *buddy_ptr);

};
} // namespace stacsos::kernel::mem
