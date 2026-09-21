# macromodel — the satellite macromodel schema.  Header-only, on purpose:
# L4 step 2's review moved srp_force (the one thing that was not schema) out
# to odl::srp_analytic, and what remained -- Cited<T>, BodyDirection,
# FlatSurface, SphericalSurface, Macromodel, MacromodelBuilder -- is all
# construction and validation, none of it needing a translation unit of its
# own.  A consumer that only wants the schema (L5's own population code)
# links no force law, which is the whole point of the split.
odl_add_module(macromodel
  DEPENDS      odl::core
  TEST_SOURCES tests/macromodel_tests.cpp)
