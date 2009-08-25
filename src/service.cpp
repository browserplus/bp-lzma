/*
 * Copyright 2009, Yahoo!
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 * 
 *  3. Neither the name of Yahoo! nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ServiceAPI/bperror.h>
#include <ServiceAPI/bptypes.h>
#include <ServiceAPI/bpdefinition.h>
#include <ServiceAPI/bpcfunctions.h>
#include <ServiceAPI/bppfunctions.h>

#include "bptypeutil.hh"
#include "bpurlutil.hh"

#include "util/bpsync.hh"
#include "util/bpthread.hh"
#include "util/fileutil.hh"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>

#include <iostream>
#include <list>

const BPCFunctionTable * g_bpCoreFunctions = NULL;


// Data structures used to manage tasks.  We only allow one task
// to run per session to put a limit on per-session resource consumption.
// In some cases this could prevent us from leveraging multiple logical
// cores, but this is arguably better than allowing any random site to
// 0wn your proc.
struct Task {
    typedef enum { T_Compress, T_Decompress } Type;

    Task(Type t) : type(t), tid(0), cid(0), canceled(false) { }
    Type type;
    std::string inPath;
    std::string outPath;    
    unsigned int tid; 
    unsigned int cid;
    bool canceled;
};

static
void performTask(Task * t)
{
    std::cout << "tf: performing task: " << t->inPath
              << " -> " << t->outPath << std::endl;

    // XXX: write me!  (where the actual work gets done :/)

    g_bpCoreFunctions->postError(t->tid, "bp.notImplemented", NULL);
}

// per-session (aka, per-site) data includes a list of tasks to perform,
// a thread to run tasks (the thread is not for performance, but rather
// to support asynchronous cancelation in the middle of a (de)compression),
// and a simple mutex
struct SessionData {
    std::list<Task> taskList;
    bp::sync::Mutex mutex;
    bp::sync::Condition cond;
    bp::thread::Thread thread;
    bool running;
    std::string tempDir;
};

static
void * threadFunc(void * sdp) 
{
    std::cout << "tf: threadfunc" << std::endl;
    
    SessionData * sd = (SessionData *) sdp;

    sd->mutex.lock();

    while (sd->running) {
        if (sd->taskList.size() > 0) {
            sd->mutex.unlock();            

            // perform the (de)compression task *outside* of our lock,
            // in the case of async cancelation, the bool 'canceled'
            // will be toggled
            performTask(&(*(sd->taskList.begin())));

            sd->mutex.lock();

            // the task was either canceled or completed.  in either case
            // we can delete it *inside* the lock
            sd->taskList.erase(sd->taskList.begin());

        } else {
            std::cout << "tf: waiting" << std::endl;
            sd->cond.wait(&(sd->mutex));
            std::cout << "tf: waited" << std::endl;
        }
    }

    sd->mutex.unlock();

    std::cout << "tf: /threadfunc" << std::endl;

    return NULL;
}

static int
BPPAllocate(void ** instance, unsigned int, const BPElement * context)
{
    SessionData * sd = new SessionData;

    // extract the temporary directory
    bp::Object * args = bp::Object::build(context);
    sd->tempDir = (std::string) (*(args->get("temp_dir")));
    delete args;

    (void) ft::mkdir(sd->tempDir);
    g_bpCoreFunctions->log(BP_INFO, "session allocated, using temp dir: %s",
                           (sd->tempDir.empty() ? "<empty>"
                                                : sd->tempDir.c_str()));

    sd->running = true;
    (void) sd->thread.run(threadFunc, (void *) sd);
    *instance = (void *) sd;
    return 0;
}

static void
BPPDestroy(void * instance)
{
    assert(instance != NULL);
    SessionData * sd = (SessionData *) instance;

    // kick the (de)compression thread
    sd->mutex.lock();

    // mark the first task as canceled to kick the compression thread out
    // of it's work cycle
    if (sd->taskList.size() > 0) {
        sd->taskList.begin()->canceled = true;
    }
    
    sd->running = false;
    sd->cond.signal();
    sd->mutex.unlock();

    // wait for him to shut down
    sd->thread.join();

    delete sd;
}

static void
BPPShutdown(void)
{
}

static void
BPPInvoke(void * instance, const char * funcName,
          unsigned int tid, const BPElement * elem)
{
    assert(instance != NULL);
    SessionData * sd = (SessionData *) instance;

    Task::Type t;
    
    if (!strcmp(funcName, "compress")) {
        t = Task::T_Compress;
    } else if (!strcmp(funcName, "decompress")) {        
        t = Task::T_Decompress;
    } else {
        g_bpCoreFunctions->log(BP_ERROR, "invalid function invoked!");
    }
    

    bp::Object * args = NULL;
    if (elem) args = bp::Object::build(elem);

    // all functions have a "file" argument, validate we can parse it
    std::string url = (*(args->get("file")));
    std::string path = bp::urlutil::pathFromURL(url);

    if (path.empty())
    {
        g_bpCoreFunctions->log(
            BP_ERROR, "can't parse file:// url: %s", url.c_str());

        g_bpCoreFunctions->postError(
            tid, "bp.fileAccessError", "invalid file URI");

        if (args) delete args;
        return;
    } 

    // generate the output path
    std::string outPath = ft::getPath(sd->tempDir, path);
    if (outPath.empty())
    {
        g_bpCoreFunctions->postError(tid, "bp.internalError",
                                     "can't generate output path");
        if (args) delete args;
        return;
    } 

    // Allocate the Task structure
    Task task(t);
    task.tid = tid;
    task.inPath = path;
    task.outPath = outPath;
    
    // append callback if available
    if (args->has("progressCB", BPTCallBack)) {
        task.cid = (long long) *(args->get("progressCB"));
    }
    
    // add task to work queue and wakeup compression thread
    sd->mutex.lock();
    sd->taskList.push_back(task);
    sd->cond.signal();
    sd->mutex.unlock();
    
    if (args) delete args;

    return;
}


BPArgumentDefinition s_compressFuncArgs[] = {
    {
        "file",
        "The file that you would like to compress.",
        BPTPath,
        BP_TRUE
    },
    {
        "progressCB",
        "A function that will recieve progress callbacks as the compression proceeds.",
        BPTCallBack,
        BP_FALSE
    }
};

BPArgumentDefinition s_decompressFuncArgs[] = {
    {
        "file",
        "The file that you would like to decompress.",
        BPTPath,
        BP_TRUE
    }
};


static BPFunctionDefinition s_functions[] = {
    {
        "compress",
        "LZMA Compress a file.",
        sizeof(s_compressFuncArgs)/sizeof(s_compressFuncArgs[0]),
        s_compressFuncArgs
    },
    {
        "decompress",
        "Decompress a file compressed with LZMA.",
        sizeof(s_decompressFuncArgs)/sizeof(s_decompressFuncArgs[0]),
        s_decompressFuncArgs
    }
};

const BPCoreletDefinition *
BPPInitialize(const BPCFunctionTable * bpCoreFunctions,
              const BPElement * parameterMap)
{
    // a description of this service
    static BPCoreletDefinition s_serviceDef = {
        "LZMA",
        0, 0, 2,
        "Perform LZMA (de)compression.",
        sizeof(s_functions)/sizeof(s_functions[0]),
        s_functions
    };

    g_bpCoreFunctions = bpCoreFunctions;
    return &s_serviceDef;
}

/** and finally, declare the entry point to the corelet */
BPPFunctionTable funcTable = {
    BPP_CORELET_API_VERSION,
    BPPInitialize,
    BPPShutdown,
    BPPAllocate,
    BPPDestroy,
    BPPInvoke,
    NULL,
    NULL
};

const BPPFunctionTable *
BPPGetEntryPoints(void)
{
    return &funcTable;
}
