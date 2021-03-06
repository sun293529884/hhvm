/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-present Facebook, Inc. (http://www.facebook.com)  |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#pragma once

#if !defined(SKIP_USER_CHANGE)

#include <sys/types.h>
#include <unistd.h>
#include <string>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

struct Capability {
  /**
   * This sets the  effective user ID of the current process, leaving
   * capability of binding to system ports (< 1024) to the user.
   */
  static bool ChangeUnixUser(uid_t uid, bool allowRoot);
  static bool ChangeUnixUser(const std::string &username, bool allowRoot);
  static bool SetDumpable();
};

///////////////////////////////////////////////////////////////////////////////
}

#endif

