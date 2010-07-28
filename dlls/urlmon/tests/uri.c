/*
 * UrlMon IUri tests
 *
 * Copyright 2010 Thomas Mullaly
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * IUri testing framework goals:
 *  - Test invalid args
 *      - invalid flags
 *      - invalid args (IUri, uri string)
 *  - Test parsing for components when no canonicalization occurs
 *  - Test parsing for components when canonicalization occurs.
 *  - More tests...
 */

#include <wine/test.h>
#include <stdarg.h>
#include <stddef.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "urlmon.h"
#include "shlwapi.h"

#define URI_STR_PROPERTY_COUNT Uri_PROPERTY_STRING_LAST+1
#define URI_DWORD_PROPERTY_COUNT (Uri_PROPERTY_DWORD_LAST - Uri_PROPERTY_DWORD_START)+1

static HRESULT (WINAPI *pCreateUri)(LPCWSTR, DWORD, DWORD_PTR, IUri**);

static const WCHAR http_urlW[] = { 'h','t','t','p',':','/','/','w','w','w','.','w','i','n','e','h','q',
        '.','o','r','g','/',0};

typedef struct _uri_create_flag_test {
    DWORD   flags;
    HRESULT expected;
} uri_create_flag_test;

static const uri_create_flag_test invalid_flag_tests[] = {
    /* Set of invalid flag combinations to test for. */
    {Uri_CREATE_DECODE_EXTRA_INFO | Uri_CREATE_NO_DECODE_EXTRA_INFO, E_INVALIDARG},
    {Uri_CREATE_CANONICALIZE | Uri_CREATE_NO_CANONICALIZE, E_INVALIDARG},
    {Uri_CREATE_CRACK_UNKNOWN_SCHEMES | Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES, E_INVALIDARG},
    {Uri_CREATE_PRE_PROCESS_HTML_URI | Uri_CREATE_NO_PRE_PROCESS_HTML_URI, E_INVALIDARG},
    {Uri_CREATE_IE_SETTINGS | Uri_CREATE_NO_IE_SETTINGS, E_INVALIDARG}
};

typedef struct _uri_str_property {
    const char* value;
    HRESULT     expected;
    BOOL        todo;
} uri_str_property;

typedef struct _uri_dword_property {
    DWORD   value;
    HRESULT expected;
    BOOL    todo;
} uri_dword_property;

typedef struct _uri_properties {
    const char*         uri;
    DWORD               create_flags;
    HRESULT             create_expected;
    BOOL                create_todo;
    DWORD               props;
    BOOL                props_todo;

    uri_str_property    str_props[URI_STR_PROPERTY_COUNT];
    uri_dword_property  dword_props[URI_DWORD_PROPERTY_COUNT];
} uri_properties;

