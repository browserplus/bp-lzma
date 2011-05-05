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

#include "bpservice/bpservice.h"
#include "bputil/bpthread.h"
#include "bputil/bpsync.h"
#include "bp-file/bpfile.h"

#include "bpurlutil.cpp"

#include "easylzma/compress.h"  

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
    Task(Type t) : type(t), tid(0), cid(0),
                   canceled(false) { }
    ~Task() 
    {
        if (inFile.is_open()) {
            inFile.close();
        }
        if (outFile.is_open()) {
            inFile.close();
        }
    }
    
    Type type;
    std::string inPath;
    std::ifstream inFile;
    std::string outPath;    
    std::ofstream outFile;
    unsigned int tid; 
    long long cid;
    bool canceled;
};

/* an input callback that will be passed to elzma_compress_run(), 
 * it reads from an opened file */  
static int  
inputCallback(void *ctx, void *buf, size_t * size)  
{
    Task * t = (Task *) ctx;
    t->inFile.read((char*)buf, *size);
    size_t rd = t->inFile.gcount();
    g_bpCoreFunctions->log(BP_DEBUG, "read %lu bytes from input file",
                           rd);
    *size = 0;
    if (rd == 0 && !t->inFile.good()) return -1;
    *size = rd;
    return 0;
}
  
static size_t  
outputCallback(void *ctx, const void *buf, size_t size)  
{
    Task * t = (Task *) ctx;
    g_bpCoreFunctions->log(BP_DEBUG, "Writing %lu bytes to output file",
                           size);
  
    t->outFile.write((char*)buf, size);
    return t->outFile.bad() ? 0 : size;
}

static
void progressCallback(void * ctx, size_t complete, size_t total)
{
    Task * t = (Task *) ctx;    
    printf("proggy (%lu) %d/%d\n", t->cid, complete, total);
    if (t->cid) {
        double pct = (double) complete / (double) total;
        g_bpCoreFunctions->invoke(t->tid, t->cid,
                                  bplus::Double(100.0 * pct).elemPtr());
    }
}

static
void performTask(Task * t)
{
    assert(t != NULL);
  
    // open the input file
    if (!bp::file::openReadableStream(t->inFile, t->inPath, std::ios_base::in | std::ios_base::binary)) {
        g_bpCoreFunctions->postError(
            t->tid, "bp.fileAccessError", "Couldn't open file for reading");
        return;
    }
    if (!bp::file::openWritableStream(t->outFile, t->outPath, std::ios_base::out | std::ios_base::binary)) {
        g_bpCoreFunctions->postError(
            t->tid, "bp.fileAccessError", "Couldn't open file for writing");
        return;
    }

    t->inFile.seekg(0, std::ios::end);
    size_t sz = t->inFile.tellg();
    t->inFile.seekg(0, std::ios::beg);
    if (sz < 0) {
        g_bpCoreFunctions->postError(
            t->tid, "bp.fileAccessError", "Couldn't ftell input file");
        return;
    }

    /* use the elzma library to determine a good dictionary size for near
     * optimal compression and minimal runtime memory cost */
    unsigned int dictSz = elzma_get_dict_size(sz);

    /* allocate compression handle */  
    elzma_compress_handle hand;
    hand = elzma_compress_alloc();
      
    /* configure the compression run with mostly default parameters  */   
    int rc = elzma_compress_config(hand, ELZMA_LC_DEFAULT,  
                                   ELZMA_LP_DEFAULT, ELZMA_PB_DEFAULT,  
                                   5, dictSz, ELZMA_lzip, sz);
      
    /* fail if we couldn't allocate */    
    if (rc != ELZMA_E_OK) {
        elzma_compress_free(&hand);
        g_bpCoreFunctions->postError(
            t->tid, "bp.internalError", "Error allocating compression engine");
        return;
    }
   
    /* now run the compression */  
    {
        /* run the streaming compression */   
        rc = elzma_compress_run(hand, inputCallback, (void *) t,  
                                outputCallback, (void *) t,
                                progressCallback, (void *) t);

        if (rc != ELZMA_E_OK) {
            elzma_compress_free(&hand);
            g_bpCoreFunctions->postError(
                t->tid, "bp.compressionError",
                "Error encountered during compression");
            return;
        }
    }

    t->outFile.close();

    g_bpCoreFunctions->postResults(t->tid, bplus::Path(t->outPath).elemPtr());

    elzma_compress_free(&hand);
      
    return;
}

// per-session (aka, per-site) data includes a list of tasks to perform,
// a thread to run tasks (the thread is not for performance, but rather
// to support asynchronous cancelation in the middle of a (de)compression),
// and a simple mutex
struct SessionData {
    std::list<Task*> taskList;
    bplus::sync::Mutex mutex;
    bplus::sync::Condition cond;
    bplus::thread::Thread thread;
    bool running;
    std::string tempDir;
};

static
void * threadFunc(void * sdp) 
{
    SessionData * sd = (SessionData *) sdp;

    sd->mutex.lock();

    while (sd->running) {
        if (sd->taskList.size() > 0) {
            sd->mutex.unlock();            

            // perform the (de)compression task *outside* of our lock,
            // in the case of async cancelation, the bool 'canceled'
            // will be toggled
            Task* task = *(sd->taskList.begin());
            performTask(task);

            sd->mutex.lock();

            // the task was either canceled or completed.  in either case
            // we can delete it *inside* the lock
            sd->taskList.erase(sd->taskList.begin());
            delete task;

        } else {
            sd->cond.wait(&(sd->mutex));
        }
    }

    sd->mutex.unlock();

    return NULL;
}

static int
BPPAllocate(void ** instance, unsigned int, const BPElement * context)
{
    SessionData * sd = new SessionData;

    // extract the temporary directory
    bplus::Object * args = bplus::Object::build(context);
    sd->tempDir = (std::string) (*(args->get("temp_dir")));
    delete args;

    boost::filesystem::create_directories(sd->tempDir);
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
        (*(sd->taskList.begin()))->canceled = true;
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
    

    bplus::Object * args = NULL;
    if (elem) args = bplus::Object::build(elem);

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
    boost::filesystem::path  outPath = bp::file::getTempPath(sd->tempDir, path);
    // append "lz"
    outPath.replace_extension(".lz");
    
    if (outPath.string().empty())
    {
        g_bpCoreFunctions->postError(tid, "bp.internalError",
                                     "can't generate output path");
        if (args) delete args;
        return;
    } 

    // Allocate the Task structure
    Task* task = new Task(t);
    task->tid = tid;
    task->inPath = path;
    task->outPath = outPath.string();
    
    // append callback if available
    if (args->has("progressCB", BPTCallBack)) {
        task->cid = (long long) *(args->get("progressCB"));
    }

    g_bpCoreFunctions->log(BP_INFO, "LZMA compressing '%s' to '%s'",
                           task->inPath.c_str(),
                           task->outPath.c_str());
    
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
    }
#if 0
    // version 1.0.x will not include support for decompression,
    // we need to add some other features in the BrowserPlus platform
    // to allow the user to securely select a writable location
    ,{
        "decompress",
        "Decompress a file compressed with LZMA.",
        sizeof(s_decompressFuncArgs)/sizeof(s_decompressFuncArgs[0]),
        s_decompressFuncArgs
    }
#endif
};

const BPCoreletDefinition *
BPPInitialize(const BPCFunctionTable * bpCoreFunctions,
              const BPElement * parameterMap)
{
    // a description of this service
    static BPCoreletDefinition s_serviceDef = {
        "LZMA",
        1, 0, 3,
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
