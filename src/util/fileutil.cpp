/**
 * ***** BEGIN LICENSE BLOCK *****
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is BrowserPlus (tm).
 * 
 * The Initial Developer of the Original Code is Yahoo!.
 * Portions created by Yahoo! are Copyright (C) 2006-2009 Yahoo!.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 * ***** END LICENSE BLOCK *****
 */

#include "fileutil.hh"
#include "bpsync.hh"

#ifdef WIN32
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <errno.h>
#define PATH_SEP '/'
#endif

#include <sstream>

#if 0
static
std::string dirName(const std::string & path)
{
    // /foo/bar -> /foo
    // /foo/    -> /foo
    // /foo/bar/ -> /foo/bar 

    std::string dirname;
    size_t pos = path.rfind(PATH_SEP);
    if (pos != std::string::npos) {
        dirname = path.substr(0, pos);
    }

    return dirname;
}
#endif

static
std::string fileName(const std::string & path)
{
    // /foo/bar -> bar
    // /foo/    -> 
    // /foo/bar/ -> 

    std::string filename;
    size_t pos = path.rfind(PATH_SEP);
    if (pos != std::string::npos) {
        filename = path.substr(pos+1);
    }

    return filename;
}

static 
std::string pathAppend(const std::string & parent, const std::string & child)
{
    std::string rv = parent;
    if (!parent.empty() && parent[parent.size()-1] != PATH_SEP) {
        rv += PATH_SEP;
    }
    return (rv + child);
}

std::string
ft::getPath(std::string tempDir, std::string sourcePath)
{
    if (!mkdir(tempDir, false)) return std::string();

    // generate a unique name within this directory
    {
        static unsigned int id = 0;
        static bp::sync::Mutex m;
        bp::sync::Lock l(m);
        std::stringstream ss;
        ss << id++;
        tempDir = pathAppend(tempDir, ss.str());        
    }
    if (!mkdir(tempDir, true)) return std::string();
    // append the filename of sourcePath
    tempDir = pathAppend(tempDir,fileName(sourcePath));
    
    return tempDir;
}

#ifdef WIN32
#include <windows.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

std::wstring
utf8ToWide(const std::string& sIn)
{
    std::wstring rval;
    // See how much space we need.
    int nChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sIn.c_str(), 
                                     -1, 0, 0);

    // Do the conversion.
    wchar_t* pawBuf = new wchar_t[nChars];
    int nRtn = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sIn.c_str(), 
                                   -1, pawBuf, nChars);
    if (nRtn==0) {
        delete[] pawBuf;
    } else {
        rval = pawBuf;
        delete[] pawBuf;
    }
    return rval;
}
#endif

bool
ft::isRegularFile(std::string path)
{
    if (path.empty()) return false;
#ifdef WIN32
    struct _stat s;
    memset((void *) &s, 0, sizeof(s));
    std::wstring wpath = utf8ToWide(path);
    if (!_wstat(wpath.c_str(), &s)) {
        return (s.st_mode & _S_IFREG);        
    }
#else
    struct stat s;
    memset((void *) &s, 0, sizeof(s));
    if (!stat(path.c_str(), &s)) {
        return ((s.st_mode & S_IFMT) == S_IFREG);
    }
#endif
    return false;
}

bool
ft::mkdir(std::string path, bool failIfExists)
{
    if (path.empty()) return false;
#ifdef WIN32    
    if (0 == ::_wmkdir(utf8ToWide(path).c_str())) return true;
#else
    if (0 == ::mkdir(path.c_str(), 0700)) return true;
#endif
    if (!failIfExists && errno == EEXIST) return true;
    return false;
}

FILE *
ft::fopen_binary_read(std::string utf8Path)
{
    if (utf8Path.empty()) return false;
#ifdef WIN32    
    return _wfopen(utf8ToWide(utf8Path).c_str(), L"rb");
#else
    return fopen(utf8Path.c_str(), "r");
#endif
}

FILE *
ft::fopen_binary_write(std::string utf8Path)
{
    if (utf8Path.empty()) return false;
#ifdef WIN32    
    return _wfopen(utf8ToWide(utf8Path).c_str(), L"wb");
#else
    return fopen(utf8Path.c_str(), "w");
#endif
}
