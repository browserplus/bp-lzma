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

#include "bpservice/bpservice.h"
#include "bpservice/bpcallback.h"
#include "bputil/bpthread.h"
#include "bputil/bpsync.h"
#include "bp-file/bpfile.h"
#include "easylzma/compress.h"  

// When unarchive() is exposed, remove uses of this define.
// It is currently unexposed because we need to consider
// all of the security aspects.
#undef DECOMPRESS_EXPOSED

// Data structures used to manage tasks.  We only allow one task
// to run per session to put a limit on per-session resource consumption.
// In some cases this could prevent us from leveraging multiple logical
// cores, but this is arguably better than allowing any random site to
// 0wn your proc.
struct Task {
    typedef enum {
        T_Compress,
        T_Decompress
    } Type;
    Task(Type t, const bplus::service::Transaction& tran, const bplus::CallBack* cb) :
        m_type(t),
        m_tran(tran),
        m_cb(NULL),
        m_canceled(false) {
        if (cb != NULL) {
            m_cb = new bplus::service::Callback(m_tran, *cb);
        }
    }
    ~Task() {
        if (m_inFile.is_open()) {
            m_inFile.close();
        }
        if (m_outFile.is_open()) {
            m_outFile.close();
        }
        if (m_cb != NULL) {
            delete m_cb;
            m_cb = NULL;
        }
    }
    Type m_type;
    bplus::service::Transaction m_tran;
    bplus::service::Callback* m_cb;
    bool m_canceled;
    boost::filesystem::path m_inPath;
    std::ifstream m_inFile;
    boost::filesystem::path m_outPath;    
    std::ofstream m_outFile;
};

static int  
inputCallback(void* ctx, void* buf, size_t* size) {
    Task* t = (Task*)ctx;
    t->m_inFile.read((char*)buf, *size);
    size_t rd = t->m_inFile.gcount();
    std::stringstream ss;
    ss << "read " << rd << " bytes from input file";
    bplus::service::Service::log(BP_DEBUG, ss.str());
    *size = 0;
    if (rd == 0 && !t->m_inFile.good()) {
        return -1;
    }
    *size = rd;
    return 0;
}
  
static size_t  
outputCallback(void *ctx, const void *buf, size_t size) {
    std::stringstream ss;
    ss << "Writing " << size << " bytes to output file";
    bplus::service::Service::log(BP_DEBUG, ss.str());
    Task* t = (Task*)ctx;
    t->m_outFile.write((char*)buf, size);
    return t->m_outFile.bad() ? 0 : size;
}

static void
progressCallback(void* ctx, size_t complete, size_t total) {
    Task* t = (Task*)ctx;    
    if (t->m_cb) {
        t->m_cb->invoke(bplus::Double(((double)complete / (double)total) * 100.0));
    }
}

static void
performTask(Task* t) {
    assert(t != NULL);
    // open the input file
    if (!bp::file::openReadableStream(t->m_inFile, t->m_inPath, std::ios_base::in | std::ios_base::binary)) {
        t->m_tran.error("bp.fileAccessError", "Couldn't open file for reading");
        return;
    }
    if (!bp::file::openWritableStream(t->m_outFile, t->m_outPath, std::ios_base::out | std::ios_base::binary)) {
        t->m_tran.error("bp.fileAccessError", "Couldn't open file for writing");
        return;
    }
    t->m_inFile.seekg(0, std::ios::end);
    size_t sz = t->m_inFile.tellg();
    t->m_inFile.seekg(0, std::ios::beg);
    if (sz < 0) {
        t->m_tran.error("bp.fileAccessError", "Couldn't ftell input file");
        return;
    }
    /* use the elzma library to determine a good dictionary size for near
     * optimal compression and minimal runtime memory cost */
    unsigned int dictSz = elzma_get_dict_size(sz);
    /* allocate compression handle */  
    elzma_compress_handle hand;
    hand = elzma_compress_alloc();
    /* configure the compression run with mostly default parameters  */   
    int rc = elzma_compress_config(hand, ELZMA_LC_DEFAULT, ELZMA_LP_DEFAULT, ELZMA_PB_DEFAULT, 5, dictSz, ELZMA_lzip, sz);
    /* fail if we couldn't allocate */    
    if (rc != ELZMA_E_OK) {
        elzma_compress_free(&hand);
        t->m_tran.error("bp.internalError", "Error allocating compression engine");
        return;
    }
    /* now run the compression */  
    {
        /* run the streaming compression */   
        rc = elzma_compress_run(hand, inputCallback, (void*)t, outputCallback, (void*)t, progressCallback, (void*)t);
        if (rc != ELZMA_E_OK) {
            elzma_compress_free(&hand);
            t->m_tran.error("bp.compressionError", "Error encountered during compression");
            return;
        }
    }
    t->m_outFile.close();
    t->m_tran.complete(bplus::Path(bp::file::nativeString(t->m_outPath)));
    elzma_compress_free(&hand);
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
    boost::filesystem::path tempDir;
};

