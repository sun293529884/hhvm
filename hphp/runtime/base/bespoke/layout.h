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

// don't mix this file up with runtime/base/bespoke-layout.h !
#ifndef HPHP_BESPOKEDIR_LAYOUT_H_
#define HPHP_BESPOKEDIR_LAYOUT_H_

#include "hphp/runtime/base/array-data.h"
#include "hphp/runtime/base/bespoke-array.h"
#include "hphp/runtime/vm/hhbc.h"
#include "hphp/util/type-scan.h"

#include <set>

namespace HPHP {

namespace jit {

struct SSATmp;
struct Block;

namespace irgen { struct IRGS; }

} // namespace jit

namespace bespoke {

void logBespokeDispatch(const ArrayData* ad, const char* fn);

#define BESPOKE_LAYOUT_FUNCTIONS(T) \
  X(size_t, HeapSize, const T* ad) \
  X(void, Scan, const T* ad, type_scan::Scanner& scanner) \
  X(ArrayData*, EscalateToVanilla, const T*, const char* reason) \
  X(void, ConvertToUncounted, T*, DataWalker::PointerMap* seen) \
  X(void, ReleaseUncounted, T*) \
  X(void, Release, T*) \
  X(bool, IsVectorData, const T*) \
  X(TypedValue, GetInt, const T*, int64_t) \
  X(TypedValue, GetStr, const T*, const StringData*) \
  X(TypedValue, GetKey, const T*, ssize_t pos) \
  X(TypedValue, GetVal, const T*, ssize_t pos) \
  X(ssize_t, GetIntPos, const T*, int64_t) \
  X(ssize_t, GetStrPos, const T*, const StringData*) \
  X(ssize_t, IterBegin, const T*) \
  X(ssize_t, IterLast, const T*) \
  X(ssize_t, IterEnd, const T*) \
  X(ssize_t, IterAdvance, const T*, ssize_t) \
  X(ssize_t, IterRewind, const T*, ssize_t) \
  X(arr_lval, LvalInt, T* ad, int64_t k) \
  X(arr_lval, LvalStr, T* ad, StringData* k) \
  X(tv_lval, ElemInt, tv_lval lval, int64_t k, bool) \
  X(tv_lval, ElemStr, tv_lval lval, StringData* k, bool) \
  X(ArrayData*, SetInt, T*, int64_t k, TypedValue v) \
  X(ArrayData*, SetStr, T*, StringData* k, TypedValue v)\
  X(ArrayData*, SetIntMove, T*, int64_t k, TypedValue v) \
  X(ArrayData*, SetStrMove, T*, StringData* k, TypedValue v)\
  X(ArrayData*, RemoveInt, T*, int64_t) \
  X(ArrayData*, RemoveStr, T*, const StringData*) \
  X(ArrayData*, Append, T*, TypedValue v) \
  X(ArrayData*, AppendMove, T*, TypedValue v) \
  X(ArrayData*, Pop, T*, Variant&) \
  X(ArrayData*, ToDVArray, T*, bool copy) \
  X(ArrayData*, ToHackArr, T*, bool copy) \
  X(ArrayData*, PreSort, T*, SortFunction sf) \
  X(ArrayData*, PostSort, T*, ArrayData* vad) \
  X(ArrayData*, SetLegacyArray, T*, bool copy, bool legacy)

struct LayoutFunctions {
#define X(Return, Name, Args...) Return (*fn##Name)(Args);
  BESPOKE_LAYOUT_FUNCTIONS(ArrayData)
#undef X
};

/**
 * Provides an interface between LayoutFunctions, which exposes methods
 * accepting ArrayData*, and the bespoke array implementations, which expose
 * methods accepting their array types. In a debug build, it uses the bespoke
 * array's static As() function to convert from ArrayData* to the specific
 * bespoke array type. This As() function should perform any invariant
 * checking. In a non-debug build, a reinterpret_cast is used to avoid any
 * overhead from this wrapper.
 */
template <typename Array>
struct LayoutFunctionDispatcher {
  ALWAYS_INLINE static Array* Cast(ArrayData* ad, const char* fn) {
    logBespokeDispatch(ad, fn);
    return Array::As(ad);
  }
  ALWAYS_INLINE static const Array* Cast(const ArrayData* ad, const char* fn) {
    logBespokeDispatch(ad, fn);
    return Array::As(ad);
  }

