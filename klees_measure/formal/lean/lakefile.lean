import Lake
open Lake DSL

package "kmp" where
  version := v!"0.1.0"

@[default_target]
lean_lib KMP_CleanCell where
  roots := #[`KMP_CleanCell]

@[default_target]
lean_lib KMP_Hypergraph where
  roots := #[`KMP_Hypergraph]
