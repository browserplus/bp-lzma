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
 * Portions created by Yahoo! are Copyright (C) 2006-2008 Yahoo!.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 * ***** END LICENSE BLOCK ***** */

/**
 * bpurlutil.hh -- a portable c++ utility to perform conversion
 *                 between file URLs and native paths.
 */

#ifndef __BPURLUTIL_H__
#define __BPURLUTIL_H__

#include <string>

namespace bp {
namespace urlutil {
    std::string pathFromURL(const std::string& url);
    std::string urlFromPath(const std::string& s);
}}

#endif
