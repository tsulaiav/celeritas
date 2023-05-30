//----------------------------------*-C++-*----------------------------------//
// Copyright 2023 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/global/LaunchAction.hh
//---------------------------------------------------------------------------//
#pragma once

#include "corecel/Assert.hh"
#include "corecel/Types.hh"
#include "corecel/sys/MultiExceptionHandler.hh"
#include "corecel/sys/ThreadId.hh"

#include "ActionInterface.hh"
#include "CoreParams.hh"
#include "CoreState.hh"
#include "KernelContextException.hh"

namespace celeritas
{
//---------------------------------------------------------------------------//
/*!
 * Helper function to run an action in parallel on CPU.
 */
template<class F>
void launch_action(ExplicitActionInterface const& action,
                   Range<ThreadId> threads,
                   celeritas::CoreParams const& params,
                   celeritas::CoreState<MemSpace::host>& state,
                   F&& execute_thread)
{
    MultiExceptionHandler capture_exception;
    size_type const start{threads.begin()->unchecked_get()};
    size_type const stop{threads.end()->unchecked_get()};
#ifdef _OPENMP
#    pragma omp parallel for
#endif
    for (size_type i = start; i < stop; ++i)
    {
        CELER_TRY_HANDLE_CONTEXT(
            execute_thread(ThreadId{i}),
            capture_exception,
            KernelContextException(params.ref<MemSpace::host>(),
                                   state.ref(),
                                   ThreadId{i},
                                   action.label()));
    }
    log_and_rethrow(std::move(capture_exception));
}

//---------------------------------------------------------------------------//
/*!
 * Helper function to run an action in parallel on CPU over all states.
 */
template<class F>
void launch_action(ExplicitActionInterface const& action,
                   celeritas::CoreParams const& params,
                   celeritas::CoreState<MemSpace::host>& state,
                   F&& execute_thread)
{
    return launch_action(action,
                         Range<ThreadId>{ThreadId{0}, ThreadId{state.size()}},
                         params,
                         state,
                         std::forward<F>(execute_thread));
}

//---------------------------------------------------------------------------//
}  // namespace celeritas