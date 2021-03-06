<?hh

<<file:__EnableUnstableFeatures('expression_trees')>>

function nullable_bool(): ?bool { return null; }
function a_bool(): bool { return true; }

/**
 * Since all Hack types are truthy, typically, most syntactic places that
 * expect booleans allow all types. However, as to not leak these truthy
 * Hack semantics to Expression Trees, ensure that those syntactic positions
 * only accept booleans, rather than any truthy expression.
 */
function test(): void {
  $y = Code`
    () ==> {
      // if/else
      if (nullable_bool()) {}
      if (nullable_bool()) {} else {}

      if (a_bool()) {}
      if (a_bool()) {} else {}

      if (a_bool()) {}
      else if (nullable_bool()) {}

      if (a_bool()) {}
      else if (nullable_bool()) {}
      else {}

      if (a_bool()) {}
      else if (a_bool()) {}
      else {}

      // while() {}
      while(nullable_bool()) {}
      while(a_bool()) {}

      // for (;;) {}
      for (;;) {}

      for (;nullable_bool();) {}
      for ($i = 0; nullable_bool();) {}
      for (; nullable_bool(); $i = $i + 1) {}
      for ($i = 0; nullable_bool(); $i = $i + 1) {}

      for (;a_bool();) {}
      for ($i = 0; a_bool();) {}
      for (; a_bool(); $i = $i + 1) {}
      for ($i = 0; a_bool(); $i = $i + 1) {}

      // Boolean ||
      nullable_bool() || a_bool();
      a_bool() || nullable_bool();
      a_bool() || a_bool();

      // Boolean &&
      nullable_bool() && a_bool();
      a_bool() && nullable_bool();
      a_bool() && a_bool();

      // Boolean !
      !nullable_bool();
      !a_bool();
    }
  `;
}

//// BEGIN DEFS
// Placeholder definition so we don't get naming/typing errors.
final class Code {
  const type TAst = mixed;
  // Lifting literals.
  public static function intLiteral(int $_): ExprTree<this, this::TAst, int> {
    throw new Exception();
  }
  public static function boolLiteral(bool $_):
    ExprTree<this, this::TAst, bool>
  {
    throw new Exception();
  }
  public static function stringLiteral(string $_):
    ExprTree<this, this::TAst, string>
  {
    throw new Exception();
  }
  public static function nullLiteral(): ExprTree<this, this::TAst, null> {
    throw new Exception();
  }

  // Expressions
  public function localVar(?ExprPos $_, string $_): this::TAst {
    throw new Exception();
  }
  public function lambdaLiteral(
    ?ExprPos $_,
    vec<string> $_args,
    vec<this::TAst> $_body,
  ): this::TAst {
    throw new Exception();
  }

  // Operators
  public function plus(
    ?ExprPos $_,
    this::TAst $_,
    this::TAst $_,
  ): this::TAst {
    throw new Exception();
  }
  public function ampamp(
    ?ExprPos $_,
    this::TAst $_,
    this::TAst $_,
  ): this::TAst {
    throw new Exception();
  }
  public function barbar(
    ?ExprPos $_,
    this::TAst $_,
    this::TAst $_,
  ): this::TAst {
    throw new Exception();
  }
  public function exclamationMark(
    ?ExprPos $_,
    this::TAst $_,
  ): this::TAst {
    throw new Exception();
  }
  public function call(
    ?ExprPos $_,
    string $_fnName,
    vec<this::TAst> $_args,
  ): this::TAst {
    throw new Exception();
  }

  public function assign(
    ?ExprPos $_,
    this::TAst $_,
    this::TAst $_,
  ): this::TAst {
    throw new Exception();
  }

  // Statements.
  public function ifStatement(
    ?ExprPos $_,
    this::TAst $_cond,
    vec<this::TAst> $_then_body,
    vec<this::TAst> $_else_body,
  ): this::TAst {
    throw new Exception();
  }
  public function whileStatement(
    ?ExprPos $_,
    this::TAst $_cond,
    vec<this::TAst> $_body,
  ): this::TAst {
    throw new Exception();
  }
  public function returnStatement(
    ?ExprPos $_,
    ?this::TAst $_,
  ): this::TAst {
    throw new Exception();
  }
  public function forStatement(
    ?ExprPos $_,
    vec<this::TAst> $_,
    this::TAst $_,
    vec<this::TAst> $_,
    vec<this::TAst> $_,
  ): this::TAst {
    throw new Exception();
  }
  public function breakStatement(?ExprPos $_): this::TAst {
    throw new Exception();
  }
  public function continueStatement(?ExprPos $_,): this::TAst {
    throw new Exception();
  }

  // Splice
  public function splice<T>(
    ?ExprPos $_,
    ExprTree<this, this::TAst, T> $_,
  ): this::TAst {
    throw new Exception();
  }

  // TODO: Discard unsupported syntax nodes while lowering
  public function unsupportedSyntax(string $msg): this::TAst {
    throw new Exception($msg);
  }
}

final class ExprTree<TVisitor, TResult, TInfer>{
  public function __construct(
    private ?ExprPos $pos,
    private string $filepath,
    private (function(TVisitor): TResult) $x,
    private (function(): TInfer) $err,
  ) {}
}

final class ExprPos {
  public function __construct(
    private int $begin_line,
    private int $begin_col,
    private int $end_line,
    private int $end_col,
  ) {}
}
//// END DEFS
