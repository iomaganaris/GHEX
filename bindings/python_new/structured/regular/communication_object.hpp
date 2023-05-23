#pragma once
#include <ghex/communication_object.hpp>
#include <structured/types.hpp>

namespace pyghex
{
namespace structured
{
namespace regular
{
namespace
{

using communication_object_args =
    gridtools::meta::cartesian_product<types::grids, types::domain_ids>;

using communication_object_specializations =
    gridtools::meta::transform<gridtools::meta::rename<ghex::communication_object>::template apply,
        communication_object_args>;
} // namespace

} //namespace regular
} // namespace structured
} // namespace pyghex
