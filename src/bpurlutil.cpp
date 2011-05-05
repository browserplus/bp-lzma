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

/** 
 * bpurlutil.cpp - some utilities to facilitate dealing with urls
 */

#include "bpurlutil.hh"

#include <string>
#include <vector>

#include <string.h>

#define FILE_URL_PREFIX "file://"

#ifdef WIN32
#define PATH_SEPARATOR "\\"
#else 
#define PATH_SEPARATOR "/"
#endif

static std::vector<std::string> 
xxsplit(const std::string& str, 
      const std::string& delim)
{
    std::vector<std::string> vsRet;
    
    unsigned int offset = 0;
    unsigned long delimIndex = 0;
    delimIndex = str.find(delim, offset);
    while (delimIndex != std::string::npos) {
        vsRet.push_back(str.substr(offset, delimIndex - offset));
        offset += delimIndex - offset + delim.length();
        delimIndex = str.find(delim, offset);
    }
    vsRet.push_back(str.substr(offset));

    return vsRet;
}

static std::string 
xxurlEncode(const std::string& s)
{
    std::string out;
    
    char hex[4];

    static const char noencode[] = "!'()*-._";
    static const char hexvals[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
        'A', 'B', 'C', 'D', 'E', 'F'
    };
    
    for (unsigned int i = 0; i < s.length(); i++) {
        if (isalnum(s[i]) || strchr(noencode, s[i]) != NULL) {
            out.append(&s[i], 1);
        } else {
            hex[0] = '%';
            hex[1] = hexvals[(s[i] >> 4) & 0x0F];
            hex[2] = hexvals[s[i] & 0xF];
            hex[3] = 0;
            out.append(hex, strlen(hex));
        }
    }
    return out;
}

static char xxchar2hex(char c) {
  if (isdigit(c)) c -= '0';
  else c = (c | 0x20) - 'a' + 10;
  return c;
}

static std::string
xxurlDecode(std::string s)
{
    unsigned int i = 0;
    unsigned char hv;

    for (i=0; i < s.size(); i++) {
        if (s.at(i) == '%') {
            if (isxdigit(s.at(i+1)) && isxdigit(s.at(i+2))) {
                hv = xxchar2hex(s.at(i+1)) << 4;
                hv = hv + (unsigned char) xxchar2hex(s.at(i+2));
                s.replace(i, 3, (const char *) &hv, 1);
            }
        } else if (s.at(i) == '+') {
            hv = ' ';
            s.replace(i, 1, (const char *) &hv, 1);
        }
    }
    return s;
}

std::string 
bp::urlutil::urlFromPath(const std::string& s)
{
    if (s.empty()) return std::string();

    // is this already a url?
    if (s.find(FILE_URL_PREFIX) == 0) {
        return s;
    }
    
    std::string delim(PATH_SEPARATOR);
    std::vector<std::string> edges = xxsplit(s, delim);
    
    std::string rval(FILE_URL_PREFIX);

    unsigned int start = 0;
    if (edges[0].empty()) {
        start++;
    }
#ifdef WIN32
    // Windows hosts appear differently in pathname (//host/path)
    // and must appear in final url as file://host/path.  Also
    // want drive names, which appear as C:/foo, to appear in
    // url as file:///C:/foo
    if (edges[start].length() == 0) {
        // have a host, e.g. //host/path
        rval.append(edges[++start]);
        start++;
    } else if (edges[start].length() == 2 && edges[start][1] == ':') {
        // have a drive, e.g. c:/foo
        rval.append("/" + edges[start]);
        start++;
    }
#endif

    // add remaining edges
    for (unsigned int i = start; i < edges.size(); i++) {
        if (edges[i].length() > 0) {
            rval.append("/");
            rval.append(xxurlEncode(edges[i]));
        }
    }
    return rval;
}


std::string
bp::urlutil::pathFromURL(const std::string& url)
{
    if (url.empty()) {
        return "";
    }

    std::string rval;
    std::string delim(PATH_SEPARATOR);

    // file url format is file://host/path

    // check for file://
    if (url.find(FILE_URL_PREFIX) != 0) {
        return rval;
    }

    // Rip off file:// and get remaining edges.
    // Note that if "s" started with "/", 
    // first edge will be empty.  Also, Windows 
    // handles hosts with //host/path.  No uniform
    // way to do hosts other than localhost on other
    // platforms, so bail.
    //
    std::string s = url.substr(strlen(FILE_URL_PREFIX));
    if (s.empty()) {
        return rval;
    }
    std::vector<std::string> edges = xxsplit(s, "/");
    unsigned int start = 0;
    if (edges[0].empty()) {
        start++;
    }

    std::string firstEdge = xxurlDecode(edges[start]);
    if (s[0] == '/') {
#ifdef WIN32
        // no host, check for drive
        if (firstEdge.length() == 2 && firstEdge[1] == ':') {
            rval = firstEdge;
        } else {
            rval = delim + firstEdge;
        }
#else
        rval = delim + firstEdge;
#endif
        start++;
    } else {
        // got a host.  everybody skips localhost, doze groks //host/path
        if (firstEdge.compare("localhost") == 0) {
            rval = delim;
        } else {
#ifdef WIN32
            rval = "\\" + firstEdge;
#else
            return rval;
#endif
        }
        start++;
    }

    // add remaining edges
    for (size_t i = start; i < edges.size(); i++) {
        rval += delim + xxurlDecode(edges[i]);
    }

    return rval;
}
