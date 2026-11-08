# C4 semantic specification (Assignment 3, Project Task E)

Normative sources: \`verification/assignment/sources/E.pdf\` (Project Task E)
and the C11 Constraints and Semantics clauses it incorporates, together with
the grammar restriction in \`CD.pdf\` (Task C) and the acceptance boundary in
\`AB.pdf\` (Task A).  Requirement identifiers are referenced by
\`verification/spec/requirements.yaml\`.

Task E says: "Major parts are name and type analysis. Name analysis
associates identifiers with declarations. Type analysis associates
expressions with types. It encompasses the Constraints and Semantics
clauses."  The clauses below therefore state the C11 rule and then the
assignment-specific restriction or exemption.  C11 requirements that are not
reachable through the C4 grammar are not C4 obligations.

## S-001 — Names, scopes, and namespaces

C11 §6.2.1, §6.2.3.

- Four scope kinds exist in C11: function, file, block, function prototype.
  C4 uses all four:
  - file scope for declarations outside blocks and parameter lists;
  - block scope for declarations inside a block and for parameters of a
    function definition (the associated block is the function body);
  - function-prototype scope for parameter names in a function declaration
    that is not a definition, ending at the end of the declarator;
  - function scope for labels, which can be used anywhere in the function.
- The three C4 namespaces are separate:
  - label names;
  - struct tags (C4 has only struct, but the tag namespace is shared with the
    omitted union/enum forms);
  - members of each struct (a separate namespace per struct);
  - all other identifiers (ordinary identifiers: objects, functions,
    parameters).
- The same spelling may denote different entities in different namespaces
  without conflict.  A tag does not conflict with an ordinary identifier; a
  label does not conflict with an ordinary identifier or a tag.
- An inner declaration hides an outer declaration of the same namespace.
  Lookup in an inner scope returns the inner entity; after the inner scope
  ends the outer entity is visible again.
- The scope of an ordinary identifier begins just after the completion of its
  declarator (C11 §6.2.1p7).  The scope of a tag begins just after the tag
  appears in the type specifier.  This makes \`struct S { struct S *next; };\`
  valid: the tag is visible inside its own definition, and a pointer to the
  incomplete type is permitted.
- A parameter of a function definition has block scope and cannot be
  redeclared in the outermost block of the body (C11 §6.9.1 footnote 164);
  it can be shadowed in an enclosed block.
- A parameter name in a prototype has prototype scope and does not conflict
  with a file-scope declaration of the same spelling.
- Tags declared in a function prototype's parameter list, including tags in
  nested function declarators, also have function-prototype scope and do not
  leak into the enclosing scope.  Tags declared in a function definition's
  parameter list have block scope of the function body.  Duplicate parameter
  names in one prototype scope violate C11 §6.7p3.

## S-002 — Linkage

C11 §6.2.2.

- C4 has no \`static\`, \`extern\`, \`auto\`, or \`register\` (Task C restricts
  declaration-specifiers to type-specifier).  Consequently:
  - a file-scope object declaration has external linkage;
  - a function declaration has external linkage, whether at file scope or
    block scope (C11 §6.2.2p5);
  - a block-scope object declaration has no linkage;
  - a parameter has no linkage.
- A block-scope function declaration therefore denotes the same function
  entity as a file-scope declaration of that name; it does not introduce a
  local object.  A block-scope object of the same spelling can hide the
  function in the ordinary namespace.
- All declarations of the same external entity must have compatible types
  (C11 §6.2.7p2, §6.7p4).

## S-003 — Declarations, redeclarations, and tentative definitions

C11 §6.7, §6.9.2.

- Every declaration must declare at least one declarator, a tag, or the
  members of an enumeration (C11 §6.7p2).  In C4, \`int;\` declares nothing
  and is invalid; \`struct S;\` declares a tag and is valid; an untagged
  struct declaration with a declarator such as \`struct { int x; } s;\` is
  valid.
- At most one init-declarator is admitted by Task C, and it has no
  initializer.  Therefore every file-scope object declaration is a tentative
  definition (C11 §6.9.2p2).
- Multiple tentative definitions of the same file-scope object are permitted
  if their types are compatible (which in C4 means equal, S-007).  They
  denote one object.  If the translation unit contains no external
  definition, the object is defined with a zero initializer (C11 §6.9.2p2).
- A block-scope object declaration has no linkage and declares a new object;
  it may not be redeclared in the same scope with the same spelling
  (C11 §6.7p3).
- A function may be declared repeatedly, at file or block scope, if all
  declarations have compatible types.  A function definition is a
  declaration with a body and must be compatible with prior declarations.
- A tentative definition whose type is still incomplete at the end of the
  translation unit is rejected (C11 §6.9.2p2–p3; see ambiguities.md for the
  external-linkage interpretation).
- An object declaration with no linkage must have a complete type by the end
  of its declarator (C11 §6.7p7).  This applies to block-scope objects,
  parameters, and members.

## S-004 — Tags, struct identity, and completeness

C11 §6.7.2.1, §6.7.2.3.

- All declarations of struct type with the same tag in the same scope denote
  the same type.  A tag declared in an inner scope denotes a distinct type
  from the same spelling in an outer scope.
- An untagged struct specifier declares a distinct type each time it
  appears.
- \`struct S;\` (a declaration with no declarator) declares an incomplete tag
  if no declaration of \`S\` is visible; it is valid.
- \`struct S { ... };\` declares or completes the tag and makes the type
  complete immediately after the closing brace.
- A tag may be completed at most once (C11 §6.7.2.3p1).  A second definition
  of the same tag in the same scope is an error.
- A struct member must have a complete object type and must not have function
  type (C11 §6.7.2.1p3).  A struct may contain a pointer to itself or to
  another incomplete struct, but not an instance of itself by value.
- Member names form a namespace per struct.  Duplicate member names in one
  struct are an error.  The same member name may be used in different
  structs.
- Bit-fields and anonymous members (in the C11 §6.7.2.1p13 sense) are not
  required: Task C excludes bit-fields; Task E says anonymous structs need
  not be accepted.  Untagged struct declarations with ordinary named members
  are part of C4.
- A struct type is complete only after its definition; operations that need
  the size (object definition, member access, pointer arithmetic, \`sizeof\`)
  require completeness.

## S-005 — Declarator-derived types

C11 §6.7.6, §6.7.7; Task C restrictions.

- A declarator derives a type from the declaration specifier type:
  - \`identifier\` has the specifier type;
  - \`( declarator )\` has the type of the inner declarator;
  - \`* declarator\` is a pointer to the inner type;
  - \`declarator ( parameter-type-list )\` is a function returning the inner
    type, with the adjusted parameter types.
- Array declarators are excluded by Task C.  Arrays nevertheless arise from
  string literals: a string literal has type \`char[N]\` (including the
  terminating NUL), and the usual array-to-pointer conversion applies in
  expressions (S-008).
- A function declarator may not specify a return type that is a function or
  array type (C11 §6.7.6.3p1).
- Parameter declarations:
  - a parameter declared with function type is adjusted to pointer to
    function (C11 §6.7.6.3p8);
  - no array declarators exist in C4, so no array-parameter adjustment is
    reachable;
  - the special parameter list \`(void)\` declares no parameters; \`void\`
    must be the only parameter and must have no declarator (C11
    §6.7.6.3p10, §6.9.1p5).
- A function definition's parameters must have complete type after
  adjustment and must include identifiers, except for the \`(void)\` special
  case (C11 §6.7.6.3p4, §6.9.1p5).
- \`type-name\` uses the same restrictions for abstract declarators; it is
  used by \`sizeof(type-name)\`.

## S-006 — Type categories and canonical types

C4 has the following canonical types:

\`\`\`
void
char
int
pointer T
function T (T1, ..., Tn)
struct S            (identified by tag and scope, or by a distinct untagged declaration)
array T[N]          (only for string literals)
\`\`\`

- \`char\` and \`int\` are integer types.  \`char\` is a distinct type from
  \`int\`; its signedness is implementation-defined (see ambiguities.md).
- Pointer, function, and array types are derived types.  C4 has no
  qualifiers.
- A function type has a return type and an ordered parameter list.  There is
  no ellipsis and no old-style parameter list in C4.
- An array type has an element type and a known bound (string literals).
- Type compatibility degenerates to type equality (Task E: "Due to the
  restricted language subset, type compatibility degenerates to equality").
  Equality is structural for derived types and identity-based for struct
  types: two struct types are equal iff they denote the same tag declaration
  in the same scope, or are the same untagged declaration.

## S-007 — Type equality and completeness predicates

- \`compatible(T, U)\` is defined as \`T = U\` under S-006.
- \`complete(T)\` holds for \`char\`, \`int\`, pointers, functions, arrays with
  known bound, and struct types whose definition has been seen.  It does not
  hold for \`void\` or an incomplete struct.
- \`objectType(T)\` holds for types other than function and void.
- \`scalar(T)\` holds for integer and pointer types.
- \`arithmetic(T)\` holds for integer types.
- \`modifiableLvalue(e)\` holds for an lvalue of complete object type that is
  not an array and not a function.  C4 has no \`const\` and no bit-fields, so
  the C11 qualifier and bit-field exclusions reduce to these conditions.

## S-008 — Value categories and conversions

C11 §6.3.2, §6.3.1.

- Expression value categories are: lvalue, function designator, and
  non-lvalue (rvalue).  A void expression has no value and is represented by
  a separate \`Void\` category in the reference checker.
- Except when it is the operand of \`sizeof\`, \`&\`, or a string-literal
  initializer (not in C4), an lvalue is converted to the value stored in the
  object (lvalue conversion).  An array lvalue is converted to a pointer to
  its first element; a function designator is converted to a pointer to the
  function.
- Integer promotions: \`char\` promotes to \`int\`; \`int\` stays \`int\`.
- Usual arithmetic conversions: for two integer operands, both are promoted
  and the result type is \`int\`.
- Assignment conversions (C11 §6.5.16.1), with C4 restrictions:
  - arithmetic to arithmetic;
  - pointer to pointer when the pointed-to types are equal, or when one is
    \`void\` and the other is an object type;
  - the null pointer constant \`0\` to a pointer;
  - no struct assignment is required (Task E exemption).
- A function call argument is converted as if by assignment to the
  corresponding parameter type (C11 §6.5.2.2p7).
- A return expression is converted as if by assignment to the function
  return type (C11 §6.8.6.4p3).

## S-009 — Null pointer constant

Task E: "The only null pointer constant that you need to support is literal
0."  The reference checker treats the integer constant token \`0\` as a null
pointer constant.  It does not treat \`(0)\`, \`0 + 0\`, \`'\0'\`, or other
constant expressions as null pointer constants.  This is an interpretation
of "literal 0"; see ambiguities.md.  A null pointer constant may be assigned
to, compared with, or used in a conditional with a pointer (C11
§6.5.16.1, §6.5.9, §6.5.15).

## S-010 — Primary expressions

C11 §6.5.1.

- \`identifier\`: name lookup in the ordinary namespace.  If no declaration is
  visible, the program is rejected.  The result has the declared type and is
  an lvalue for an object or a function designator for a function.
- \`constant\`: an integer constant has type \`int\`; a character constant has
  type \`int\`.  \`0\` additionally carries the null-pointer-constant property
  (S-009).
- \`string-literal\`: type \`char[N]\`, lvalue, where \`N\` is the number of
  decoded characters plus one for the terminating NUL.  The decoded value is
  not needed for name/type analysis.
- \`( expression )\`: same type and value category as the inner expression.

## S-011 — Postfix expressions

C11 §6.5.2.

- \`E1[E2]\`: one operand must be a pointer to a complete object type and the
  other must have integer type; the result is an lvalue of the pointed-to
  type (C11 §6.5.2.1p1).  Because C4 has no array declarators, the pointer
  operand normally comes from a string literal, a pointer variable, or
  pointer arithmetic.  A pointer to \`void\` or to an incomplete struct is not
  a pointer to a complete object type.
- \`E(args)\`: the callee must have function type or pointer-to-function
  type; after function-to-pointer conversion it must be a pointer to a
  function returning \`void\` or a complete object type other than an array.
  If the function type has a prototype, the number of arguments must equal
  the number of parameters and each argument must be assignable to the
  corresponding parameter type (C11 §6.5.2.2p1–p2).  C4 has no variadic
  functions.  A call to a \`(void)\` function must have zero arguments.
- \`E.member\`: \`E\` must have struct type; the member must exist.  The result
  is an lvalue if \`E\` is an lvalue (C11 §6.5.2.3p3).
- \`E->member\`: \`E\` must be a pointer to struct; the member must exist.  The
  result is an lvalue (C11 §6.5.2.3p4).
- The result of a call returning \`void\` has void type; it may be used as an
  expression statement but not as an operand requiring a value.

## S-012 — Unary expressions

C11 §6.5.3.

- \`&E\`: \`E\` must be a function designator, the result of \`[]\` or unary
  \`*\`, or an lvalue object (C4 has no bit-fields or \`register\`).  The
  result is a pointer to the type of \`E\`.
- \`*E\`: \`E\` must have pointer type.  If it points to a function, the result
  is a function designator; otherwise it is an lvalue of the pointed-to type
  (C11 §6.5.3.2p4).
- \`-E\`: \`E\` must have arithmetic type; integer promotions apply; the result
  has the promoted type.
- \`!E\`: \`E\` must have scalar type; the result has type \`int\`.
- \`sizeof E\` and \`sizeof(type-name)\`: the operand type must not be a
  function type or an incomplete type (C11 §6.5.3.4p1); \`void\` is
  incomplete, so \`sizeof(void)\` is invalid.  The result has type \`int\`
  (Task E: "Use int for the types ptrdiff_t and size_t").  The operand
  expression is not evaluated (C11 §6.5.3.4p2), which is why Sema needs no
  expression values.
- Unary \`+\`, \`~\`, prefix \`++\`/\`--\`, and casts are outside C4 (Task C).

## S-013 — Binary arithmetic, relational, equality, and logical operators

C11 §6.5.5, §6.5.6, §6.5.8, §6.5.9, §6.5.13, §6.5.14.

- \`E1 * E2\`: both operands arithmetic; usual arithmetic conversions; result
  type \`int\` for C4's \`char\`/\`int\` operands.
- \`E1 + E2\`: either both arithmetic (usual conversions), or one pointer to
  a complete object type and the other integer (result type is the pointer
  type).  A pointer to \`void\` or incomplete struct is not a pointer to a
  complete object type.
- \`E1 - E2\`: both arithmetic; or both pointers to compatible complete object
  types (result type \`int\`, per Task E's \`ptrdiff_t = int\`); or pointer to
  complete object minus integer (result type is the pointer type).
- \`E1 < E2\`: both arithmetic, or both pointers to compatible object types;
  result \`int\`.
- \`E1 == E2\`, \`E1 != E2\`: both arithmetic; pointers to compatible types;
  pointer to object vs pointer to \`void\`; or pointer vs null pointer
  constant; result \`int\`.
- \`E1 && E2\`, \`E1 || E2\`: both operands scalar; result \`int\`.
- The other binary operators of C11 §6.5 (\`/\`, \`%\`, \`<<\`, \`>>\`, \`>\`,
  \`<=\`, \`>=\`, \`&\`, \`^\`, \`|\`) are outside C4 (Task C).

## S-014 — Conditional expression

C11 §6.5.15.

- The first operand must have scalar type.
- One of the following must hold for the second and third operands:
  both arithmetic; both the same struct type; both \`void\`; pointers to
  compatible types; one pointer and one null pointer constant; one pointer
  to object and one pointer to \`void\`.
- The result type is determined as in C11 §6.5.15p5–p6, restricted to C4
  types.  The result is not an lvalue.
- C4 has no struct assignment/return requirement, but a conditional of two
  same-struct-type operands is still a valid C expression; the reference
  checker accepts it.  Whether the implementation accepts it is optional
  under the Task E exemption.

## S-015 — Assignment

C11 §6.5.16.

- The left operand must be a modifiable lvalue (S-007).
- The right operand must be assignable to the left operand's type under
  S-008.
- The result has the type of the left operand and is not an lvalue.
- Compound assignment operators are outside C4 (Task C admits only \`=\`).
- Struct assignment is exempted by Task E: the implementation need not
  accept it.  If it does accept it, the C11 rule applies.  The reference
  checker treats struct assignment as outside the required acceptance
  domain and classifies it accordingly (see requirements.yaml).

## S-016 — Statements and control flow

C11 §6.8.

- A compound statement introduces a block scope.  Declarations and
  statements may be interleaved.
- An expression statement evaluates an expression for its side effects; a
  null statement \`;\` is valid.
- \`if (E) S\` and \`if (E) S else S\`: \`E\` must have scalar type (C11
  §6.8.4p1).  The dangling \`else\` binds to the nearest unmatched \`if\`
  (C11 §6.8.4p3); the parser/grammar must implement that resolution.
- \`while (E) S\`: \`E\` must have scalar type (C11 §6.8.5p1).
- \`break;\` may appear only in a loop (C4 has no \`switch\`); \`continue;\`
  only in a loop (C11 §6.8.6.2–p3).
- \`return E;\` may appear only in a function whose return type is not
  \`void\`, and \`E\` must be assignable to the return type.  \`return;\` may
  appear only in a function whose return type is \`void\` (C11
  §6.8.6.4p1).
- Reaching the end of a non-void function without a return is not a
  constraint violation in C11; C4 need not diagnose it.
- \`goto L;\` requires a label \`L\` defined in the same function (C11
  §6.8.6.1p1).  Labels have function scope and may be used before their
  definition.
- A label is declared by \`identifier :\` and has function scope.  Duplicate
  labels in one function are an error (C11 §6.8.1p2).  Labels are a separate
  namespace from ordinary identifiers and tags.
- Labels may appear before declarations or statements; C4's grammar admits
  \`identifier : statement\` and a declaration is not a \`statement\`, so a
  label immediately before a declaration is outside the restricted grammar.

## S-017 — Function definitions and calls

C11 §6.9.1, §6.5.2.2.

- A function definition's declarator must declare a function (C11
  §6.9.1p2).  The return type must be \`void\` or a complete object type
  other than an array (C11 §6.9.1p3).
- The parameters must have identifiers, except for the single unnamed
  \`void\` parameter (C11 §6.9.1p5).
- The parameter types must be complete after adjustment (C11 §6.7.6.3p4).
- The function body is a compound statement.  The function name has file
  scope and external linkage; a block-scope function declaration refers to
  the same entity (S-002).
- A call requires the callee to be a function or pointer to function
  returning \`void\` or a complete object type; the argument count and types
  must agree with the prototype (C11 §6.5.2.2p1–p2).
- Recursive calls, including mutual recursion, are permitted (C11
  §6.5.2.2p11).
- Functions returning a struct or taking a struct parameter need not be
  accepted (Task E).  The reference checker classifies them as exempted
  rather than as required acceptance.

## S-018 — Error locations

Task E: "For the error location use the location of the (first) terminal of
the syntactic construct where the error was detected. E.g. for adding two
pointers, show the location of the +. For an if whose condition is not
scalar, show the location of the keyword if."

The reference checker records a source location with every diagnostic.  The
conformance suite compares locations for the categories where the assignment
gives an unambiguous rule:

- binary operator error: the operator token;
- \`if\`/\`while\` condition error: the \`if\`/\`while\` keyword;
- assignment error: the assignment operator \`=\`;
- call arity/type error: the call's \`(\` or the callee?  Task E says the
  first terminal of the construct; for a call the first terminal is the
  callee expression's first token.  This is an interpretation; see
  ambiguities.md;
- declaration/type error: the first terminal of the declaration (the type
  specifier) or the declarator?  Task E says the first terminal of the
  syntactic construct; the reference checker uses the declaration's first
  token for declaration-level errors and the offending declarator/identifier
  for name errors.  Ambiguous cases are recorded rather than treated as
  defects.

## S-019 — Explicit exemptions and freedoms (Task E)

The following are explicitly not required.  A rejection is conforming; an
acceptance is allowed only if the behavior is otherwise correct C.

1. Functions returning a struct or taking a struct parameter (E.pdf).
2. Assignments of struct type (E.pdf).
3. Anonymous structs in the C11 §6.7.2.1p13 sense (unnamed members).  The
   parenthetical "struct declarations without tags are still part of our
   language subset" means untagged struct declarations with named members
   remain in C4.
4. \`__func__\` (E.pdf).
5. Evaluation of expression values during semantic analysis (E.pdf).
6. \`size_t\` and \`ptrdiff_t\` are \`int\` (E.pdf).
7. Type compatibility degenerates to equality (E.pdf).
8. The only null pointer constant that must be supported is literal \`0\`
   (E.pdf).
9. C11 requirements that are unreachable through the Task C grammar
   (arrays, qualifiers, typedef, enum, union, bit-fields, variadic
   functions, old-style definitions, etc.) are not C4 obligations.

## S-020 — Implementation interface and verification

- Sema: \`src/Sema.cpp\`, \`src/Sema.hpp\`, \`src/SemanticModel.cpp\`,
  \`src/Type.cpp\`.
- AST: \`src/AST.hpp\`, \`src/Parser.cpp\`.
- Diagnostics: \`src/Support.cpp\` (\`Diagnostics\`), \`src/main.cpp\`.
- The independent executable reference checker is
  \`verification/conformance/c4ref/sema.py\`; it consumes the AST produced by
  the reference parser.  The conformance harness compares its
  accept/reject decision and supported diagnostic locations with
  \`c4 --parse\` and \`c4 --print-ast\`.
- A finite conformance corpus is evidence of agreement, not a proof of
  correctness for all inputs.  The report states this limitation explicitly.