  static size_t HeapSize(const ArrayData* ad) {
    // NB: The garbage collector relies on this being computable even if
    // objects referenced by ad have been freed. As a result, we don't check
    // invariants.
    return Array::HeapSize(reinterpret_cast<const Array*>(ad));
  }
  static void Scan(const ArrayData* ad, type_scan::Scanner& scanner) {
    return Array::Scan(Cast(ad, __func__), scanner);
  }
  static ArrayData* EscalateToVanilla(const ArrayData* ad, const char* reason) {
    return Array::EscalateToVanilla(Cast(ad, __func__), reason);
  }
  static void ConvertToUncounted(ArrayData* ad, DataWalker::PointerMap* seen) {
    return Array::ConvertToUncounted(Cast(ad, __func__), seen);
  }
  static void ReleaseUncounted(ArrayData* ad) {
    return Array::ReleaseUncounted(Cast(ad, __func__));
  }
  static void Release(ArrayData* ad) {
    return Array::Release(Cast(ad, __func__));
  }
  static bool IsVectorData(const ArrayData* ad) {
    return Array::IsVectorData(Cast(ad, __func__));
  }
  static TypedValue GetInt(const ArrayData* ad, int64_t k) {
    return Array::GetInt(Cast(ad, __func__), k);
  }
  static TypedValue GetStr(const ArrayData* ad, const StringData* k) {
    return Array::GetStr(Cast(ad, __func__), k);
  }
  static TypedValue GetKey(const ArrayData* ad, ssize_t pos) {
    return Array::GetKey(Cast(ad, __func__), pos);
  }
  static TypedValue GetVal(const ArrayData* ad, ssize_t pos) {
    return Array::GetVal(Cast(ad, __func__), pos);
  }
  static ssize_t GetIntPos(const ArrayData* ad, int64_t k) {
    return Array::GetIntPos(Cast(ad, __func__), k);
  }
  static ssize_t GetStrPos(const ArrayData* ad, const StringData* k) {
    return Array::GetStrPos(Cast(ad, __func__), k);
  }
  static arr_lval LvalInt(ArrayData* ad, int64_t k) {
    return Array::LvalInt(Cast(ad, __func__), k);
  }
  static arr_lval LvalStr(ArrayData* ad, StringData* k) {
    return Array::LvalStr(Cast(ad, __func__), k);
  }
  static tv_lval ElemInt(tv_lval lval, int64_t k, bool throwOnMissing) {
    Cast(lval.val().parr, __func__);
    return Array::ElemInt(lval, k, throwOnMissing);
  }
  static tv_lval ElemStr(tv_lval lval, StringData* k, bool throwOnMissing) {
    Cast(lval.val().parr, __func__);
    return Array::ElemStr(lval, k, throwOnMissing);
  }
  static ArrayData* SetInt(ArrayData* ad, int64_t k, TypedValue v) {
    return Array::SetInt(Cast(ad, __func__), k, v);
  }
  static ArrayData* SetStr(ArrayData* ad, StringData* k, TypedValue v){
    return Array::SetStr(Cast(ad, __func__), k, v);
  }
  static ArrayData* SetIntMove(ArrayData* ad, int64_t k, TypedValue v) {
    return Array::SetIntMove(Cast(ad, __func__), k, v);
  }
  static ArrayData* SetStrMove(ArrayData* ad, StringData* k, TypedValue v){
    return Array::SetStrMove(Cast(ad, __func__), k, v);
  }
  static ArrayData* RemoveInt(ArrayData* ad, int64_t k) {
    return Array::RemoveInt(Cast(ad, __func__), k);
  }
  static ArrayData* RemoveStr(ArrayData* ad, const StringData* k) {
    return Array::RemoveStr(Cast(ad, __func__), k);
  }
  static ssize_t IterBegin(const ArrayData* ad) {
    return Array::IterBegin(Cast(ad, __func__));
  }
  static ssize_t IterLast(const ArrayData* ad) {
    return Array::IterLast(Cast(ad, __func__));
  }
  static ssize_t IterEnd(const ArrayData* ad) {
    return Array::IterEnd(Cast(ad, __func__));
  }
  static ssize_t IterAdvance(const ArrayData* ad, ssize_t pos) {
    return Array::IterAdvance(Cast(ad, __func__), pos);
  }
  static ssize_t IterRewind(const ArrayData* ad, ssize_t pos) {
    return Array::IterRewind(Cast(ad, __func__), pos);
  }
  static ArrayData* Append(ArrayData* ad, TypedValue v) {
    return Array::Append(Cast(ad, __func__), v);
  }
  static ArrayData* AppendMove(ArrayData* ad, TypedValue v) {
    return Array::AppendMove(Cast(ad, __func__), v);
  }
  static ArrayData* Pop(ArrayData* ad, Variant& v) {
    return Array::Pop(Cast(ad, __func__), v);
  }
  static ArrayData* ToDVArray(ArrayData* ad, bool copy) {
    return Array::ToDVArray(Cast(ad, __func__), copy);
  }
  static ArrayData* ToHackArr(ArrayData* ad, bool copy) {
    return Array::ToHackArr(Cast(ad, __func__), copy);
  }
  static ArrayData* PreSort(ArrayData* ad, SortFunction sf) {
    return Array::PreSort(Cast(ad, __func__), sf);
  }
  static ArrayData* PostSort(ArrayData* ad, ArrayData* vad) {
    return Array::PostSort(Cast(ad, __func__), vad);
  }
  static ArrayData* SetLegacyArray(ArrayData* ad, bool copy, bool legacy) {
    return Array::SetLegacyArray(Cast(ad, __func__), copy, legacy);
  }
};

template <typename Array>
constexpr LayoutFunctions fromArray() {
  LayoutFunctions result;
  if constexpr (debug) {
#define X(Return, Name, Args...) \
  result.fn##Name = LayoutFunctionDispatcher<Array>::Name;
  BESPOKE_LAYOUT_FUNCTIONS(ArrayData)
#undef X
  } else {
#define X(Return, Name, Args...) \
  result.fn##Name = reinterpret_cast<Return(*)(Args)>(Array::Name);
  BESPOKE_LAYOUT_FUNCTIONS(ArrayData)
#undef X
  }
  return result;
}

/*
 * A bespoke::Layout can represent either both the concrete layout of a given
 * BespokeArray or some abstract type that's a union of concrete layouts.
 *
 * bespoke::Layout also maintains the type hierarchy of bespoke layouts.
 * Bespoke layouts form a lattice, with BespokeTop as the top type and the null
 * layout as the bottom type. Each layout specifies its set of immediate
 * parents and whether or not it is "liveable"--whether it is sufficiently
 * general to be used as a guard type for a live translation. The type
 * hierarchy satisfies the following constraints:
 *
 *   1) When a layout is initialized, all of its parents must have already been
 *   initialized. This ensures that the type hierarchy is a DAG. Each layout
 *   other than BespokeTop must have at least one parent.
 *
 *   2) The supplied parents of each node are immediate parents. That is, no
 *   supplied parent can be an ancestor of another supplied parent. This
 *   ensures that the parent edges form a covering relation and simplifies the
 *   process of computing joins and meets.
 *
 *   3) The type hierarchy forms a join semilattice. That is, the least upper
 *   bound of every pair of layouts exists (this is trivial as we have
 *   BespokeTop) and is unique. Together with our bottom type, this implies
 *   that the type hierarchy is a lattice in which both least upper bounds and
 *   greatest lower bounds are unique.
 *
 *   4) Each layout has a distinct least liveable ancestor. This is equivalent
 *   to the constraint that each liveable layout is the unique parent of each
 *   of its non-liveable immediate children. This makes the process of
 *   converting a layout to a liveable layout unambiguous.
 *
 * These constraints are validated upon the creation of each layout in debug
 * mode. If the constraints are satisfied, we are left with a DAG corresponding
 * to the covering relation of a valid lattice, in which join and meet can be
 * implemented by simple BFS.
 *
 * Once the type hierarchy has been created, we supply the standard <=, meet
 * (&), and join (|) operations for the layouts, which are used from ArraySpec
 * for type operations. We further expose getLiveableAncestor to obtain a
 * layout's least liveable ancestor. Note that several of these operations
 * cannot be correctly implemented until the layout hierarchy is finalized
 * (which occurs when FinalizeHierarchy is invoked).
 *
 * When the layout hierarchy is final, all type operations are valid. Before
 * the layout hierarchy is final, only type operations on BespokeTop are
 * permitted. This enables us to use BespokeTop and normal type operations in
 * profiling tracelets while disallowing more specific type operations that
 * require knowledge of the bespoke hierarchy.
 */
struct Layout {
  using LayoutSet = std::set<LayoutIndex>;

