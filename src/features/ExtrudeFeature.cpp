#include "ExtrudeFeature.h"

namespace features {

ExtrudeFeature::ExtrudeFeature(std::string id, ExtrudeParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

} // namespace features