static void*
threadFunc(void* pv) {
    SessionData* sd = (SessionData*)pv;
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

class LZMA : public bplus::service::Service {
public:
BP_SERVICE(LZMA)
    LZMA();
    ~LZMA();
    virtual void finalConstruct();
    void compress(const bplus::service::Transaction& tran, const bplus::Map& args);
#if DECOMPRESS_EXPOSED
    void decompress(const bplus::service::Transaction& tran, const bplus::Map& args);
#endif // DECOMPRESS_EXPOSED
private:
    void lzmaImpl(Task::Type t, const bplus::service::Transaction& tran, const bplus::Map& args);
private:
    SessionData* m_sd;
};

BP_SERVICE_DESC(LZMA, "LZMA", "1.1.0",
                "Perform LZMA (de)compression.")
ADD_BP_METHOD(LZMA, compress,
             "LZMA Compress a file.")
ADD_BP_METHOD_ARG(compress, "file", Path, true,
                  "The file that you would like to compress.")
ADD_BP_METHOD_ARG(compress, "progressCB", CallBack, false,
                  "A function that will recieve progress callbacks as the compression proceeds.")
#if DECOMPRESS_EXPOSED
ADD_BP_METHOD(LZMA, decompress,
              "Decompress a file compressed with LZMA.")
ADD_BP_METHOD_ARG(decompress, "file", Path, true,
                  "The file that you would like to decompress.")
#endif // DECOMPRESS_EXPOSED
END_BP_SERVICE_DESC

LZMA::LZMA() :
    m_sd(NULL) {
}

LZMA::~LZMA() {
    if (m_sd != NULL) {
        // kick the (de)compression thread
        m_sd->mutex.lock();
        // mark the first task as canceled to kick the compression thread out
        // of it's work cycle
        if (m_sd->taskList.size() > 0) {
            (*(m_sd->taskList.begin()))->m_canceled = true;
        }
        m_sd->running = false;
        m_sd->cond.signal();
        m_sd->mutex.unlock();
        // wait for him to shut down
        m_sd->thread.join();
        delete m_sd;
        m_sd = NULL;
    }
}

void
LZMA::finalConstruct() {
    assert(m_sd == NULL);
    m_sd = new SessionData;
    m_sd->tempDir = boost::filesystem::path(tempDir());
    boost::filesystem::create_directory(m_sd->tempDir);
    std::stringstream ss;
    ss << "session allocated, using temp dir: " << (m_sd->tempDir.empty() ? "<empty>" : m_sd->tempDir.string());
    bplus::service::Service::log(BP_INFO, ss.str());
    m_sd->running = true;
    (void)m_sd->thread.run(threadFunc, (void*)m_sd);
}
void
LZMA::compress(const bplus::service::Transaction& tran, const bplus::Map& args) {
    lzmaImpl(Task::T_Compress, tran, args);
}

#if DECOMPRESS_EXPOSED
void
LZMA::decompress(const bplus::service::Transaction& tran, const bplus::Map& args) {
    lzmaImpl(Task::T_Decompress, tran, args);
}
#endif // DECOMPRESS_EXPOSED

void
LZMA::lzmaImpl(Task::Type t, const bplus::service::Transaction& tran, const bplus::Map& args) {
    std::stringstream ss;
    assert(m_sd != NULL);
    // first we'll get the input file into a string
    const bplus::Path* bpPath = dynamic_cast<const bplus::Path*>(args.value("file"));
    boost::filesystem::path path((bplus::tPathString)*bpPath);
    if (path.empty()) {
        tran.error("bp.fileAccessError", "invalid file parameter");
        return;
    }
    // generate the output path
    boost::filesystem::path outPath = bp::file::getTempPath(m_sd->tempDir, bp::file::nativeUtf8String(path));
    // append "lz"
    outPath.replace_extension(".lz");
    if (outPath.string().empty()) {
        tran.error("bp.internalError", "can't generate output path");
        return;
    } 
    // append callback if available
    const bplus::CallBack* cb = NULL;
    if (args.has("progressCB")) {
        cb = dynamic_cast<const bplus::CallBack*>(args.value("progressCB"));
    }
    // Allocate the Task structure
    Task* task = new Task(t, tran, cb);
    task->m_inPath = path;
    task->m_outPath = outPath.string();
    ss.str("");
    ss << "LZMA compressing '" << task->m_inPath << "' to '" << task->m_outPath << "'";
    bplus::service::Service::log(BP_INFO, ss.str());
    // add task to work queue and wakeup compression thread
    m_sd->mutex.lock();
    m_sd->taskList.push_back(task);
    m_sd->cond.signal();
    m_sd->mutex.unlock();
}

