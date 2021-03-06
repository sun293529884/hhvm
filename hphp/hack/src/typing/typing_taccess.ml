(*
 * Copyright (c) 2015, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)

open Hh_prelude
open Common
open Typing_defs
module Reason = Typing_reason
module Env = Typing_env
module Log = Typing_log
module Phase = Typing_phase
module TySet = Typing_set
module TR = Typing_reactivity
module CT = Typing_subtype.ConditionTypes
module Cls = Decl_provider.Class
module MakeType = Typing_make_type

(* A guiding principle when expanding a type access C::T is that if C <: D and
   we know that D::T = X (represented by an Exact result below), then C::T is
   also X. So Exact is propagated down the <: relation, see `update_class_name`
   below where this behavior is encoded. *)

type context = {
  id: Nast.sid;  (** The T in the type access C::T *)
  root_pos: Pos.t;
  ety_env: expand_env;
      (** The expand environment as passed in by Typing_phase.localize *)
  generics_seen: TySet.t;
      (** A set of visited types used to avoid infinite loops during expansion. *)
  allow_abstract: bool;
      (** Whether or not an abstract type constant is allowed as the result. In the
          future, this boolean should disappear and abstract type constants should
          appear only in the class where they are defined. *)
  abstract_as_tyvar: bool;
      (** If set, abstract type constants will be expanded as type variables. This
          is a hack which should naturally go away when the semantics of abstract
          type constants is cleaned up. *)
  base: locl_ty option;
      (** The origin of the extension. For example if TC is a generic parameter
          subject to the constraint TC as C and we would like to expand TC::T we
          will expand C::T with base set to `Some (Tgeneric "TC")` (and root set
          to C). If it is None the base is exactly the current root. *)
  on_error: Errors.typing_error_callback;  (** A callback for errors *)
}

(** The result of an expansion
   - Missing err means that the type constant is not present, with error function
     to be called if we need to report this
   - Exact ty means that the expansion results precisely in 'ty'
   - Abstract (n0, [n1, n2, n3], bound) means that the result is a
     generic with name n0::T such that:
     n0::T as n1::T as n2::T as n3::T as bound *)
type result =
  | Missing of (unit -> unit)
  | Exact of locl_ty
  | Abstract of string * string list * locl_ty option

let make_reason env id root r =
  Reason.Rtypeconst (r, id, Typing_print.error env root, get_reason root)

(* FIXME: It is bogus to use strings here and put them in Tgeneric; one
   possible problem is when a type parameter has a name which conflicts
   with a class name *)
let tp_name class_name id = class_name ^ "::" ^ snd id

(** A smart constructor for Abstract that also checks if the type we are
    creating is known to be equal to some other type *)
let make_abstract env id name namel bnd =
  let tp_name = tp_name name id in
  if not (Typing_set.is_empty (Env.get_equal_bounds env tp_name [])) then
    (* If the resulting abstract type is exactly equal to something,
       mark the result as exact.
       For example, if we have the following
       abstract class Box {
         abstract const type T;
       }
       function addFiveToValue<T1 as Box>(T1 $x) : int where T1::T = int {
           return $x->get() + 5;
       }
       Here, $x->get() has type expr#1::T as T1::T (as Box::T).
       But T1::T is exactly equal to int, so $x->get() no longer needs
       to be expression dependent. Thus, $x->get() typechecks. *)
    Exact (MakeType.generic Reason.Rnone tp_name)
  else
    Abstract (name, namel, bnd)

(** Lookup a type constant in a class and return a result. A type constant has
    both a constraint type and assigned type. Which one we choose depends if
    the current root is the base (origin) of the expansion, or if it is an
    upper bound of the base. *)
