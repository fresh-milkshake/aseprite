/* Aseprite
   Copyright (C) 2020-2024  Igara Studio S.A.

   This program is distributed under the terms of
   the End-User License Agreement for Aseprite.  */

#include "ver/info.h"
#include "generated_version.h" /* It defines the VERSION macro */

#define PACKAGE   "Aseprite"
#define COPYRIGHT "Copyright (C) 2001-2025 Igara Studio S.A."

#if defined(_WIN32) || defined(__APPLE__)
  #define HTTP "https"
#else
  #define HTTP "http"
#endif

#define WEBSITE HTTP "://www.aseprite.org/"
#define WEBSITE_DOWNLOAD     WEBSITE "download/"
#define WEBSITE_CONTRIBUTORS WEBSITE "contributors/"

const char* get_app_name()
{
  return PACKAGE;
}
const char* get_app_version()
{
  return VERSION;
}
const char* get_app_copyright()
{
  return COPYRIGHT;
}

const char* get_app_url()
{
  return WEBSITE;
}
const char* get_app_download_url()
{
  return WEBSITE_DOWNLOAD;
}
const char* get_app_contributors_url()
{
  return WEBSITE_CONTRIBUTORS;
}
