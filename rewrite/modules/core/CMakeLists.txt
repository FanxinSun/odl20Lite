# core — the tree's foundational types.  Header-only.
#
# A §2 ADDITION, reported rather than assumed (plan §5 constraint 6).  §2 lists
# the domain modules and odl::Result is not one; but it is linked by every one of
# them, so it has to live somewhere, and a loose header outside any target would
# be visible to everything by accident — which is precisely the property
# cmake/OdlModule.cmake exists to prevent.  Making it a module keeps the link
# boundary meaning what it says: a module sees core because it declares core.
odl_add_module(core
  DEPENDS      tl::expected
  TEST_SOURCES tests/result_tests.cpp)