let create_root_from_type_constant ctx env root (_class_pos, class_name) class_
    =
  let { id = (id_pos, id_name) as id; _ } = ctx in
  match Env.get_typeconst env class_ id_name with
  | None ->
    ( env,
      Missing
        (fun () ->
          if not ctx.ety_env.quiet then
            Errors.smember_not_found
              `class_typeconst
              id_pos
              (Cls.pos class_, class_name)
              id_name
              `no_hint
              ctx.on_error) )
  | Some typeconst ->
    let name = tp_name class_name id in
    let type_expansions =
      (false, id_pos, name) :: ctx.ety_env.type_expansions
    in
    (match Typing_defs.has_expanded ctx.ety_env name with
    | Some report ->
      ( if report then
        let seen = List.rev_map type_expansions (fun (_, _, x) -> x) in
        Errors.cyclic_typeconst (fst typeconst.ttc_name) seen );
      (* This is a cycle through a type constant that we are using *)
      (env, Missing (fun () -> ()))
    | None ->
      let drop_exact ty =
        (* Legacy behavior is to preserve exactness only on `this` and not
       through `this::T` *)
        map_ty ty ~f:(function
            | Tclass (cid, _, tyl) -> Tclass (cid, Nonexact, tyl)
            | ty -> ty)
      in
      let ety_env =
        let from_class = None in
        let this_ty = drop_exact (Option.value ctx.base ~default:root) in
        { ctx.ety_env with from_class; type_expansions; this_ty }
      in
      let make_abstract env bnd =
        ( if (not ctx.allow_abstract) && not ety_env.quiet then
          let tc_pos = fst typeconst.ttc_name in
          Errors.abstract_tconst_not_allowed id_pos (tc_pos, id_name) );
        (* TODO(T59448452): this treatment of abstract type constants is unsound *)
        make_abstract env id class_name [] bnd
      in
      (* Quiet: don't report errors in expanded definition or constraint.
       * These will have been reported at the definition site already. *)
      let ety_env = { ety_env with quiet = true } in
      (match typeconst with
      (* Concrete type constants *)
      | { ttc_type = Some ty; ttc_constraint = None; _ } ->
        let (env, ty) = Phase.localize ~ety_env env ty in
        let ty = map_reason ty ~f:(make_reason env id root) in
        (env, Exact ty)
      (* A type constant with default can be seen as abstract or exact, depending
     on the root and base of the access. *)
      | { ttc_type = Some ty; ttc_constraint = Some _; _ } ->
        let (env, ty) = Phase.localize ~ety_env env ty in
        let ty = map_reason ty ~f:(make_reason env id root) in
        if Cls.final class_ || Option.is_none ctx.base then
          (env, Exact ty)
        else
          (env, make_abstract env (Some ty))
      (* Abstract type constants with constraint *)
      | { ttc_constraint = Some cstr; _ } ->
        let (env, cstr) = Phase.localize ~ety_env env cstr in
        (env, make_abstract env (Some cstr))
      (* Abstract type constant without constraint. *)
      | _ -> (env, make_abstract env None)))

(* Cheap intersection operation. Do not call Typing_intersection.intersect
 * as this calls into subtype which in turn calls into expand_with_env below
 *)
let intersect ty1 ty2 =
  if equal_locl_ty ty1 ty2 then
    ty1
  else
    MakeType.intersection (get_reason ty1) [ty1; ty2]

(* Cheap union operation. Do not call Typing_union.union
 * as this calls into subtype which in turn calls into expand_with_env below
 *)
let union ty1 ty2 =
  if equal_locl_ty ty1 ty2 then
    ty1
  else
    MakeType.union (get_reason ty1) [ty1; ty2]

(* Given the results of projecting a type constant from types t1 , ... , tn,
 * determine the result of projecting a type constant from type (t1 | ... | tn).
 * If the type constant is missing from any of the disjuncts, then it's treated
 * as missing from the union.
 *)