  Layout(const Layout& l) = delete;
  Layout& operator=(const Layout&) = delete;

  virtual ~Layout() {}

  /*
   * Bespoke indexes are 15 bits wide. When we store them in m_extra of
   * ArrayData, we always set the sign bit, which allows us to test that
   * (m_size >= constant && isVanilla()) in a single comparison.
   */
  static constexpr LayoutIndex kMaxIndex = {(1 << 15) - 1};

  LayoutIndex index() const { return m_index; }
  const std::string& describe() const { return m_description; }
  virtual bool isConcrete() const { return false; }
  /*
   * Checks whether the layout is marked as "liveable"--general enough to be
   * used as a guard type in a live translation.
   */
  bool isLiveable() const { return m_liveable; }

  /*
   * In order to support efficient layout type tests in the JIT, we let
   * layout initializers reserve aligned blocks of indices to populate.
   *
   *   @precondition:  `size` must be a power of two.
   *   @postcondition: The result is a multiple of size.
   */
  static LayoutIndex ReserveIndices(size_t size);
  static const Layout* FromIndex(LayoutIndex index);

  /*
   * Seals the bespoke type hierarchy. Before this is invoked, type operations
   * on bespoke layouts other than BespokeTop are invalid. After it is invoked,
   * all type operations are valid but no new layouts can be created.
   */
  static void FinalizeHierarchy();

