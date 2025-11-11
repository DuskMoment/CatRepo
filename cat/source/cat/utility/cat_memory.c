////////////////////////////////////////////////////////////////////////////////
/// Copyright 2025 Daniel S. Buckstein
/// 
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
/// 
///     http://www.apache.org/licenses/LICENSE-2.0
/// 
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
////////////////////////////////////////////////////////////////////////////////

/*
* cat_memory.c
* Memory management implementation.
*/

#include "cat/utility/cat_memory.h"
#include "cat/cat_platform.inl"

#include <assert.h>
#include <string.h>


cat_implementation_begin;


#ifdef CAT_DEBUG
typedef struct cat_malloc_metadata_s
{
#ifdef _WIN32
    //****TO-DO-MEMORY: fill in this structure.
    struct cat_malloc_metadata_s* p_prev;
    struct cat_malloc_metadata_s* p_next;
    char* file;
    uint32_t line;
    uint32_t mode;
    size_t size;
    uint32_t sequence;
    uint32_t reserved;
#else // #ifdef _WIN32
    uint32_t reserved;
#endif // #else // #ifdef _WIN32
} cat_malloc_metadata_t;
#endif // #ifdef CAT_DEBUG

static void* pool;
static size_t poolSize;
static cat_malloc_metadata_t* heap;

cat_impl void* cat_memset(void* const p_block, uint8_t const value, size_t const set_size)
{
    assert_or_bail(p_block) NULL;
    assert_or_bail(set_size) NULL;
    return memset(p_block, value, set_size);
}

cat_impl void* cat_memclr(void* const p_block, size_t const clr_size)
{
    return cat_memset(p_block, 0xFF, clr_size);
}

cat_impl void* cat_memcpy(void* const p_block_dst, void const* const p_block_src, size_t const cpy_size)
{
    assert_or_bail(p_block_dst) NULL;
    assert_or_bail(p_block_src) NULL;
    assert_or_bail(cpy_size) NULL;
    return memcpy(p_block_dst, p_block_src, cpy_size);
}

cat_impl bool cat_memcmp(void const* const p_block_lh, void const* const p_block_rh, size_t const cmp_size)
{
    assert_or_bail(p_block_lh) false;
    assert_or_bail(p_block_rh) false;
    assert_or_bail(cmp_size) false;
    return (memcmp(p_block_lh, p_block_rh, cmp_size) == 0);
}

cat_impl void* cat_malloc(size_t const block_size)
{
#ifdef CAT_DEBUG
    cat_malloc_metadata_t* p_meta = NULL;
#endif // #ifdef CAT_DEBUG
    void* p_block = NULL;
    assert_or_bail(block_size) NULL;
    p_block = malloc(block_size);
#ifdef CAT_DEBUG
    unused(p_meta);
#endif // #ifdef CAT_DEBUG
    return p_block;
}

cat_impl void* cat_calloc(size_t const element_count, size_t const element_size)
{
#ifdef CAT_DEBUG
    cat_malloc_metadata_t* p_meta = NULL;
#endif // #ifdef CAT_DEBUG
    void* p_block = NULL;
    assert_or_bail(element_count) NULL;
    assert_or_bail(element_size) NULL;
    p_block = calloc(element_count, element_size);
#ifdef CAT_DEBUG
    unused(p_meta);
#endif // #ifdef CAT_DEBUG
    return p_block;
}

cat_impl void* cat_realloc(void* const p_block, size_t const block_size)
{
#ifdef CAT_DEBUG
    cat_malloc_metadata_t* p_meta = NULL;
#endif // #ifdef CAT_DEBUG
    void* p_block_new = NULL;
    assert_or_bail(p_block) NULL;
    assert_or_bail(block_size) NULL;
    p_block_new = realloc(p_block, block_size);
#ifdef CAT_DEBUG
    unused(p_meta);
#endif // #ifdef CAT_DEBUG
    return p_block_new;
}

cat_impl void cat_free(void* const p_block)
{
#ifdef CAT_DEBUG
    cat_malloc_metadata_t* p_meta = NULL;
#endif // #ifdef CAT_DEBUG
    assert_or_bail(p_block);
#ifdef CAT_DEBUG
    unused(p_meta);
#endif // #ifdef CAT_DEBUG
    free(p_block);
}

