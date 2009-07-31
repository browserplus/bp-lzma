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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>


// 2mb is max allowable read
#define FA_MAX_READ (1<<21)

const BPCFunctionTable * g_bpCoreFunctions = NULL;

static int
BPPAllocate(void ** instance, unsigned int, const BPElement *)
{
    *instance = NULL;
    return 0;
}

static void
BPPDestroy(void * instance)
{
    assert(instance == NULL);
}

static void
BPPShutdown(void)
{
}

static void
BPPInvoke(void * instance, const char * funcName,
          unsigned int tid, const BPElement * elem)
{
    bp::Object * args = NULL;
    if (elem) args = bp::Object::build(elem);

    // all functions have a "file" argument, validate we can parse it

    std::string path;
    
    if (!strcmp(funcName, "Compress"))
    {
/* now we need to extract the file argument and covert to native path, then perform
   compression!  work here! 
        bp::url::Url pathUrl;
        if (!pathUrl.parse((std::string) (*(args->get("file")))))
        {
            g_bpCoreFunctions->postError(tid, "bp.fileAccessError",
                                         "invalid file URI");
            if (args) delete args;
            return;
        } 

        // XXX: implement me!
        g_bpCoreFunctions->postError(tid, "bp.notImplemented", NULL);

        // now we'll need to convert into a native path :/ and perform the compression
        //     std::string path = bp::url::pathFromURL(pathUrl.toString());
        // XXX: write me
*/
    }
    else
    {
        g_bpCoreFunctions->postError(tid, "bp.internalError", "No such function!");
    }

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


static BPFunctionDefinition s_functions[] = {
    {
        "Compress",
        "LZMA Compress a file.",
        sizeof(s_compressFuncArgs)/sizeof(s_compressFuncArgs[0]),
        s_compressFuncArgs
    }
};

const BPCoreletDefinition *
BPPInitialize(const BPCFunctionTable * bpCoreFunctions,
              const BPElement * parameterMap)
{
    // a description of this service
    static BPCoreletDefinition s_serviceDef = {
        "LZMA",
        0, 0, 1,
        "Perform LZMA compression.",
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
