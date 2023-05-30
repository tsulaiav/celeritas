//---------------------------------*-CUDA-*----------------------------------//
// Copyright 2023 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/track/ExtendFromPrimariesAction.cu
//---------------------------------------------------------------------------//
#include "ExtendFromPrimariesAction.hh"

#include "corecel/device_runtime_api.h"
#include "corecel/Types.hh"
#include "corecel/sys/Device.hh"
#include "corecel/sys/KernelParamCalculator.device.hh"
#include "corecel/sys/Stream.hh"
#include "celeritas/global/CoreParams.hh"
#include "celeritas/global/CoreState.hh"

#include "detail/ProcessPrimariesExecutor.hh"

namespace celeritas
{
namespace
{
//---------------------------------------------------------------------------//
// KERNELS
//---------------------------------------------------------------------------//
__global__ void
process_primaries_kernel(detail::ProcessPrimariesExecutor execute)
{
    execute(KernelParamCalculator::thread_id());
}

//---------------------------------------------------------------------------//
}  // namespace

//---------------------------------------------------------------------------//
/*!
 * Launch a kernel to create track initializers from primary particles.
 */
void ExtendFromPrimariesAction::process_primaries(
    CoreParams const&,
    CoreStateDevice& state,
    Span<Primary const> primaries) const
{
    CELER_LAUNCH_KERNEL(process_primaries,
                        celeritas::device().default_block_size(),
                        primaries.size(),
                        celeritas::device().stream(state.stream_id()).get(),
                        detail::ProcessPrimariesExecutor{
                            state.ptr(), primaries, state.counters()});
}

//---------------------------------------------------------------------------//
}  // namespace celeritas