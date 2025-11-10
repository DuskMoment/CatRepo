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
* cat_thread.c
* Thread management implementation.
*/


#include "cat/utility/cat_thread.h"
#include "cat/cat_platform.inl"

#include <threads.h>
#ifdef _WIN32
#include <Windows.h>
#else // #ifdef _WIN32
#endif // #else // #ifdef _WIN32


cat_implementation_begin;


static int cat_thrd_internal_entry_point(cat_thread_params_t const* const p_thread_params)
{
    assert_or_bail(p_thread_params) 1;
    assert_or_bail(p_thread_params->func) 1;
    assert_or_bail((p_thread_params->argc == 0) || p_thread_params->argv) 1;
    return p_thread_params->func(p_thread_params->argc, p_thread_params->argv);
}


cat_impl int cat_thrd_create(thrd_t* const p_thread_out, cat_thread_params_t const* const p_thread_params)
{
    assert_or_bail(p_thread_out) thrd_error;
    assert_or_bail(p_thread_params) thrd_error;
    return thrd_create(p_thread_out, &cat_thrd_internal_entry_point, (void*)p_thread_params);
}

cat_impl bool cat_thread_rename(cstr_t const name)
{
    bool result = false;
    assert_or_bail(name) false;
#ifdef _WIN32
    // Thread rename: 
    // https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2015/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2015&redirectedfrom=MSDN
    {
#pragma warning(push)
#pragma warning(disable: 4820 6320 6322)// suppress warnings
#pragma pack(push, 8) // 8-bit alignment
	    // name change structure: 
	    // declare a data structure that can be used internally
        struct tagTHREADNAME_INFO
        {
            DWORD  type;    // reserved (must be 0x1000)
            LPCSTR name;    // name string (provided by caller)
            DWORD  threadID;// thread ID (-1 for calling thread)
            DWORD  flags;   // reserved (must be zero)
        } const info = { 0x1000, name, (DWORD)(-1), 0 };
#pragma pack(pop)
        __try
        {
            // attempt name change
            DWORD const exception = 0x406D1388;
            RaiseException(exception, 0,
                (sizeof(info) / sizeof(ULONG_PTR)),
                (ULONG_PTR const*)(&info));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // unhandled exception
        }
        result = true;
#pragma warning(pop)
    }
#else // #ifdef _WIN32
    // Not supported.
#endif // #else // #ifdef _WIN32
    return result;
}


#include "cat/utility/cat_time.h"
#include "cat/utility/cat_console.h"

typedef struct thread_data
{
    int executing;
    int result;
} thread_data;

typedef struct thread_data_node
{
    struct thread_data_node* nextNode;
    thread_data thrd_data;
} thread_data_node;

typedef struct thread_manager
{
    thread_data_node* deactive_thread_list;
    thread_data_node* active_thread_list;
} thread_manager;

static int cat_thread_test_func(size_t const argc, void* const argv[])
{
    int result = 0;
    thrd_t const* p_thrd = NULL;
    cstr_t thrd_name = NULL;
    int print_count = 0;

    assert_or_bail((argc == 3) && argv && argv[0] && argv[1] && argv[2]) 1;
    p_thrd = (thrd_t const*)argv[0];
    thrd_name = (cstr_t)argv[1];
    print_count = *(int const*)argv[2];
    result |= !cat_thread_rename(thrd_name);
    printf("\nThread: \n    thrd_name=\"%s\" id=%"PRIu32, thrd_name, p_thrd->_Tid);
    while ((print_count > 0) != 0)
    {
        if ((print_count % 1000) == 0)
            printf("\n    print_count=%"PRIi32, print_count);
        --print_count;
    }
    return result;
}

static int new_thread_test_func(size_t const argc, void* const argv[])
{
    thrd_t const* p_thrd = NULL;
    thread_data * p_thrd_data = NULL;
    assert_or_bail((argc == 2) && argv && argv[0] && argv[1]) 1;
    p_thrd = (thrd_t const*)argv[0];
    p_thrd_data = (thread_data *)argv[1];

    cat_platform_sleep(cat_platform_time_rate() * 5);

    p_thrd_data->result = 21;
    p_thrd_data->executing = 0;

    return 0;
}

static int thrd_test_func(void* const arg)
{
    unused(arg);
    printf("\n%s", __FUNCTION__);
    cat_platform_sleep(cat_platform_time_rate());
    return 0;
}

cat_noinl void create_new_thread(thread_manager* const thrd_manager)
{
    thread_data_node* currNode = thrd_manager->deactive_thread_list;

    if (currNode == NULL)
    {
        currNode = (thread_data_node*)malloc(sizeof(thread_data_node));
        currNode->nextNode = NULL;

        thrd_manager->deactive_thread_list = currNode;

        return;
    }

    while (currNode->nextNode != NULL)
    {
        currNode = currNode->nextNode;
    }

    currNode->nextNode = (thread_data_node*)malloc(sizeof(thread_data_node));
    currNode->nextNode->nextNode = NULL;
}

