#include <odl/forcemodel/earth_orientation.hpp>

namespace odl::forcemodel {

odl::Result<EarthOrientation::At, ForceModelError> EarthOrientation::at(const time::Epoch& t) const {
    eop::EopRecord record;
    if (series_) {
        auto r = series_->at(t, policy_);
        if (!r) return odl::err(ForceModelError{r.error().id, r.error().message});
        record = *r;
    } else {
        record = *fixed_;
    }
    auto rot = frames::gcrs_to_itrs(t, record, *leaps_);
    if (!rot) return odl::err(ForceModelError{rot.error().id, rot.error().message});
    return At{record, *rot};
}

}  // namespace odl::forcemodel
