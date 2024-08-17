//----------------------------------*-C++-*----------------------------------//
// Copyright 2023-2024 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file geocel/GeoParamsOutput.cc
//---------------------------------------------------------------------------//
#include "GeoParamsOutput.hh"

#include <utility>
#include <nlohmann/json.hpp>

#include "corecel/Config.hh"

#include "corecel/Assert.hh"
#include "corecel/cont/Range.hh"
#include "corecel/io/JsonPimpl.hh"
#include "corecel/io/LabelIO.json.hh"

#include "BoundingBoxIO.json.hh"
#include "GeoParamsInterface.hh"

namespace celeritas
{
//---------------------------------------------------------------------------//
/*!
 * Construct from shared geometry data.
 */
GeoParamsOutput::GeoParamsOutput(SPConstGeoParams geo) : geo_(std::move(geo))
{
    CELER_EXPECT(geo_);
}

//---------------------------------------------------------------------------//
/*!
 * Write output to the given JSON object.
 */
void GeoParamsOutput::output(JsonPimpl* j) const
{
    using json = nlohmann::json;

    auto obj = json::object();

    obj["supports_safety"] = geo_->supports_safety();
    obj["bbox"] = geo_->bbox();

    // Save volume names
    {
        auto label = json::array();

        for (auto id : range(VolumeId{geo_->num_volumes()}))
        {
            label.push_back(geo_->id_to_label(id));
        }
        obj["volumes"] = {
            {"label", std::move(label)},
        };
    }

    // Save surface names
    if (auto* surf_geo
        = dynamic_cast<GeoParamsSurfaceInterface const*>(geo_.get()))
    {
        auto label = json::array();

        for (auto id : range(SurfaceId{surf_geo->num_surfaces()}))
        {
            label.push_back(surf_geo->id_to_label(id));
        }
        obj["surfaces"] = {
            {"label", std::move(label)},
        };
    }

    j->obj = std::move(obj);
}

//---------------------------------------------------------------------------//
}  // namespace celeritas