let rec union_results err rl =
  match rl with
  | [] -> Missing err
  | [r] -> r
  | r1 :: rl ->
    let r2 = union_results err rl in

    (* Union is defined iff both are defined *)
    (match (r1, r2) with
    | (Missing err, _)
    | (_, Missing err) ->
      Missing err
    (* In essence, this says (C | D)::TP = (C::TP) | (D::TP) *)
    | (Exact ty1, Exact ty2) -> Exact (union ty1 ty2)
    | (Abstract (id1, ids1, tyopt1), Abstract (id2, ids2, tyopt2))
      when String.equal id1 id2 && List.equal String.equal ids1 ids2 ->
      (* Take the union of the bounds on abstract type constants *)
      let tyopt =
        match (tyopt1, tyopt2) with
        | (None, _)
        | (_, None) ->
          None
        | (Some ty1, Some ty2) -> Some (union ty1 ty2)
      in
      Abstract (id1, ids1, tyopt)
    (* If paths don't match, regard the type constant as missing *)
    | (Abstract _, Abstract _) -> Missing err
    | (Abstract (id, ids, tyopt), Exact ty)
    | (Exact ty, Abstract (id, ids, tyopt)) ->
      Abstract
        ( id,
          ids,
          match tyopt with
          | None -> None
          | Some bound -> Some (union ty bound) ))

(* Given the results of projecting a type constant from types t1, ..., tn,
 * determine the result of projecting a type constant from type (t1 & ... & tn).
 *)
let rec intersect_results err rl =
  match rl with
  (* Essentially, this is `mixed`. *)
  | [] -> Missing err
  | [r] -> r
  | r1 :: rs ->
    let r2 = intersect_results err rs in
    (match (r1, r2) with
    | (Missing _, r)
    | (r, Missing _) ->
      r
    (* In essence, we're saying (C & D)::TP = (C::TP) & (D::TP) *)
    | (Exact ty1, Exact ty2) -> Exact (intersect ty1 ty2)
    | (Abstract (id1, ids1, tyopt1), Abstract (id2, ids2, tyopt2))
      when String.equal id1 id2 && List.equal String.equal ids1 ids2 ->
      (* For abstract type constants, take the intersection of the bounds *)
      let tyopt =
        match (tyopt1, tyopt2) with
        | (None, None) -> None
        | (Some ty, None)
        | (None, Some ty) ->
          Some ty
        | (Some ty1, Some ty2) -> Some (intersect ty1 ty2)
      in
      Abstract (id1, ids1, tyopt)
    (* The strategy here is to take the last result. It is necessary for
         poor reasons, unfortunately. Because `type_of_result` bogusly uses
         `Env.add_upper_bound_global`, local type refinement information can
         leak outside its scope. To remain consistent with the previous
         version of the type access algorithm wrt this bug, we pick the last
         result. See T59317869.
         The test test/typecheck/tconst/type_refinement_stress.php monitors
         the situation here. *)
    | (Abstract _, Abstract _) -> r2
    (* Exact type overrides abstract type: the bound on abstract type will be checked
    * against the exact type at implementation site. *)
    | (Abstract _, Exact ty)
    | (Exact ty, Abstract _) ->
      Exact ty)

let rec type_of_result ~ignore_errors ctx env root res =
  let { id = (id_pos, id_name) as id; _ } = ctx in
  let type_with_bound env as_tyvar name bnd =
    if as_tyvar then (
      let (env, tvar) = Env.fresh_invariant_type_var env id_pos in
      Log.log_new_tvar_for_tconst_access env id_pos tvar name id_name;
      (env, tvar)
    ) else
      let generic_name = tp_name name id in
      let reason = make_reason env id root Reason.Rnone in
      let ty = MakeType.generic reason generic_name in
      let env =
        Option.fold bnd ~init:env ~f:(fun env bnd ->
            (* TODO(T59317869): play well with flow sensitivity *)
            Env.add_upper_bound_global env generic_name bnd)
      in
      (env, ty)
  in
  match res with
  | Exact ty -> (env, ty)
  | Abstract (name, name' :: namel, bnd) ->
    let res' = Abstract (name', namel, bnd) in
    let (env, ty) = type_of_result ~ignore_errors ctx env root res' in
    type_with_bound env false name (Some ty)
  | Abstract (name, [], bnd) ->
    type_with_bound env ctx.abstract_as_tyvar name bnd
  | Missing err ->
    if (not ctx.ety_env.quiet) && not ignore_errors then err ();
    let reason = make_reason env id root Reason.Rnone in
    (env, Typing_utils.terr env reason)

let update_class_name env id new_name = function
  | (Exact _ | Missing _) as res -> res
  | Abstract (name, namel, bnd) ->
    make_abstract env id new_name (name :: namel) bnd

let rec expand ctx env root : _ * result =
  let (env, root) = Env.expand_type env root in
  let err () =
    let (pos, tconst) = ctx.id in
    let ty = Typing_print.error env root in
    Errors.non_object_member_read
      ~is_method:false
      tconst
      (get_pos root)
      ty
      pos
      ctx.on_error
  in

  match get_node root with
  | Tany _
  | Terr ->
    (env, Exact root)
  | Tnewtype (name, _, ty) ->
    let ctx =
      let base = Some (Option.value ctx.base ~default:root) in
      let allow_abstract = true in
      { ctx with base; allow_abstract }
    in
    let (env, res) = expand ctx env ty in
    let name = Printf.sprintf "<cls#%s>" name in
    (env, update_class_name env ctx.id name res)
  | Tclass (cls, _, _) ->
    begin
      match Env.get_class env (snd cls) with
      | None -> (env, Missing (fun () -> ()))
      | Some ci ->
        (* Hack: `self` in a trait is mistakenly replaced by the trait instead
           of the class using the trait, so if a trait is the root, it is
           likely because originally there was `self::T` written.
           TODO(T54081153): fix `self` in traits and clean this up *)
        let allow_abstract =
          Ast_defs.(equal_class_kind (Decl_provider.Class.kind ci) Ctrait)
          || ctx.allow_abstract
        in

        let ctx = { ctx with allow_abstract } in
        create_root_from_type_constant ctx env root cls ci
    end
  | Tgeneric (s, tyargs) ->
    let ctx =
      let generics_seen = TySet.add root ctx.generics_seen in
      let base = Some (Option.value ctx.base ~default:root) in
      let allow_abstract = true in
      let abstract_as_tyvar = false in
      { ctx with generics_seen; base; allow_abstract; abstract_as_tyvar }
    in

    (* Ignore seen bounds to avoid infinite loops *)
    let upper_bounds =
      TySet.elements
        (TySet.diff (Env.get_upper_bounds env s tyargs) ctx.generics_seen)
    in
    let (env, resl) = List.map_env env upper_bounds (expand ctx) in
    let res = intersect_results err resl in
    (env, update_class_name env ctx.id s res)
  | Tdependent (dep_ty, ty) ->
    let ctx =
      let base = Some (Option.value ctx.base ~default:root) in
      let allow_abstract = true in
      let abstract_as_tyvar = false in
      { ctx with base; allow_abstract; abstract_as_tyvar }
    in
    let (env, res) = expand ctx env ty in
    (env, update_class_name env ctx.id (DependentKind.to_string dep_ty) res)
  | Tintersection tyl ->
    (* Terrible hack (compatible with previous behaviour) that first attempts to project off the
     * non-type-variable conjunects. If this doesn't succeed, then try the type variable
     * conjunects, which will cause type-const constraints to be added to the type variables.
     *)
    let (tyl_vars, tyl_nonvars) =
      List.partition_tf tyl ~f:(fun t ->
          let (_env, t) = Env.expand_type env t in
          is_tyvar t)
    in
    let (env, resl) = List.map_env env tyl_nonvars (expand ctx) in
    let result = intersect_results err resl in
    begin
      match result with
      | Missing _ ->
        let (env, resl) = List.map_env env tyl_vars (expand ctx) in
        (env, intersect_results err resl)
      | _ -> (env, result)
    end
  | Tunion tyl ->
    let (env, resl) = List.map_env env tyl (expand ctx) in
    let result = union_results err resl in
    (env, result)
  | Tvar n ->
    let (env, ty) =
      Typing_subtype_tconst.get_tyvar_type_const
        env
        n
        ctx.id
        ~on_error:ctx.on_error
    in
    (env, Exact ty)
  | Tunapplied_alias _ ->
    Typing_defs.error_Tunapplied_alias_in_illegal_context ()
  | Tpu _
  | Tpu_type_access _
  | Taccess _
  | Tobject
  | Tnonnull
  | Tprim _
  | Tshape _
  | Ttuple _
  | Tvarray _
  | Tdarray _
  | Tvarray_or_darray _
  | Tfun _
  | Tdynamic
  | Toption _ ->
    (env, Missing err)

(** Expands a type constant access like A::T to its definition. *)
let expand_with_env
    (ety_env : expand_env)
    env
    ?(ignore_errors = false)
    ?(as_tyvar_with_cnstr = false)
    root
    id
    ~root_pos
    ~on_error
    ~allow_abstract_tconst =
  let (env, ty) =
    Log.log_type_access ~level:1 root id
    @@
    let ctx =
      {
        id;
        ety_env;
        base = None;
        generics_seen = TySet.empty;
        allow_abstract = allow_abstract_tconst;
        abstract_as_tyvar = as_tyvar_with_cnstr;
        on_error;
        root_pos;
      }
    in
    let (env, res) = expand ctx env root in
    type_of_result ~ignore_errors ctx env root res
  in
  (* If type constant has type this::ID and method has associated condition
     type ROOTCOND_TY for the receiver - check if condition type has type
     constant at the same path.  If yes - attach a condition type
     ROOTCOND_TY::ID to a result type *)
  match
    ( deref root,
      id,
      TR.condition_type_from_reactivity (Typing_env_types.env_reactivity env) )
  with
  | ((_, Tdependent (DTthis, _)), (_, tconst), Some cond_ty) ->
    begin
      match CT.try_get_class_for_condition_type env cond_ty with
      | Some (_, cls) when Cls.has_typeconst cls tconst ->
        let cond_ty = mk (Reason.Rwitness (fst id), Taccess (cond_ty, id)) in
        Option.value
          (TR.try_substitute_type_with_condition env cond_ty ty)
          ~default:(env, ty)
      | _ -> (env, ty)
    end
  | _ -> (env, ty)

(* This is called with non-nested type accesses e.g. this::T1::T2 is
 * represented by (this, [T1; T2])
 *)
let referenced_typeconsts env ety_env (root, ids) ~on_error =
  let (env, root) = Phase.localize ~ety_env env root in
  List.fold
    ids
    ~init:((env, root), [])
    ~f:
      begin
        fun ((env, root), acc) (pos, tconst) ->
        let (env, tyl) = Typing_utils.get_concrete_supertypes env root in
        let acc =
          List.fold tyl ~init:acc ~f:(fun acc ty ->
              let (env, ty) = Env.expand_type env ty in
              match get_node ty with
              | Tclass ((_, class_name), _, _) ->
                let ( >>= ) = Option.( >>= ) in
                Option.value
                  ~default:acc
                  ( Typing_env.get_class env class_name >>= fun class_ ->
                    Typing_env.get_typeconst env class_ tconst
                    >>= fun typeconst ->
                    Some ((typeconst.Typing_defs.ttc_origin, tconst, pos) :: acc)
                  )
              | _ -> acc)
        in
        ( expand_with_env
            ety_env
            env
            ~as_tyvar_with_cnstr:false
            root
            (pos, tconst)
            ~root_pos:(get_pos root)
            ~on_error
            ~allow_abstract_tconst:true,
          acc )
      end
  |> snd

(*****************************************************************************)
(* Exporting *)
(*****************************************************************************)

let () = Typing_utils.expand_typeconst_ref := expand_with_env