  /*
   * Returns the layout's unique least liveable ancestor for use in a live
   * translation. If the type hierarchy is not finalized, we simply return
   * BespokeTop. This enables us to support live translations that may be
   * created before RTA has finalized the hierarchy.
   */
  const Layout* getLiveableAncestor() const;

  bool operator<=(const Layout& other) const;
  const Layout* operator|(const Layout& other) const;
  const Layout* operator&(const Layout& other) const;

  ///////////////////////////////////////////////////////////////////////////

  /*
   * JIT support
   *
   * In all the irgen emit helpers below, `arr` is guaranteed to be an array
   * matching this BespokeLayout's type class.
   *
   * For those methods that take `key`, it is guaranteed to be a valid key
   * for the base's type. For example, if `arr` is a dict, then `key` is an
   * arraykey, and if `arr` is a vec, `key` is an int. (We make no claims
   * about whether `key` matches tighter per-layout type constraints.)
   */

  using SSATmp = jit::SSATmp;
  using Block = jit::Block;
  using IRGS = jit::irgen::IRGS;

  /*
   * Return the value at `key` in `arr`, branching to `taken` if the key is
   * not present. This operation does no refcounting.
   */
  virtual SSATmp* emitGet(
      IRGS& env, SSATmp* arr, SSATmp* key, Block* taken) const = 0;

