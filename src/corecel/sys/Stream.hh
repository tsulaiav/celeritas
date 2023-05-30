//----------------------------------*-C++-*----------------------------------//
// Copyright 2023 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file corecel/sys/Stream.hh
//---------------------------------------------------------------------------//
#pragma once

#include "corecel/device_runtime_api.h"
#include "corecel/Assert.hh"
#include "corecel/Types.hh"

namespace celeritas
{
//---------------------------------------------------------------------------//
#if !CELER_USE_DEVICE
struct MockStream_st;
#endif

//---------------------------------------------------------------------------//
/*!
 * CUDA or HIP stream.
 *
 * This creates/destroys a stream on construction/destruction.
 */
class Stream
{
  public:
    //!@{
    //! \name Type aliases
#if CELER_USE_DEVICE
    using StreamT = CELER_DEVICE_PREFIX(Stream_t);
#else
    using StreamT = MockStream_st*;
#endif
    //!@}

  public:
    // Construct by creating a stream
    Stream();

    // Construct with the default stream
    Stream(std::nullptr_t) {}

    // Destroy the stream
    ~Stream();

    // Move construct and assign
    Stream(Stream const&) = delete;
    Stream& operator=(Stream const&) = delete;
    Stream(Stream&&) noexcept;
    Stream& operator=(Stream&&) noexcept;
    void swap(Stream& other) noexcept;

    // Access the stream
    StreamT get() const { return stream_; }

  private:
    StreamT stream_{nullptr};
};

//---------------------------------------------------------------------------//
}  // namespace celeritas