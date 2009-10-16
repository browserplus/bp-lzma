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

/*
 *  fileutil.hh - a small pile of file xplat manipulation routines    
 *
 *  Created by Lloyd Hilaiel on 08/25/09.
 */

#ifndef __FILETOOLS_HH__
#define __FILETOOLS_HH__

#include <string>

namespace ft {
    // generate a path within the specified tempDir with a leave named
    // the same as the source
    // returns empty on failure
    std::string getPath(std::string tempDir, std::string sourcePath);

    // check if that's a regular ol' file.  like one we could compress.
    bool isRegularFile(std::string path);

    // create a directory with user only perms
    bool mkdir(std::string path, bool failIfExists = true);

    FILE * fopen_binary_read(std::string utf8Path);
    FILE * fopen_binary_write(std::string utf8Path);
};

#endif
