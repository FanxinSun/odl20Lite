#include <odl/gravity/degree.hpp>

#include <sstream>

namespace odl::gravity {

odl::Result<Degree, GravityError> Degree::of(int n) {
    if (n < 0 || n > kEgm2008MaxDegree) {
        std::ostringstream m;
        m << "requested degree " << n << "; the model carries degrees 0 to " << kEgm2008MaxDegree
          << ". Degree and ORDER are different limits and this is the degree one "
             "(GRAV-F-004).";
        return odl::err(GravityError{"GRAV-F-004", m.str()});
    }
    return Degree{n};
}

odl::Result<Order, GravityError> Order::of(int m) {
    if (m < 0 || m > kEgm2008MaxOrder) {
        std::ostringstream s;
        s << "requested order " << m << "; the model carries orders 0 to " << kEgm2008MaxOrder
          << ". EGM2008 is complete to degree and order 2159 and carries ADDITIONAL "
             "coefficients to degree "
          << kEgm2008MaxDegree << " at order " << kEgm2008MaxOrder
          << ", so the two limits differ by 31 and clamping one to the other would be wrong "
             "in both directions (GRAV-F-004).";
        return odl::err(GravityError{"GRAV-F-004", s.str()});
    }
    return Order{m};
}

}  // namespace odl::gravity
