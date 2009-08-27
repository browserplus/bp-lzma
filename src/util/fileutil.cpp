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

bool
ft::isRegularFile(std::string path)
{
    if (path.empty()) return false;
    struct stat s;
    memset((void *) &s, 0, sizeof(s));
    if (!stat(path.c_str(), &s)) {
        return ((s.st_mode & S_IFMT) == S_IFREG);
    }
    return false;
}

bool
ft::mkdir(std::string path, bool failIfExists)
{
    if (path.empty()) return false;
    if (0 == ::mkdir(path.c_str(), 0700)) return true;
    if (!failIfExists && errno == EEXIST) return true;
    return false;
}
