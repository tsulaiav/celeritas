//----------------------------------*-C++-*----------------------------------//
// Copyright 2023 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/global/KernelContextException.hh
//---------------------------------------------------------------------------//
#pragma once

#include "corecel/Assert.hh"
#include "celeritas/Quantities.hh"
#include "celeritas/Types.hh"

#include "CoreTrackDataFwd.hh"

namespace celeritas
{
class CoreTrackView;
//---------------------------------------------------------------------------//
/*!
 * Provide contextual information about failed errors on CPU.
 *
 * When a CPU track hits an exception, gather properties about the current
 * thread and failing track. These properties are accessible through this
 * exception class *or* they can be chained into the failing exception and
 * processed by \c ExceptionOutput as context for the failure.
 *
 * \code
    CELER_TRY_HANDLE_CONTEXT(
        launch(ThreadId{i}),
        capture_exception,
        KernelContextException(data, ThreadId{i}, this->label())
    );
 * \endcode
 */
class KernelContextException : public RichContextException
{
  public:
    //!@{
    //! \name Type aliases
    using CoreHostRef = CoreRef<MemSpace::host>;
    using Energy = units::MevEnergy;
    //!@}

  public:
    // Construct with track data and kernel label
    KernelContextException(CoreHostRef const& data,
                           ThreadId tid,
                           std::string&& label);

    // This class type
    char const* type() const final;

    // Save context to a JSON object
    void output(JsonPimpl* json) const final;

    //! Get an explanatory message
    char const* what() const noexcept final { return what_.c_str(); }

    //!@{
    //! \name Track accessors
    //! Thread slot ID
    ThreadId thread() const { return thread_; }
    //! Event ID
    EventId event() const { return event_; }
    //! Track ID
    TrackId track() const { return track_; }
    //! Parent track ID
    TrackId parent() const { return parent_; }
    //! Step counter
    size_type num_steps() const { return num_steps_; }
    //! Particle type
    ParticleId particle() const { return particle_; }
    //! Particle energy
    Energy energy() const { return energy_; }
    //! Position
    Real3 const& pos() const { return pos_; }
    //! Direction
    Real3 const& dir() const { return dir_; }
    //! Volume ID
    VolumeId volume() const { return volume_; }
    //! Surface
    SurfaceId surface() const { return surface_; }
    //! Next surface
    SurfaceId next_surface() const { return next_surface_; }
    //!@}

    //! Label of the kernel that died
    std::string const& label() const { return label_; }

  private:
    ThreadId thread_;
    EventId event_;
    TrackId track_;
    TrackId parent_;
    size_type num_steps_;
    ParticleId particle_;
    Energy energy_;
    Real3 pos_;
    Real3 dir_;
    VolumeId volume_;
    SurfaceId surface_;
    SurfaceId next_surface_;

    std::string label_;
    std::string what_;

    // Populate properties during construction
    void initialize(CoreTrackView const& core);
};

//---------------------------------------------------------------------------//
}  // namespace celeritas