  /*
   * Return a half-lval (immutable type pointer) to the value at `key` in the
   * array at `lval`. If escalation or copying is performed, the array at
   * `lval` is updated.  If the key is not present, it throws if
   * `throwOnMissing` is true.  Otherwise, it returns an lval to
   * immutable_null_base. This operation does no refcounting.
   */
  virtual SSATmp* emitElem(
      IRGS& env, SSATmp* lval, SSATmp* key, bool throwOnMissing) const = 0;

  /*
   * Create a new array by setting `arr[key] = val`, CoWing or escalating as
   * needed. This op consumes a ref on `arr` and produces a ref on the result.
   */
  virtual SSATmp* emitSet(
      IRGS& env, SSATmp* arr, SSATmp* key, SSATmp* val) const = 0;

  /*
   * Create a new array by setting `arr[] = val`, CoWing or escalating as
   * needed. This op consumes a ref on `arr` and produces a ref on the result.
   */
  virtual SSATmp* emitAppend(
      IRGS& env, SSATmp* arr, SSATmp* val) const = 0;

  /*
   * Escalate the bespoke array to vanilla. The default implementation invokes
   * the general BespokeArray implementation. It performs no refcounting
   * operations.
   */
  virtual SSATmp* emitEscalateToVanilla(
      IRGS& env, SSATmp* arr, const char* reason) const;

  /**
   * Obtain the pos corresponding to the first valid element (i.e. not a
   * tombstone).
   */
  virtual SSATmp* emitIterFirstPos(IRGS& env, SSATmp* arr) const = 0;

  /**
   * Obtain the pos in the array that corresponding to the last to a valid
   * element (i.e. not a tombstone).
   */
  virtual SSATmp* emitIterLastPos(IRGS& env, SSATmp* arr) const = 0;

  /**
   * Obtain the pos in the array corresponding to the specified index. It
   * assumes that the array contains no tombstones.
   */
  virtual SSATmp* emitIterPos(IRGS& env, SSATmp* arr, SSATmp* idx) const = 0;

  /**
   * Advance the supplied pos a single step forward.
   */
  virtual SSATmp* emitIterAdvancePos(
      IRGS& env, SSATmp* arr, SSATmp* pos) const = 0;

  /**
   * Convert the supplied pos to an elm used to access the element.
   */
  virtual SSATmp* emitIterElm(IRGS& env, SSATmp* arr, SSATmp* pos) const = 0;

  /**
   * Obtain the key at the supplied elm.
   */
  virtual SSATmp* emitIterGetKey(IRGS& env, SSATmp* arr, SSATmp* elm) const = 0;

  /**
   * Obtain the value at the supplied elm.
   */
  virtual SSATmp* emitIterGetVal(IRGS& env, SSATmp* arr, SSATmp* elm) const = 0;


protected:
  explicit Layout(const std::string& description, LayoutSet&& parents,
                  bool liveable);
  Layout(LayoutIndex index, const std::string& description, LayoutSet&& parents,
         bool liveable);

private:
  bool checkInvariants() const;
  bool isDescendentOf(const Layout* other) const;
  LayoutSet computeAncestors() const;
  LayoutSet computeDescendents() const;
  const Layout* nearestBound(bool upward, const Layout* other) const;
  const Layout* nearestBoundDebug(bool upward, const Layout* other) const;

  struct Initializer;
  static Initializer s_initializer;
  struct BFSWalker;