cat_impl bool cat_memory_pool_create(size_t const pool_size)
{
    assert_or_bail(pool_size) false;
    
    //****TO-DO-MEMORY: allocate and initialize pool.
    if (pool == NULL)
    {
        pool = (void*)malloc(pool_size);
        poolSize = pool_size;
        return true;
    }

    return false;
}

cat_impl bool cat_memory_pool_destroy(void)
{
    //****TO-DO-MEMORY: safely deallocate pool allocated above.
    if (pool != NULL)
    {
        free(pool);
        return true;
    }

    return false;
}

cat_impl void* cat_memory_alloc(size_t const block_size)
{
    assert_or_bail(block_size) NULL;

    //****TO-DO-MEMORY: reserve block in managed pool.
    if (pool != NULL)
    {
        //make sure we have memoery to assig
        assert(poolSize > block_size);
        //no allocations so make one
        if (heap == NULL)
        {
            cat_malloc_metadata_t* head = (cat_malloc_metadata_t*)pool;
            head->p_prev = NULL;
            head->p_next = NULL;
            head->sequence = 0;
            head->file = (char*)pool; //is this this correct?
            head->size = block_size;
            head->mode = 0;

            heap = head;

            poolSize -= block_size;

            return (void*)((char*)pool);
        }

        cat_malloc_metadata_t* cur = heap;

        //find the next place in memeoryfile
        while (cur->p_next != NULL)
        {
            cur = cur->p_next;
        }

        cat_malloc_metadata_t* newNode = (cat_malloc_metadata_t*)(cur->file + cur->size);
        cur->p_next = newNode;
        newNode->p_prev = cur;
        newNode->p_next = NULL;
        newNode->sequence = cur->sequence++;
        newNode->mode = 0;
        //try to ge the start of the next node
        newNode->file = ((char*)pool + cur->size);
        newNode->size = block_size;

        poolSize -= block_size;
        return (void*)((char*)pool + cur->size);

    }

    return NULL;
}

cat_impl bool cat_memory_dealloc(void* const p_block)
{
    assert_or_bail(p_block) false;

    //****TO-DO-MEMORY: safely release block reserved above.

    assert(heap != NULL);
    
    cat_malloc_metadata_t* cur = heap;

    while (cur->p_next != NULL)
    {
        if (cur->file == (char*)p_block)
        {
            cat_malloc_metadata_t* prev = cur->p_prev;
            cat_malloc_metadata_t* next = cur->p_next;

            if (prev != NULL)
            {
                prev->p_next = next;
            }

            if (next != NULL)
            {
                next->p_prev = prev;
            }


            poolSize += cur->size;
        }
        cur = cur->p_next;
    }
    


    return false;
}


#include "cat/utility/cat_time.h"
#include "cat/utility/cat_console.h"


cat_noinl void cat_memory_test(void)
{
    bool result = false;
    void* block_lh = cat_malloc(1024);
    void* block_rh = cat_malloc(2048);

    bool test = cat_memory_pool_create(2048);
    assert(test == true);

    //intentinal memeory leak
    void* aloc;
    unused(aloc);

    for (int i = 0; i < 2048; i++)
    {
        cat_memory_alloc(100);
    }

    if (block_lh && block_rh)
    {
        cat_console_clear();
        cat_memset(block_lh, 0xFF, 1024);
        cat_memclr(block_rh, 2048);
        result = cat_memcmp(block_lh, block_rh, 2048);
        printf("\nMemory: \n    Blocks are equal: %"PRIi32, (int32_t)result);
        cat_memcpy(block_lh, block_rh, 1024);
        result = cat_memcmp(block_lh, block_rh, 1024);
        printf("\nMemory: \n    Blocks are equal: %"PRIi32, (int32_t)result);
        cat_platform_sleep(cat_platform_time_rate());
    }

    cat_free(block_lh);
    block_lh = NULL;
    cat_free(block_rh);
    block_rh = NULL;

}


cat_implementation_end;