cat_noinl int run_new_thread(thread_manager* const thrd_manager, cat_thread_func_t func, thrd_t thrd, cat_thread_params_t params)
{
    unused(func);
    if (thrd_manager->deactive_thread_list != NULL)
    {
        //Saves first avalable thread ptr
        thread_data_node* temp = thrd_manager->deactive_thread_list;

        //Removes the first thread ptr off the deactive list 
        thrd_manager->deactive_thread_list = thrd_manager->deactive_thread_list->nextNode;

        //Adds the thread to the front of the active list
        temp->nextNode = thrd_manager->active_thread_list;

        //Reassigns the active list start pointer
        thrd_manager->active_thread_list = temp;

        //Creates and runs the actual std::thread
        int thrd_res = cat_thrd_create(&thrd, &params);
        assert_or_bail(thrd_res == thrd_success) 0;
        thrd_detach(thrd);
        
        //int res = 0;
        //thrd_join(thrd, &res);

        //Sets executing value
        thrd_manager->active_thread_list->thrd_data.executing = 1;

        return 1;
    }

    return 0;
}

cat_noinl void handle_thread_finished(thread_manager* const thrd_manager, thread_data_node* nodeDoneExecuting)
{
    thread_data_node* currNode = thrd_manager->active_thread_list;
    thread_data_node* prevNode = NULL;

    //To lazy to make a doubly linked list so just iterate until we find the 
    // current node and keep track of the before node
    while (currNode != nodeDoneExecuting && currNode != NULL)
    {
        prevNode = currNode;
        currNode = currNode->nextNode;
    }

    if (prevNode == NULL)
    {
        //Updates active thread list start
        thrd_manager->active_thread_list = currNode->nextNode;
    }
    else if (currNode != NULL)
    {
        //Updates active thread list connections
        prevNode->nextNode = currNode->nextNode;
    }

    currNode->nextNode = thrd_manager->deactive_thread_list;
    thrd_manager->deactive_thread_list = currNode;
}

cat_noinl void free_thread_manager(thread_manager* thrd_manager)
{
    thread_data_node* currNode = thrd_manager->active_thread_list;
    thread_data_node* nextNode = NULL;

    while (currNode != NULL)
    {
        nextNode = currNode->nextNode;
        free(currNode);
        currNode = nextNode;
    }

    currNode = thrd_manager->deactive_thread_list;
    nextNode = NULL;

    while (currNode != NULL)
    {
        nextNode = currNode->nextNode;
        free(currNode);
        currNode = nextNode;
    }

    free(thrd_manager);
}

cat_noinl void cat_thread_test(void)
{
    //thrd_t thrd = { 0 };
    //int thrd_res = 0;
    //int print_count = 10000;
    //void* const args[] = {
    //    &thrd,       // thread object
    //    __FUNCTION__,// thread name
    //    &print_count,// print count
    //};
    //cat_thread_params_t const params = {
    //    &cat_thread_test_func, array_count(args), args
    //};

    //NEW VALUES
    thread_manager* const thrd_manager = (thread_manager*)malloc(sizeof(thread_manager));
    thrd_manager->active_thread_list = NULL;
    thrd_manager->deactive_thread_list = NULL;

    create_new_thread(thrd_manager);
    create_new_thread(thrd_manager);

    //run_new_thread();

    thrd_t thrd = { 0 };
    void* const args[] = {
    &thrd,       // thread object
    &thrd_manager->deactive_thread_list->thrd_data,
    };

    cat_thread_params_t const params = {
        &new_thread_test_func, array_count(args), args
    };

    run_new_thread(thrd_manager, new_thread_test_func, thrd, params);

    thrd_t thrd2 = { 0 };
    void* const args2[] = {
    &thrd2,       // thread object
    &thrd_manager->deactive_thread_list->thrd_data,
    };

    cat_thread_params_t const params2 = {
        &new_thread_test_func, array_count(args2), args2
    };
    run_new_thread(thrd_manager, new_thread_test_func, thrd2, params2);

    cat_console_clear();
    /*{
        thrd_res = thrd_create(&thrd, &thrd_test_func, NULL);
        assert_or_bail(thrd_res == thrd_success);
        thrd_join(thrd, &thrd_res);
    }
    {
        thrd_res = cat_thrd_create(&thrd, &params);
        assert_or_bail(thrd_res == thrd_success);
        thrd_join(thrd, &thrd_res);
    }*/
    {
        /*thrd_res_new = cat_thrd_create(&thrd_new, &params_new);
        assert_or_bail(thrd_res_new == thrd_success);
        thrd_detach(thrd_new);*/
    }

    while (thrd_manager->active_thread_list != NULL)
    {
        thread_data_node* currNode = thrd_manager->active_thread_list;
        thread_data_node* nextNode = NULL;

        while (currNode != NULL)
        {
            if (currNode->thrd_data.executing == 0)
            {
                nextNode = currNode->nextNode;
                handle_thread_finished(thrd_manager, currNode);
                currNode = nextNode;
            }
        }
    }

    cat_platform_sleep(cat_platform_time_rate());

    free_thread_manager(thrd_manager);
}


cat_implementation_end;