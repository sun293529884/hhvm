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

#ifndef HPHP_BESPOKE_LAYOUT_H_
#define HPHP_BESPOKE_LAYOUT_H_

#include <cstdint>
#include <string>

#include "hphp/runtime/vm/hhbc.h"
#include "hphp/util/assertions.h"

#include <folly/Optional.h>

namespace HPHP {

namespace jit {

struct Block;
struct SSATmp;
struct IRInstruction;

namespace irgen { struct IRGS; }

} // namespace jit

namespace bespoke { struct Layout; struct ConcreteLayout; }

/*
 * Identifies information about a bespoke layout necessary to JIT code handling
 * arrays of that layout
 */
struct BespokeLayout {
  explicit BespokeLayout(const bespoke::Layout* layout);
  explicit BespokeLayout(const bespoke::ConcreteLayout* layout);

  bool operator==(const BespokeLayout& o) const {
    return o.m_layout == m_layout;
  }
  bool operator!=(const BespokeLayout& o) const {
    return !(*this == o);
  }
  bool operator<=(const BespokeLayout& o) const;
  BespokeLayout operator|(const BespokeLayout& o) const;
  folly::Optional<BespokeLayout> operator&(const BespokeLayout& o) const;
  BespokeLayout getLiveableAncestor() const;
  static void FinalizeHierarchy();

  /* get the index of this layout */
  uint16_t index() const;

  /* retrieve a layout by index */
  static BespokeLayout FromIndex(uint16_t index);

  /* retrieve the logging layout */
  static BespokeLayout LoggingLayout();
  static BespokeLayout TopLayout();

  /* get a human-readable string describing the layout */
  const std::string& describe() const;

  /****************************************************************************
   * access to arraydata methods
   ****************************************************************************/

  using SSATmp = jit::SSATmp;
  using Block = jit::Block;
  using IRInstruction = jit::IRInstruction;
  using IRGS = jit::irgen::IRGS;

  SSATmp* emitGet(IRGS& env, SSATmp* arr, SSATmp* key, Block* taken) const;
  SSATmp* emitElem(IRGS& env, SSATmp* arr, SSATmp* key, bool throwOnMissing) const;
  SSATmp* emitSet(IRGS& env, SSATmp* arr, SSATmp* key, SSATmp* val) const;
  SSATmp* emitAppend(IRGS& env, SSATmp* arr, SSATmp* val) const;
  SSATmp* emitEscalateToVanilla(IRGS& env, SSATmp* arr, const char* reason) const;
  SSATmp* emitIterFirstPos(IRGS& env, SSATmp* arr) const;
  SSATmp* emitIterLastPos(IRGS& env, SSATmp* arr) const;
  SSATmp* emitIterPos(IRGS& env, SSATmp* arr, SSATmp* idx) const;
  SSATmp* emitIterElm(IRGS& env, SSATmp* arr, SSATmp* pos) const;
  SSATmp* emitIterGetKey(IRGS& env, SSATmp* arr, SSATmp* elm) const;
  SSATmp* emitIterGetVal(IRGS& env, SSATmp* arr, SSATmp* elm) const;

private:
  const bespoke::Layout* m_layout{nullptr};
};

} // namespace HPHP

#endif // HPHP_BESPOKE_LAYOUT_H_