  LayoutIndex m_index;
  std::string m_description;
  const LayoutFunctions* m_vtable;
  LayoutSet m_parents;
  LayoutSet m_children;
  bool m_liveable;
};

/*
 * A concrete bespoke layout providing a vtable to access the bespoke array
 * implementation methods. It also provides default implementations for the
 * various JIT helpers in terms of the vtable methods.
 */
struct ConcreteLayout : public Layout {
  ConcreteLayout(const std::string& description, const LayoutFunctions* vtable,
                 LayoutSet&& parents, bool liveable);
  ConcreteLayout(LayoutIndex index, const std::string& description,
                 const LayoutFunctions* vtable, LayoutSet&& parents,
                 bool liveable);
  virtual ~ConcreteLayout() {}

  const LayoutFunctions* vtable() const { return m_vtable; }
  bool isConcrete() const override { return true; }

  static const ConcreteLayout* FromConcreteIndex(LayoutIndex index);

  ///////////////////////////////////////////////////////////////////////////

  using SSATmp = jit::SSATmp;
  using Block = jit::Block;
  using IRGS = jit::irgen::IRGS;

  /*
   * This default implementation invokes the layout-specific GetInt/GetStr
   * methods without virtualization.
   */
  virtual SSATmp* emitGet(
      IRGS& env, SSATmp* arr, SSATmp* key, Block* taken) const override;

  /*
   * This default implementation invokes the layout-specific ElemInt/ElemStr
   * methods without virtualization.
   */
  virtual SSATmp* emitElem(
      IRGS& env, SSATmp* lval, SSATmp* key, bool throwOnMissing) const override;

  /*
   * This default implementation punts.
   */
  virtual SSATmp* emitSet(
      IRGS& env, SSATmp* arr, SSATmp* key, SSATmp* val) const override;

  /*
   * This default implementation punts.
   */
  virtual SSATmp* emitAppend(
      IRGS& env, SSATmp* arr, SSATmp* val) const override;

  /*
   * This default implementation invokes the layout-specific EscalateToVanilla
   * method without virtualization.
   */
  virtual SSATmp* emitEscalateToVanilla(
      IRGS& env, SSATmp* arr, const char* reason) const override;

  /**
   * This default implementation invokes the layout-specific IterBegin method
   * without virtualization.
   */
  virtual SSATmp* emitIterFirstPos(IRGS& env, SSATmp* arr) const override;

  /**
   * This default implementation invokes the layout-specific IterLast method
   * without virtualization.
   */
  virtual SSATmp* emitIterLastPos(IRGS& env, SSATmp* arr) const override;

  /**
   * This default implementation punts.
   */
  virtual SSATmp* emitIterPos(
      IRGS& env, SSATmp* arr, SSATmp* idx) const override;

  /**
   * This default implementation invokes the layout-specific IterAdvance method
   * without virtualization.
   */
  virtual SSATmp* emitIterAdvancePos(
      IRGS& env, SSATmp* arr, SSATmp* pos) const override;

  /**
   * This default implementation returns the supplied pos. In other words,
   * elm = pos for all indices.
   */
  virtual SSATmp* emitIterElm(
      IRGS& env, SSATmp* arr, SSATmp* pos) const override;

  /**
   * This default implementation invokes the layout-specific GetPosKey method
   * without virtualization. If a non-default implementation of emitIterElm is
   * provided, this will also have to be updated.
   */
  virtual SSATmp* emitIterGetKey(
      IRGS& env, SSATmp* arr, SSATmp* elm) const override;

  /**
   * This default implementation invokes the layout-specific GetPosVal method
   * without virtualization. If a non-default implementation of emitIterElm is
   * provided, this will also have to be updated.
   */
  virtual SSATmp* emitIterGetVal(
      IRGS& env, SSATmp* arr, SSATmp* elm) const override;

private:
  const LayoutFunctions* m_vtable;
};

}}

#endif // HPHP_BESPOKEDIR_LAYOUT_H_