static const uri_properties uri_tests[] = {
    {   "http://www.winehq.org/tests/../tests/../..", 0, S_OK, FALSE,
        /* A flag bitmap containing all the Uri_HAS_* flags that correspond to this uri. */
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|
        Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://www.winehq.org/",S_OK,TRUE},                       /* ABSOLUTE_URI */
            {"www.winehq.org",S_OK,TRUE},                               /* AUTHORITY */
            {"http://www.winehq.org/",S_OK,TRUE},                       /* DISPLAY_URI */
            {"winehq.org",S_OK,TRUE},                                   /* DOMAIN */
            {"",S_FALSE,TRUE},                                          /* EXTENSION */
            {"",S_FALSE,TRUE},                                          /* FRAGMENT */
            {"www.winehq.org",S_OK,FALSE},                              /* HOST */
            {"",S_FALSE,FALSE},                                         /* PASSWORD */
            {"/",S_OK,TRUE},                                            /* PATH */
            {"/",S_OK,TRUE},                                            /* PATH_AND_QUERY */
            {"",S_FALSE,TRUE},                                          /* QUERY */
            {"http://www.winehq.org/tests/../tests/../..",S_OK,FALSE},  /* RAW_URI */
            {"http",S_OK,FALSE},                                        /* SCHEME_NAME */
            {"",S_FALSE,FALSE},                                         /* USER_INFO */
            {"",S_FALSE,FALSE}                                          /* USER_NAME */
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},                                   /* HOST_TYPE */
            {80,S_OK,TRUE},                                             /* PORT */
            {URL_SCHEME_HTTP,S_OK,FALSE},                               /* SCHEME */
            {URLZONE_INVALID,E_NOTIMPL,FALSE}                           /* ZONE */
        }
    },
    {   "http://winehq.org/tests/.././tests", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|
        Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://winehq.org/tests",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"http://winehq.org/tests",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"winehq.org",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/tests",S_OK,TRUE},
            {"/tests",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://winehq.org/tests/.././tests",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "HtTp://www.winehq.org/tests/..?query=x&return=y", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|
        Uri_HAS_DOMAIN|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://www.winehq.org/?query=x&return=y",S_OK,TRUE},
            {"www.winehq.org",S_OK,TRUE},
            {"http://www.winehq.org/?query=x&return=y",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.winehq.org",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/?query=x&return=y",S_OK,TRUE},
            {"?query=x&return=y",S_OK,TRUE},
            {"HtTp://www.winehq.org/tests/..?query=x&return=y",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE},
        }
    },
    {   "hTTp://us%45r%3Ainfo@examp%4CE.com:80/path/a/b/./c/../%2E%2E/Forbidden'<|> Characters", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://usEr%3Ainfo@example.com/path/a/Forbidden'%3C%7C%3E%20Characters",S_OK,TRUE},
            {"usEr%3Ainfo@example.com",S_OK,TRUE},
            {"http://example.com/path/a/Forbidden'%3C%7C%3E%20Characters",S_OK,TRUE},
            {"example.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"example.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/path/a/Forbidden'%3C%7C%3E%20Characters",S_OK,TRUE},
            {"/path/a/Forbidden'%3C%7C%3E%20Characters",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"hTTp://us%45r%3Ainfo@examp%4CE.com:80/path/a/b/./c/../%2E%2E/Forbidden'<|> Characters",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"usEr%3Ainfo",S_OK,FALSE},
            {"usEr%3Ainfo",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE},
        }
    },
    {   "ftp://winepass:wine@ftp.winehq.org:9999/dir/foo bar.txt", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_EXTENSION|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://winepass:wine@ftp.winehq.org:9999/dir/foo%20bar.txt",S_OK,TRUE},
            {"winepass:wine@ftp.winehq.org:9999",S_OK,TRUE},
            {"ftp://ftp.winehq.org:9999/dir/foo%20bar.txt",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {".txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp.winehq.org",S_OK,FALSE},
            {"wine",S_OK,FALSE},
            {"/dir/foo%20bar.txt",S_OK,TRUE},
            {"/dir/foo%20bar.txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://winepass:wine@ftp.winehq.org:9999/dir/foo bar.txt",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"winepass:wine",S_OK,FALSE},
            {"winepass",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {9999,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "file://c:\\tests\\../tests/foo%20bar.mp3", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"file:///c:/tests/foo%2520bar.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"file:///c:/tests/foo%2520bar.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"/c:/tests/foo%2520bar.mp3",S_OK,TRUE},
            {"/c:/tests/foo%2520bar.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"file://c:\\tests\\../tests/foo%20bar.mp3",S_OK,FALSE},
            {"file",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_FILE,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "FILE://localhost/test dir\\../tests/test%20file.README.txt", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"file:///tests/test%20file.README.txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"file:///tests/test%20file.README.txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"/tests/test%20file.README.txt",S_OK,TRUE},
            {"/tests/test%20file.README.txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"FILE://localhost/test dir\\../tests/test%20file.README.txt",S_OK,FALSE},
            {"file",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_FILE,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "urn:nothing:should:happen here", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_DISPLAY_URI|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|
        Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"urn:nothing:should:happen here",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"urn:nothing:should:happen here",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"nothing:should:happen here",S_OK,TRUE},
            {"nothing:should:happen here",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"urn:nothing:should:happen here",S_OK,FALSE},
            {"urn",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://127.0.0.1/tests/../test dir/./test.txt", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://127.0.0.1/test%20dir/test.txt",S_OK,TRUE},
            {"127.0.0.1",S_OK,TRUE},
            {"http://127.0.0.1/test%20dir/test.txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"127.0.0.1",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/test%20dir/test.txt",S_OK,TRUE},
            {"/test%20dir/test.txt",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://127.0.0.1/tests/../test dir/./test.txt",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[fedc:ba98:7654:3210:fedc:ba98:7654:3210]/",S_OK,TRUE},
            {"[fedc:ba98:7654:3210:fedc:ba98:7654:3210]",S_OK,TRUE},
            {"http://[fedc:ba98:7654:3210:fedc:ba98:7654:3210]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"fedc:ba98:7654:3210:fedc:ba98:7654:3210",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "ftp://[::13.1.68.3]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://[::13.1.68.3]/",S_OK,TRUE},
            {"[::13.1.68.3]",S_OK,TRUE},
            {"ftp://[::13.1.68.3]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"::13.1.68.3",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://[::13.1.68.3]",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://[FEDC:BA98:0:0:0:0:0:3210]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[fedc:ba98::3210]/",S_OK,TRUE},
            {"[fedc:ba98::3210]",S_OK,TRUE},
            {"http://[fedc:ba98::3210]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"fedc:ba98::3210",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[FEDC:BA98:0:0:0:0:0:3210]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "1234://www.winehq.org", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"1234://www.winehq.org/",S_OK,TRUE},
            {"www.winehq.org",S_OK,TRUE},
            {"1234://www.winehq.org/",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.winehq.org",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"1234://www.winehq.org",S_OK,FALSE},
            {"1234",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "://www.winehq.org", 0, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Window's doesn't like URI's which are implicitly file paths without the ALLOW_IMPLICIT_FILE_SCHEME flag set. */
    {   "C:/test/test.mp3", 0, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Window's doesn't like URI's which are implicitly file paths without the ALLOW_IMPLICIT_FILE_SCHEME flag set. */
    {   "\\\\Server/test/test.mp3", 0, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Window's doesn't like URI's which are implicitly file paths without the ALLOW_IMPLICIT_FILE_SCHEME flag set. */
    {   "\\\\Server/test/test.mp3", Uri_CREATE_ALLOW_RELATIVE, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Window's doesn't like URI's which are implicitly file paths without the ALLOW_IMPLICIT_FILE_SCHEME flag set. */
    {   "C:/test/test.mp3", Uri_CREATE_ALLOW_IMPLICIT_WILDCARD_SCHEME, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Test's to make sure the parser/canonicalizer handles implicit file schemes correctly. */
    {   "C:/test/test.mp3", Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"file:///C:/test/test.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"file:///C:/test/test.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"/C:/test/test.mp3",S_OK,TRUE},
            {"/C:/test/test.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"C:/test/test.mp3",S_OK,FALSE},
            {"file",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_FILE,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Test's to make sure the parser/canonicalizer handles implicit file schemes correctly. */
    {   "\\\\Server/test.mp3", Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_HOST|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"file://server/test.mp3",S_OK,TRUE},
            {"server",S_OK,TRUE},
            {"file://server/test.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"server",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/test.mp3",S_OK,TRUE},
            {"/test.mp3",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"\\\\Server/test.mp3",S_OK,FALSE},
            {"file",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_FILE,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "www.winehq.org/test", Uri_CREATE_ALLOW_IMPLICIT_WILDCARD_SCHEME, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"*:www.winehq.org/test",S_OK,TRUE},
            {"www.winehq.org",S_OK,TRUE},
            {"*:www.winehq.org/test",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.winehq.org",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/test",S_OK,TRUE},
            {"/test",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"www.winehq.org/test",S_OK,FALSE},
            {"*",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_WILDCARD,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Valid since the '*' is the only character in the scheme name. */
    {   "*:www.winehq.org/test", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"*:www.winehq.org/test",S_OK,TRUE},
            {"www.winehq.org",S_OK,TRUE},
            {"*:www.winehq.org/test",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.winehq.org",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/test",S_OK,TRUE},
            {"/test",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"*:www.winehq.org/test",S_OK,FALSE},
            {"*",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_WILDCARD,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not valid since a '*' is only allowed to appear if its the only character in scheme. */
    {   "*abcd://not.valid.com", 0, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not valid since a '*' is only allowed to appear if its the only character in scheme. */
    {   "*a*b*c*d://not.valid.com", 0, E_INVALIDARG, FALSE,
        0, TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "/../some dir/test.ext", Uri_CREATE_ALLOW_RELATIVE, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"/../some dir/test.ext",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"/../some dir/test.ext",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".ext",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"/../some dir/test.ext",S_OK,TRUE},
            {"/../some dir/test.ext",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"/../some dir/test.ext",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "//implicit/wildcard/uri scheme", Uri_CREATE_ALLOW_RELATIVE|Uri_CREATE_ALLOW_IMPLICIT_WILDCARD_SCHEME, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"*://implicit/wildcard/uri%20scheme",S_OK,TRUE},
            {"",S_OK,TRUE},
            {"*://implicit/wildcard/uri%20scheme",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"//implicit/wildcard/uri%20scheme",S_OK,TRUE},
            {"//implicit/wildcard/uri%20scheme",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"//implicit/wildcard/uri scheme",S_OK,FALSE},
            {"*",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_WILDCARD,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* URI is considered opaque since CREATE_NO_CRACK_UNKNOWN_SCHEMES is set and its an unknown scheme. */
    {   "zip://google.com", Uri_CREATE_NO_CRACK_UNKNOWN_SCHEMES, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_DISPLAY_URI|Uri_HAS_EXTENSION|Uri_HAS_PATH|
        Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME_NAME|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip:/.//google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip:/.//google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {".com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"/.//google.com",S_OK,TRUE},
            {"/.//google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://google.com",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Windows uses the first occurence of ':' to delimit the userinfo. */
    {   "ftp://user:pass:word@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://user:pass:word@winehq.org/",S_OK,TRUE},
            {"user:pass:word@winehq.org",S_OK,TRUE},
            {"ftp://winehq.org/",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"winehq.org",S_OK,FALSE},
            {"pass:word",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://user:pass:word@winehq.org/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"user:pass:word",S_OK,FALSE},
            {"user",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Make sure % encoded unreserved characters are decoded. */
    {   "ftp://w%49%4Ee:PA%53%53@ftp.google.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://wINe:PASS@ftp.google.com/",S_OK,TRUE},
            {"wINe:PASS@ftp.google.com",S_OK,TRUE},
            {"ftp://ftp.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp.google.com",S_OK,FALSE},
            {"PASS",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://w%49%4Ee:PA%53%53@ftp.google.com/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"wINe:PASS",S_OK,FALSE},
            {"wINe",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Make sure % encoded characters which are NOT unreserved are NOT decoded. */
    {   "ftp://w%5D%5Be:PA%7B%7D@ftp.google.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://w%5D%5Be:PA%7B%7D@ftp.google.com/",S_OK,TRUE},
            {"w%5D%5Be:PA%7B%7D@ftp.google.com",S_OK,TRUE},
            {"ftp://ftp.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp.google.com",S_OK,FALSE},
            {"PA%7B%7D",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://w%5D%5Be:PA%7B%7D@ftp.google.com/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"w%5D%5Be:PA%7B%7D",S_OK,FALSE},
            {"w%5D%5Be",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* You're allowed to have an empty password portion in the userinfo section. */
    {   "ftp://empty:@ftp.google.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://empty:@ftp.google.com/",S_OK,TRUE},
            {"empty:@ftp.google.com",S_OK,TRUE},
            {"ftp://ftp.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp.google.com",S_OK,FALSE},
            {"",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://empty:@ftp.google.com/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"empty:",S_OK,FALSE},
            {"empty",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Make sure forbidden characters in "userinfo" get encoded. */
    {   "ftp://\" \"weird@ftp.google.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://%22%20%22weird@ftp.google.com/",S_OK,TRUE},
            {"%22%20%22weird@ftp.google.com",S_OK,TRUE},
            {"ftp://ftp.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp.google.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://\" \"weird@ftp.google.com/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"%22%20%22weird",S_OK,FALSE},
            {"%22%20%22weird",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Make sure the forbidden characters don't get percent encoded. */
    {   "ftp://\" \"weird@ftp.google.com/", Uri_CREATE_NO_ENCODE_FORBIDDEN_CHARACTERS, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://\" \"weird@ftp.google.com/",S_OK,TRUE},
            {"\" \"weird@ftp.google.com",S_OK,TRUE},
            {"ftp://ftp.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp.google.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://\" \"weird@ftp.google.com/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"\" \"weird",S_OK,FALSE},
            {"\" \"weird",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Allowed to have invalid % encoded because its an unknown scheme type. */
    {   "zip://%xy:word@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip://%xy:word@winehq.org/",S_OK,TRUE},
            {"%xy:word@winehq.org",S_OK,TRUE},
            {"zip://%xy:word@winehq.org/",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"winehq.org",S_OK,FALSE},
            {"word",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://%xy:word@winehq.org/",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"%xy:word",S_OK,FALSE},
            {"%xy",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Unreserved, percent encoded characters aren't decoded in the userinfo becuase the scheme
     * isn't known.
     */
    {   "zip://%2E:%52%53ord@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip://%2E:%52%53ord@winehq.org/",S_OK,TRUE},
            {"%2E:%52%53ord@winehq.org",S_OK,TRUE},
            {"zip://%2E:%52%53ord@winehq.org/",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"winehq.org",S_OK,FALSE},
            {"%52%53ord",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://%2E:%52%53ord@winehq.org/",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"%2E:%52%53ord",S_OK,FALSE},
            {"%2E",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "ftp://[](),'test':word@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|
        Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://[](),'test':word@winehq.org/",S_OK,TRUE},
            {"[](),'test':word@winehq.org",S_OK,TRUE},
            {"ftp://winehq.org/",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"winehq.org",S_OK,FALSE},
            {"word",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://[](),'test':word@winehq.org/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"[](),'test':word",S_OK,FALSE},
            {"[](),'test'",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "ftp://test?:word@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://test/?:word@winehq.org/",S_OK,TRUE},
            {"test",S_OK,TRUE},
            {"ftp://test/?:word@winehq.org/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"test",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/?:word@winehq.org/",S_OK,TRUE},
            {"?:word@winehq.org/",S_OK,TRUE},
            {"ftp://test?:word@winehq.org/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "ftp://test#:word@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_FRAGMENT|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://test/#:word@winehq.org/",S_OK,TRUE},
            {"test",S_OK,TRUE},
            {"ftp://test/#:word@winehq.org/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"#:word@winehq.org/",S_OK,TRUE},
            {"test",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://test#:word@winehq.org/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Allowed to have a backslash in the userinfo since it's an unknown scheme. */
    {   "zip://test\\:word@winehq.org/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|Uri_HAS_HOST|
        Uri_HAS_PASSWORD|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_USER_INFO|Uri_HAS_USER_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip://test\\:word@winehq.org/",S_OK,TRUE},
            {"test\\:word@winehq.org",S_OK,TRUE},
            {"zip://test\\:word@winehq.org/",S_OK,TRUE},
            {"winehq.org",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"winehq.org",S_OK,FALSE},
            {"word",S_OK,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://test\\:word@winehq.org/",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"test\\:word",S_OK,FALSE},
            {"test\\",S_OK,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* It normalizes IPv4 addresses correctly. */
    {   "http://127.000.000.100/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://127.0.0.100/",S_OK,TRUE},
            {"127.0.0.100",S_OK,TRUE},
            {"http://127.0.0.100/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"127.0.0.100",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://127.000.000.100/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Make sure it normalizes partial IPv4 addresses correctly. */
    {   "http://127.0/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://127.0.0.0/",S_OK,TRUE},
            {"127.0.0.0",S_OK,TRUE},
            {"http://127.0.0.0/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"127.0.0.0",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://127.0/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Make sure it converts implicit IPv4's correctly. */
    {   "http://123456/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://0.1.226.64/",S_OK,TRUE},
            {"0.1.226.64",S_OK,TRUE},
            {"http://0.1.226.64/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"0.1.226.64",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://123456/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* UINT_MAX */
    {   "http://4294967295/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://255.255.255.255/",S_OK,TRUE},
            {"255.255.255.255",S_OK,TRUE},
            {"http://255.255.255.255/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"255.255.255.255",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://4294967295/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* UINT_MAX+1 */
    {   "http://4294967296/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://4294967296/",S_OK,TRUE},
            {"4294967296",S_OK,TRUE},
            {"http://4294967296/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"4294967296",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://4294967296/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Window's doesn't normalize IP address for unknown schemes. */
    {   "1234://4294967295/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"1234://4294967295/",S_OK,TRUE},
            {"4294967295",S_OK,TRUE},
            {"1234://4294967295/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"4294967295",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"1234://4294967295/",S_OK,FALSE},
            {"1234",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Window's doesn't normalize IP address for unknown schemes. */
    {   "1234://127.001/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"1234://127.001/",S_OK,TRUE},
            {"127.001",S_OK,TRUE},
            {"1234://127.001/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"127.001",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"1234://127.001/",S_OK,FALSE},
            {"1234",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_IPV4,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://[FEDC:BA98::3210]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[fedc:ba98::3210]/",S_OK,TRUE},
            {"[fedc:ba98::3210]",S_OK,TRUE},
            {"http://[fedc:ba98::3210]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"fedc:ba98::3210",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[FEDC:BA98::3210]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://[::]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[::]/",S_OK,TRUE},
            {"[::]",S_OK,TRUE},
            {"http://[::]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"::",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[::]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://[FEDC:BA98::]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[fedc:ba98::]/",S_OK,TRUE},
            {"[fedc:ba98::]",S_OK,TRUE},
            {"http://[fedc:ba98::]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"fedc:ba98::",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[FEDC:BA98::]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Too many h16 components. */
    {   "http://[1:2:3:4:5:6:7:8:9]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not enough room for IPv4 address. */
    {   "http://[1:2:3:4:5:6:7:192.0.1.0]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not enough h16 component */
    {   "http://[1:2:3:4]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not enough components including IPv4 address. */
    {   "http://[1:192.0.1.0]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not valid to have a partial IPv4 address in a IPv6 address. */
    {   "http://[::192.0]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not valid because a elision of 2 bytes appears at the beginning of address. */
    {   "http://[::2:3:4:5:6:7:8]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not valid because a elision of 2 bytes appears at the end of address. */
    {   "http://[1:2:3:4:5:6:7::]", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Valid even with 2 byte elision because it doesn't appear the beginning or end. */
    {   "http://[1::3:4:5:6:7:8]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[1:0:3:4:5:6:7:8]/",S_OK,TRUE},
            {"[1:0:3:4:5:6:7:8]",S_OK,TRUE},
            {"http://[1:0:3:4:5:6:7:8]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"1:0:3:4:5:6:7:8",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[1::3:4:5:6:7:8]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    {   "http://[v2.34]/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[v2.34]/",S_OK,TRUE},
            {"[v2.34]",S_OK,TRUE},
            {"http://[v2.34]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"[v2.34]",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[v2.34]/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Windows ignores ':' if they appear after a '[' on a non-IPLiteral host. */
    {   "http://[xyz:12345.com/test", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[xyz:12345.com/test",S_OK,TRUE},
            {"[xyz:12345.com",S_OK,TRUE},
            {"http://[xyz:12345.com/test",S_OK,TRUE},
            {"[xyz:12345.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"[xyz:12345.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/test",S_OK,TRUE},
            {"/test",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[xyz:12345.com/test",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Invalid URI since the host begins with '[' and ends with ']' but doesn't have
     * a valid IP literal inside.
     */
    {   "ftp://[not.valid.uri]/", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Valid URI since the '[' and ']' don't appear at the begining and end
     * of the host name (respectively).
     */
    {   "ftp://www.[works].com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"ftp://www.[works].com/",S_OK,TRUE},
            {"www.[works].com",S_OK,TRUE},
            {"ftp://www.[works].com/",S_OK,TRUE},
            {"[works].com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.[works].com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"ftp://www.[works].com/",S_OK,FALSE},
            {"ftp",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {21,S_OK,TRUE},
            {URL_SCHEME_FTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Considers ':' a delimiter since it appears after the ']'. */
    {   "http://www.google.com]:12345/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://www.google.com]:12345/",S_OK,TRUE},
            {"www.google.com]:12345",S_OK,TRUE},
            {"http://www.google.com]:12345/",S_OK,TRUE},
            {"google.com]",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.google.com]",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://www.google.com]:12345/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {12345,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Unknown scheme types can have invalid % encoded data in the hostname. */
    {   "zip://w%XXw%GEw.google.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip://w%XXw%GEw.google.com/",S_OK,TRUE},
            {"w%XXw%GEw.google.com",S_OK,TRUE},
            {"zip://w%XXw%GEw.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"w%XXw%GEw.google.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://w%XXw%GEw.google.com/",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Not allowed to have invalid % encoded data. */
    {   "ftp://google.co%XX/", 0, E_INVALIDARG, FALSE,
        0,
        TRUE,
        {
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_UNKNOWN,S_FALSE,FALSE},
            {0,S_FALSE,FALSE},
            {URL_SCHEME_INVALID,S_FALSE,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Unknown scheme types hostname doesn't get lower cased. */
    {   "zip://GOOGLE.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip://GOOGLE.com/",S_OK,TRUE},
            {"GOOGLE.com",S_OK,TRUE},
            {"zip://GOOGLE.com/",S_OK,TRUE},
            {"GOOGLE.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"GOOGLE.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://GOOGLE.com/",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Hostname get's lower cased for known scheme types. */
    {   "http://WWW.GOOGLE.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://www.google.com/",S_OK,TRUE},
            {"www.google.com",S_OK,TRUE},
            {"http://www.google.com/",S_OK,TRUE},
            {"google.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.google.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://WWW.GOOGLE.com/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Characters that get % encoded in the hostname also have their percent
     * encoded forms lower cased.
     */
    {   "http://www.%7Cgoogle|.com/", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_DOMAIN|
        Uri_HAS_HOST|Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|
        Uri_HAS_SCHEME_NAME|Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://www.%7cgoogle%7c.com/",S_OK,TRUE},
            {"www.%7cgoogle%7c.com",S_OK,TRUE},
            {"http://www.%7cgoogle%7c.com/",S_OK,TRUE},
            {"%7cgoogle%7c.com",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"www.%7cgoogle%7c.com",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://www.%7Cgoogle|.com/",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE}
        },
        {
            {Uri_HOST_DNS,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* IPv4 addresses attached to IPv6 can be included in elisions. */
    {   "http://[1:2:3:4:5:6:0.0.0.0]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[1:2:3:4:5:6::]/",S_OK,TRUE},
            {"[1:2:3:4:5:6::]",S_OK,TRUE},
            {"http://[1:2:3:4:5:6::]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"1:2:3:4:5:6::",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[1:2:3:4:5:6:0.0.0.0]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* IPv4 addresses get normalized. */
    {   "http://[::001.002.003.000]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[::1.2.3.0]/",S_OK,TRUE},
            {"[::1.2.3.0]",S_OK,TRUE},
            {"http://[::1.2.3.0]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"::1.2.3.0",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[::001.002.003.000]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* Windows doesn't do anything to IPv6's in unknown schemes. */
    {   "zip://[0001:0:000:0004:0005:0006:001.002.003.000]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_SCHEME,
        TRUE,
        {
            {"zip://[0001:0:000:0004:0005:0006:001.002.003.000]/",S_OK,TRUE},
            {"[0001:0:000:0004:0005:0006:001.002.003.000]",S_OK,TRUE},
            {"zip://[0001:0:000:0004:0005:0006:001.002.003.000]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"0001:0:000:0004:0005:0006:001.002.003.000",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"zip://[0001:0:000:0004:0005:0006:001.002.003.000]",S_OK,FALSE},
            {"zip",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {0,S_FALSE,TRUE},
            {URL_SCHEME_UNKNOWN,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    },
    /* IPv4 address is converted into 2 h16 components. */
    {   "http://[ffff::192.222.111.32]", 0, S_OK, FALSE,
        Uri_HAS_ABSOLUTE_URI|Uri_HAS_AUTHORITY|Uri_HAS_DISPLAY_URI|Uri_HAS_HOST|
        Uri_HAS_PATH|Uri_HAS_PATH_AND_QUERY|Uri_HAS_RAW_URI|Uri_HAS_SCHEME_NAME|
        Uri_HAS_HOST_TYPE|Uri_HAS_PORT|Uri_HAS_SCHEME,
        TRUE,
        {
            {"http://[ffff::c0de:6f20]/",S_OK,TRUE},
            {"[ffff::c0de:6f20]",S_OK,TRUE},
            {"http://[ffff::c0de:6f20]/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"",S_FALSE,TRUE},
            {"ffff::c0de:6f20",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"/",S_OK,TRUE},
            {"/",S_OK,TRUE},
            {"",S_FALSE,TRUE},
            {"http://[ffff::192.222.111.32]",S_OK,FALSE},
            {"http",S_OK,FALSE},
            {"",S_FALSE,FALSE},
            {"",S_FALSE,FALSE},
        },
        {
            {Uri_HOST_IPV6,S_OK,TRUE},
            {80,S_OK,TRUE},
            {URL_SCHEME_HTTP,S_OK,FALSE},
            {URLZONE_INVALID,E_NOTIMPL,FALSE}
        }
    }
};

typedef struct _uri_equality {
    const char* a;
    DWORD       create_flags_a;
    BOOL        create_todo_a;
    const char* b;
    DWORD       create_flags_b;
    BOOL        create_todo_b;
    BOOL        equal;
    BOOL        todo;
} uri_equality;

static const uri_equality equality_tests[] = {
    {
        "HTTP://www.winehq.org/test dir/./",0,FALSE,
        "http://www.winehq.org/test dir/../test dir/",0,FALSE,
        TRUE, TRUE
    },
    {
        /* http://www.winehq.org/test%20dir */
        "http://%77%77%77%2E%77%69%6E%65%68%71%2E%6F%72%67/%74%65%73%74%20%64%69%72",0,FALSE,
        "http://www.winehq.org/test dir",0,FALSE,
        TRUE, TRUE,
    },
    {
        "c:\\test.mp3",Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME,FALSE,
        "file:///c:/test.mp3",0,FALSE,
        TRUE,TRUE
    },
    {
        "ftp://ftp.winehq.org/",0,FALSE,
        "ftp://ftp.winehq.org",0,FALSE,
        TRUE, TRUE
    },
    {
        "ftp://ftp.winehq.org/test/test2/../../testB/",0,FALSE,
        "ftp://ftp.winehq.org/t%45stB/",0,FALSE,
        FALSE, TRUE
    }
};

static inline LPWSTR a2w(LPCSTR str) {
    LPWSTR ret = NULL;

    if(str) {
        DWORD len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        ret = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    }

    return ret;
}

static inline BOOL heap_free(void* mem) {
    return HeapFree(GetProcessHeap(), 0, mem);
}

static inline DWORD strcmp_aw(LPCSTR strA, LPCWSTR strB) {
    LPWSTR strAW = a2w(strA);
    DWORD ret = lstrcmpW(strAW, strB);
    heap_free(strAW);
    return ret;
}

/*
 * Simple tests to make sure the CreateUri function handles invalid flag combinations
 * correctly.
 */
static void test_CreateUri_InvalidFlags(void) {
    DWORD i;

    for(i = 0; i < sizeof(invalid_flag_tests)/sizeof(invalid_flag_tests[0]); ++i) {
        HRESULT hr;
        IUri *uri = (void*) 0xdeadbeef;

        hr = pCreateUri(http_urlW, invalid_flag_tests[i].flags, 0, &uri);
        todo_wine {
            ok(hr == invalid_flag_tests[i].expected, "Error: CreateUri returned 0x%08x, expected 0x%08x, flags=0x%08x\n",
                    hr, invalid_flag_tests[i].expected, invalid_flag_tests[i].flags);
        }
        todo_wine { ok(uri == NULL, "Error: expected the IUri to be NULL, but it was %p instead\n", uri); }
    }
}

static void test_CreateUri_InvalidArgs(void) {
    HRESULT hr;
    IUri *uri = (void*) 0xdeadbeef;

    const WCHAR invalidW[] = {'i','n','v','a','l','i','d',0};

    hr = pCreateUri(http_urlW, 0, 0, NULL);
    ok(hr == E_INVALIDARG, "Error: CreateUri returned 0x%08x, expected 0x%08x\n", hr, E_INVALIDARG);

    hr = pCreateUri(NULL, 0, 0, &uri);
    ok(hr == E_INVALIDARG, "Error: CreateUri returned 0x%08x, expected 0x%08x\n", hr, E_INVALIDARG);
    ok(uri == NULL, "Error: Expected the IUri to be NULL, but it was %p instead\n", uri);

    uri = (void*) 0xdeadbeef;
    hr = pCreateUri(invalidW, 0, 0, &uri);
    ok(hr == E_INVALIDARG, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);
    ok(uri == NULL, "Error: Expected the IUri to be NULL, but it was %p instead\n", uri);
}

static void test_IUri_GetPropertyBSTR(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    /* Make sure GetPropertyBSTR handles invalid args correctly. */
    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        BSTR received = NULL;

        hr = IUri_GetPropertyBSTR(uri, Uri_PROPERTY_RAW_URI, NULL, 0);
        ok(hr == E_POINTER, "Error: GetPropertyBSTR returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        /* Make sure it handles a invalid Uri_PROPERTY's correctly. */
        hr = IUri_GetPropertyBSTR(uri, Uri_PROPERTY_PORT, &received, 0);
        ok(hr == S_OK, "Error: GetPropertyBSTR returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
        ok(received != NULL, "Error: Expected the string not to be NULL.\n");
        ok(!SysStringLen(received), "Error: Expected the string to be of len=0 but it was %d instead.\n", SysStringLen(received));
        SysFreeString(received);

        /* Make sure it handles the ZONE property correctly. */
        received = NULL;
        hr = IUri_GetPropertyBSTR(uri, Uri_PROPERTY_ZONE, &received, 0);
        ok(hr == S_FALSE, "Error: GetPropertyBSTR returned 0x%08x, expected 0x%08x.\n", hr, S_FALSE);
        ok(received != NULL, "Error: Expected the string not to be NULL.\n");
        ok(!SysStringLen(received), "Error: Expected the string to be of len=0 but it was %d instead.\n", SysStringLen(received));
        SysFreeString(received);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);
        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x. Failed on uri_tests[%d].\n",
                        hr, test.create_expected, i);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x. Failed on uri_tests[%d].\n",
                    hr, test.create_expected, i);
        }

        if(SUCCEEDED(hr)) {
            DWORD j;

            /* Checks all the string properties of the uri. */
            for(j = Uri_PROPERTY_STRING_START; j <= Uri_PROPERTY_STRING_LAST; ++j) {
                BSTR received = NULL;
                uri_str_property prop = test.str_props[j];

                hr = IUri_GetPropertyBSTR(uri, j, &received, 0);
                if(prop.todo) {
                    todo_wine {
                        ok(hr == prop.expected, "GetPropertyBSTR returned 0x%08x, expected 0x%08x. On uri_tests[%d].str_props[%d].\n",
                                hr, prop.expected, i, j);
                    }
                    todo_wine {
                        ok(!strcmp_aw(prop.value, received), "Expected %s but got %s on uri_tests[%d].str_props[%d].\n",
                                prop.value, wine_dbgstr_w(received), i, j);
                    }
                    if (hr == E_NOTIMPL)  /* no point in continuing */
                    {
                        if(uri) IUri_Release(uri);
                        heap_free(uriW);
                        return;
                    }
                } else {
                    ok(hr == prop.expected, "GetPropertyBSTR returned 0x%08x, expected 0x%08x. On uri_tests[%d].str_props[%d].\n",
                            hr, prop.expected, i, j);
                    ok(!strcmp_aw(prop.value, received), "Expected %s but got %s on uri_tests[%d].str_props[%d].\n",
                            prop.value, wine_dbgstr_w(received), i, j);
                }

                SysFreeString(received);
            }
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

static void test_IUri_GetPropertyDWORD(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        DWORD received = 0xdeadbeef;

        hr = IUri_GetPropertyDWORD(uri, Uri_PROPERTY_DWORD_START, NULL, 0);
        ok(hr == E_INVALIDARG, "Error: GetPropertyDWORD returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);

        hr = IUri_GetPropertyDWORD(uri, Uri_PROPERTY_ABSOLUTE_URI, &received, 0);
        ok(hr == E_INVALIDARG, "Error: GetPropertyDWORD returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);
        ok(received == 0, "Error: Expected received=%d but instead received=%d.\n", 0, received);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);
        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x. Failed on uri_tests[%d].\n",
                        hr, test.create_expected, i);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x. Failed on uri_tests[%d].\n",
                    hr, test.create_expected, i);
        }

        if(SUCCEEDED(hr)) {
            DWORD j;

            /* Checks all the DWORD properties of the uri. */
            for(j = 0; j < sizeof(test.dword_props)/sizeof(test.dword_props[0]); ++j) {
                DWORD received;
                uri_dword_property prop = test.dword_props[j];

                hr = IUri_GetPropertyDWORD(uri, j+Uri_PROPERTY_DWORD_START, &received, 0);
                if(prop.todo) {
                    todo_wine {
                        ok(hr == prop.expected, "GetPropertyDWORD returned 0x%08x, expected 0x%08x. On uri_tests[%d].dword_props[%d].\n",
                                hr, prop.expected, i, j);
                    }
                    todo_wine {
                        ok(prop.value == received, "Expected %d but got %d on uri_tests[%d].dword_props[%d].\n",
                                prop.value, received, i, j);
                    }
                    if (hr == E_NOTIMPL)  /* no point in continuing */
                    {
                        if(uri) IUri_Release(uri);
                        heap_free(uriW);
                        return;
                    }
                } else {
                    ok(hr == prop.expected, "GetPropertyDWORD returned 0x%08x, expected 0x%08x. On uri_tests[%d].dword_props[%d].\n",
                            hr, prop.expected, i, j);
                    ok(prop.value == received, "Expected %d but got %d on uri_tests[%d].dword_props[%d].\n",
                            prop.value, received, i, j);
                }
            }
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

/* Tests all the 'Get*' property functions which deal with strings. */
static void test_IUri_GetStrProperties(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    /* Make sure all the 'Get*' string property functions handle invalid args correctly. */
    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        hr = IUri_GetAbsoluteUri(uri, NULL);
        ok(hr == E_POINTER, "Error: GetAbsoluteUri returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetAuthority(uri, NULL);
        ok(hr == E_POINTER, "Error: GetAuthority returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetDisplayUri(uri, NULL);
        ok(hr == E_POINTER, "Error: GetDisplayUri returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetDomain(uri, NULL);
        ok(hr == E_POINTER, "Error: GetDomain returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetExtension(uri, NULL);
        ok(hr == E_POINTER, "Error: GetExtension returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetFragment(uri, NULL);
        ok(hr == E_POINTER, "Error: GetFragment returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetHost(uri, NULL);
        ok(hr == E_POINTER, "Error: GetHost returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetPassword(uri, NULL);
        ok(hr == E_POINTER, "Error: GetPassword returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetPath(uri, NULL);
        ok(hr == E_POINTER, "Error: GetPath returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetPathAndQuery(uri, NULL);
        ok(hr == E_POINTER, "Error: GetPathAndQuery returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetQuery(uri, NULL);
        ok(hr == E_POINTER, "Error: GetQuery returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetRawUri(uri, NULL);
        ok(hr == E_POINTER, "Error: GetRawUri returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetSchemeName(uri, NULL);
        ok(hr == E_POINTER, "Error: GetSchemeName returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetUserInfo(uri, NULL);
        ok(hr == E_POINTER, "Error: GetUserInfo returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);

        hr = IUri_GetUserName(uri, NULL);
        ok(hr == E_POINTER, "Error: GetUserName returned 0x%08x, expected 0x%08x.\n", hr, E_POINTER);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);
        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, test.create_expected, i);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                    hr, test.create_expected, i);
        }

        if(SUCCEEDED(hr)) {
            uri_str_property prop;
            BSTR received = NULL;

            /* GetAbsoluteUri() tests. */
            prop = test.str_props[Uri_PROPERTY_ABSOLUTE_URI];
            hr = IUri_GetAbsoluteUri(uri, &received);
            if (hr == E_NOTIMPL) goto skip_unimplemented;
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetAbsoluteUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetAbsoluteUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetAuthority() tests. */
            prop = test.str_props[Uri_PROPERTY_AUTHORITY];
            hr = IUri_GetAuthority(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetAuthority returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetAuthority returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetDisplayUri() tests. */
            prop = test.str_props[Uri_PROPERTY_DISPLAY_URI];
            hr = IUri_GetDisplayUri(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetDisplayUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_test[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetDisplayUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetDomain() tests. */
            prop = test.str_props[Uri_PROPERTY_DOMAIN];
            hr = IUri_GetDomain(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetDomain returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetDomain returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetExtension() tests. */
            prop = test.str_props[Uri_PROPERTY_EXTENSION];
            hr = IUri_GetExtension(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetExtension returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetExtension returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetFragment() tests. */
            prop = test.str_props[Uri_PROPERTY_FRAGMENT];
            hr = IUri_GetFragment(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetFragment returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetFragment returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetPassword() tests. */
            prop = test.str_props[Uri_PROPERTY_PASSWORD];
            hr = IUri_GetPassword(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetPassword returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetPassword returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

        skip_unimplemented:
            /* GetHost() tests. */
            prop = test.str_props[Uri_PROPERTY_HOST];
            hr = IUri_GetHost(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetHost returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetHost returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetPath() tests. */
            prop = test.str_props[Uri_PROPERTY_PATH];
            hr = IUri_GetPath(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetPath returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetPath returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetPathAndQuery() tests. */
            prop = test.str_props[Uri_PROPERTY_PATH_AND_QUERY];
            hr = IUri_GetPathAndQuery(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetPathAndQuery returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetPathAndQuery returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetQuery() tests. */
            prop = test.str_props[Uri_PROPERTY_QUERY];
            hr = IUri_GetQuery(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetQuery returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetQuery returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetRawUri() tests. */
            prop = test.str_props[Uri_PROPERTY_RAW_URI];
            hr = IUri_GetRawUri(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetRawUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetRawUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetSchemeName() tests. */
            prop = test.str_props[Uri_PROPERTY_SCHEME_NAME];
            hr = IUri_GetSchemeName(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetSchemeName returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetSchemeName returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetUserInfo() tests. */
            prop = test.str_props[Uri_PROPERTY_USER_INFO];
            hr = IUri_GetUserInfo(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetUserInfo returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetUserInfo returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
            received = NULL;

            /* GetUserName() tests. */
            prop = test.str_props[Uri_PROPERTY_USER_NAME];
            hr = IUri_GetUserName(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetUserName returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                            prop.value, wine_dbgstr_w(received), i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetUserName returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(!strcmp_aw(prop.value, received), "Error: Expected %s but got %s on uri_tests[%d].\n",
                        prop.value, wine_dbgstr_w(received), i);
            }
            SysFreeString(received);
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

static void test_IUri_GetDwordProperties(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    /* Make sure all the 'Get*' dword property functions handle invalid args correctly. */
    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        hr = IUri_GetHostType(uri, NULL);
        ok(hr == E_INVALIDARG, "Error: GetHostType returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);

        hr = IUri_GetPort(uri, NULL);
        ok(hr == E_INVALIDARG, "Error: GetPort returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);

        hr = IUri_GetScheme(uri, NULL);
        ok(hr == E_INVALIDARG, "Error: GetScheme returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);

        hr = IUri_GetZone(uri, NULL);
        ok(hr == E_INVALIDARG, "Error: GetZone returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);
        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, test.create_expected, i);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                    hr, test.create_expected, i);
        }

        if(SUCCEEDED(hr)) {
            uri_dword_property prop;
            DWORD received;

            /* Assign an insane value so tests don't accidentally pass when
             * they shouldn't!
             */
            received = -9999999;

            /* GetHostType() tests. */
            prop = test.dword_props[Uri_PROPERTY_HOST_TYPE-Uri_PROPERTY_DWORD_START];
            hr = IUri_GetHostType(uri, &received);
            if (hr == E_NOTIMPL) goto skip_unimplemented;
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetHostType returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetHostType returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
            }
            received = -9999999;

            /* GetPort() tests. */
            prop = test.dword_props[Uri_PROPERTY_PORT-Uri_PROPERTY_DWORD_START];
            hr = IUri_GetPort(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetPort returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetPort returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
            }
            received = -9999999;

        skip_unimplemented:
            /* GetScheme() tests. */
            prop = test.dword_props[Uri_PROPERTY_SCHEME-Uri_PROPERTY_DWORD_START];
            hr = IUri_GetScheme(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetScheme returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetScheme returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
            }
            received = -9999999;

            /* GetZone() tests. */
            prop = test.dword_props[Uri_PROPERTY_ZONE-Uri_PROPERTY_DWORD_START];
            hr = IUri_GetZone(uri, &received);
            if(prop.todo) {
                todo_wine {
                    ok(hr == prop.expected, "Error: GetZone returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                            hr, prop.expected, i);
                }
                todo_wine {
                    ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
                }
            } else {
                ok(hr == prop.expected, "Error: GetZone returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, prop.expected, i);
                ok(received == prop.value, "Error: Expected %d but got %d on uri_tests[%d].\n", prop.value, received, i);
            }
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

static void test_IUri_GetPropertyLength(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    /* Make sure it handles invalid args correctly. */
    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        DWORD received = 0xdeadbeef;

        hr = IUri_GetPropertyLength(uri, Uri_PROPERTY_STRING_START, NULL, 0);
        ok(hr == E_INVALIDARG, "Error: GetPropertyLength returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);

        hr = IUri_GetPropertyLength(uri, Uri_PROPERTY_DWORD_START, &received, 0);
        ok(hr == E_INVALIDARG, "Error: GetPropertyLength return 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);
        ok(received == 0xdeadbeef, "Error: Expected 0xdeadbeef but got 0x%08x.\n", received);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);
        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x on uri_tests[%d].\n",
                        hr, test.create_expected, i);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x on uri_test[%d].\n",
                    hr, test.create_expected, i);
        }

        if(SUCCEEDED(hr)) {
            DWORD j;

            for(j = Uri_PROPERTY_STRING_START; j <= Uri_PROPERTY_STRING_LAST; ++j) {
                DWORD expectedLen, receivedLen;
                uri_str_property prop = test.str_props[j];

                expectedLen = lstrlen(prop.value);

                /* This won't be necessary once GetPropertyLength is implemented. */
                receivedLen = -1;

                hr = IUri_GetPropertyLength(uri, j, &receivedLen, 0);
                if(prop.todo) {
                    todo_wine {
                        ok(hr == prop.expected, "Error: GetPropertyLength returned 0x%08x, expected 0x%08x on uri_tests[%d].str_props[%d].\n",
                                hr, prop.expected, i, j);
                    }
                    todo_wine {
                        ok(receivedLen == expectedLen, "Error: Expected a length of %d but got %d on uri_tests[%d].str_props[%d].\n",
                                expectedLen, receivedLen, i, j);
                    }
                    if (hr == E_NOTIMPL)  /* no point in continuing */
                    {
                        if(uri) IUri_Release(uri);
                        heap_free(uriW);
                        return;
                    }
                } else {
                    ok(hr == prop.expected, "Error: GetPropertyLength returned 0x%08x, expected 0x%08x on uri_tests[%d].str_props[%d].\n",
                            hr, prop.expected, i, j);
                    ok(receivedLen == expectedLen, "Error: Expected a length of %d but got %d on uri_tests[%d].str_props[%d].\n",
                            expectedLen, receivedLen, i, j);
                }
            }
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

static void test_IUri_GetProperties(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        hr = IUri_GetProperties(uri, NULL);
        ok(hr == E_INVALIDARG, "Error: GetProperties returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);
        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, test.create_expected);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, test.create_expected);
        }

        if(SUCCEEDED(hr)) {
            DWORD received = 0;
            DWORD j;

            hr = IUri_GetProperties(uri, &received);
            if(test.props_todo) {
                todo_wine {
                    ok(hr == S_OK, "Error: GetProperties returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
                }
                if (hr == E_NOTIMPL)  /* no point in continuing */
                {
                    if(uri) IUri_Release(uri);
                    heap_free(uriW);
                    return;
                }
            } else {
                ok(hr == S_OK, "Error: GetProperties returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
            }

            for(j = 0; j <= Uri_PROPERTY_DWORD_LAST; ++j) {
                /* (1 << j) converts a Uri_PROPERTY to its corresponding Uri_HAS_* flag mask. */
                if(test.props & (1 << j)) {
                    if(test.props_todo) {
                        todo_wine {
                            ok(received & (1 << j), "Error: Expected flag for property %d on uri_tests[%d].\n", j, i);
                        }
                    } else {
                        ok(received & (1 << j), "Error: Expected flag for property %d on uri_tests[%d].\n", j, i);
                    }
                } else {
                    /* NOTE: Since received is initialized to 0, this test will always pass while
                     * GetProperties is unimplemented.
                     */
                    ok(!(received & (1 << j)), "Error: Received flag for property %d when not expected on uri_tests[%d].\n", j, i);
                }
            }
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

static void test_IUri_HasProperty(void) {
    IUri *uri = NULL;
    HRESULT hr;
    DWORD i;

    hr = pCreateUri(http_urlW, 0, 0, &uri);
    ok(hr == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, S_OK);
    if(SUCCEEDED(hr)) {
        hr = IUri_HasProperty(uri, Uri_PROPERTY_RAW_URI, NULL);
        ok(hr == E_INVALIDARG, "Error: HasProperty returned 0x%08x, expected 0x%08x.\n", hr, E_INVALIDARG);
    }
    if(uri) IUri_Release(uri);

    for(i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); ++i) {
        uri_properties test = uri_tests[i];
        LPWSTR uriW;
        uri = NULL;

        uriW = a2w(test.uri);

        hr = pCreateUri(uriW, test.create_flags, 0, &uri);
        if(test.create_todo) {
            todo_wine {
                ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, test.create_expected);
            }
        } else {
            ok(hr == test.create_expected, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hr, test.create_expected);
        }

        if(SUCCEEDED(hr)) {
            DWORD j;

            for(j = 0; j <= Uri_PROPERTY_DWORD_LAST; ++j) {
                /* Assign -1, then explicitly test for TRUE or FALSE later. */
                BOOL received = -1;

                hr = IUri_HasProperty(uri, j, &received);
                if(test.props_todo) {
                    todo_wine {
                        ok(hr == S_OK, "Error: HasProperty returned 0x%08x, expected 0x%08x for property %d on uri_tests[%d].\n",
                                hr, S_OK, j, i);
                    }
                    if (hr == E_NOTIMPL)  /* no point in continuing */
                    {
                        if(uri) IUri_Release(uri);
                        heap_free(uriW);
                        return;
                    }

                    /* Check if the property should be true. */
                    if(test.props & (1 << j)) {
                        todo_wine {
                            ok(received == TRUE, "Error: Expected to have property %d on uri_tests[%d].\n", j, i);
                        }
                    } else {
                        todo_wine {
                            ok(received == FALSE, "Error: Wasn't expecting to have property %d on uri_tests[%d].\n", j, i);
                        }
                    }
                } else {
                    ok(hr == S_OK, "Error: HasProperty returned 0x%08x, expected 0x%08x for property %d on uri_tests[%d].\n",
                            hr, S_OK, j, i);

                    if(test.props & (1 << j)) {
                        ok(received == TRUE, "Error: Expected to have property %d on uri_tests[%d].\n", j, i);
                    } else {
                        ok(received == FALSE, "Error: Wasn't expecting to have property %d on uri_tests[%d].\n", j, i);
                    }
                }
            }
        }

        if(uri) IUri_Release(uri);

        heap_free(uriW);
    }
}

static void test_IUri_IsEqual(void) {
    IUri *uriA, *uriB;
    HRESULT hrA, hrB;
    DWORD i;

    uriA = uriB = NULL;

    /* Make sure IsEqual handles invalid args correctly. */
    hrA = pCreateUri(http_urlW, 0, 0, &uriA);
    hrB = pCreateUri(http_urlW, 0, 0, &uriB);
    ok(hrA == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hrA, S_OK);
    ok(hrB == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x.\n", hrB, S_OK);
    if(SUCCEEDED(hrA) && SUCCEEDED(hrB)) {
        BOOL equal = -1;
        hrA = IUri_IsEqual(uriA, NULL, &equal);
        ok(hrA == S_OK, "Error: IsEqual returned 0x%08x, expected 0x%08x.\n", hrA, S_OK);
        ok(equal == FALSE, "Error: Expected equal to be FALSE, but was %d instead.\n", equal);

        hrA = IUri_IsEqual(uriA, uriB, NULL);
        ok(hrA == E_POINTER, "Error: IsEqual returned 0x%08x, expected 0x%08x.\n", hrA, E_POINTER);
    }
    if(uriA) IUri_Release(uriA);
    if(uriB) IUri_Release(uriB);

    for(i = 0; i < sizeof(equality_tests)/sizeof(equality_tests[0]); ++i) {
        uri_equality test = equality_tests[i];
        LPWSTR uriA_W, uriB_W;

        uriA = uriB = NULL;

        uriA_W = a2w(test.a);
        uriB_W = a2w(test.b);

        hrA = pCreateUri(uriA_W, test.create_flags_a, 0, &uriA);
        if(test.create_todo_a) {
            todo_wine {
                ok(hrA == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x on equality_tests[%d].a\n",
                        hrA, S_OK, i);
            }
        } else {
            ok(hrA == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x on equality_tests[%d].a\n",
                    hrA, S_OK, i);
        }

        hrB = pCreateUri(uriB_W, test.create_flags_b, 0, &uriB);
        if(test.create_todo_b) {
            todo_wine {
                ok(hrB == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x on equality_tests[%d].b\n",
                        hrB, S_OK, i);
            }
        } else {
            ok(hrB == S_OK, "Error: CreateUri returned 0x%08x, expected 0x%08x on equality_tests[%d].b\n",
                    hrB, S_OK, i);
        }

        if(SUCCEEDED(hrA) && SUCCEEDED(hrB)) {
            BOOL equal = -1;

            hrA = IUri_IsEqual(uriA, uriB, &equal);
            if(test.todo) {
                todo_wine {
                    ok(hrA == S_OK, "Error: IsEqual returned 0x%08x, expected 0x%08x on equality_tests[%d].\n",
                            hrA, S_OK, i);
                }
                todo_wine {
                    ok(equal == test.equal, "Error: Expected the comparison to be %d on equality_tests[%d].\n", test.equal, i);
                }
            } else {
                ok(hrA == S_OK, "Error: IsEqual returned 0x%08x, expected 0x%08x on equality_tests[%d].\n", hrA, S_OK, i);
                ok(equal == test.equal, "Error: Expected the comparison to be %d on equality_tests[%d].\n", test.equal, i);
            }
        }
        if(uriA) IUri_Release(uriA);
        if(uriB) IUri_Release(uriB);

        heap_free(uriA_W);
        heap_free(uriB_W);
    }
}

START_TEST(uri) {
    HMODULE hurlmon;

    hurlmon = GetModuleHandle("urlmon.dll");
    pCreateUri = (void*) GetProcAddress(hurlmon, "CreateUri");

    if(!pCreateUri) {
        win_skip("CreateUri is not present, skipping tests.\n");
        return;
    }

    trace("test CreateUri invalid flags...\n");
    test_CreateUri_InvalidFlags();

    trace("test CreateUri invalid args...\n");
    test_CreateUri_InvalidArgs();

    trace("test IUri_GetPropertyBSTR...\n");
    test_IUri_GetPropertyBSTR();

    trace("test IUri_GetPropertyDWORD...\n");
    test_IUri_GetPropertyDWORD();

    trace("test IUri_GetStrProperties...\n");
    test_IUri_GetStrProperties();

    trace("test IUri_GetDwordProperties...\n");
    test_IUri_GetDwordProperties();

    trace("test IUri_GetPropertyLength...\n");
    test_IUri_GetPropertyLength();

    trace("test IUri_GetProperties...\n");
    test_IUri_GetProperties();

    trace("test IUri_HasProperty...\n");
    test_IUri_HasProperty();

    trace("test IUri_IsEqual...\n");
    test_IUri_IsEqual();
}
