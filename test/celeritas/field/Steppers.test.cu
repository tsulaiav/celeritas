//---------------------------------*-CUDA-*----------------------------------//
// Copyright 2020-2022 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/field/Steppers.test.cu
//---------------------------------------------------------------------------//
#include "Steppers.test.hh"

#include <thrust/device_vector.h>

#include "corecel/Types.hh"
#include "corecel/cont/Range.hh"
#include "corecel/sys/KernelParamCalculator.device.hh"
#include "celeritas/Constants.hh"
#include "celeritas/Quantities.hh"
#include "celeritas/Units.hh"
#include "celeritas/field/DormandPrinceStepper.hh"
#include "celeritas/field/MagFieldEquation.hh"
#include "celeritas/field/RungeKuttaStepper.hh"
#include "celeritas/field/Types.hh"
#include "celeritas/field/UniformField.hh"
#include "celeritas/field/UniformZField.hh"
#include "celeritas/field/ZHelixStepper.hh"

using celeritas::detail::truncation_error;
using thrust::raw_pointer_cast;
using namespace celeritas;

namespace celeritas_test
{
//---------------------------------------------------------------------------//
// HELP FUNCTIONS
//---------------------------------------------------------------------------//
template<class FieldT, template<class> class StepperT>
__device__ inline void gpu_stepper(celeritas_test::FieldTestParams param,
                                   const FieldT&                   field,
                                   real_type*                      pos_x,
                                   real_type*                      pos_z,
                                   real_type*                      mom_y,
                                   real_type*                      mom_z,
                                   real_type*                      error)
{
    auto tid = celeritas::KernelParamCalculator::thread_id();
    if (tid.get() >= param.nstates)
        return;

    // Construct a StepperT for testing
    auto advance_step
        = make_mag_field_stepper<StepperT>(field, units::ElementaryCharge{-1});

    // Initial state and the epected state after revolutions
    OdeState y;
    y.pos = {param.radius, 0.0, tid.get() * 1.0e-6};
    y.mom = {0.0, param.momentum_y, param.momentum_z};

    // Test parameters and the sub-step size
    real_type hstep       = 2.0 * constants::pi * param.radius / param.nsteps;
    real_type total_error = 0;

    for (auto nr : range(param.revolutions))
    {
        // Travel hstep for nsteps times in the field
        for (CELER_MAYBE_UNUSED int i : celeritas::range(param.nsteps))
        {
            StepperResult result = advance_step(hstep, y);
            y                    = result.end_state;
            total_error += truncation_error(hstep, 0.001, y, result.err_state);
        }
    }
    // Output for verification
    pos_x[tid.get()] = y.pos[0];
    pos_z[tid.get()] = y.pos[2];
    mom_y[tid.get()] = y.mom[1];
    mom_z[tid.get()] = y.mom[2];
    error[tid.get()] = total_error;
}

//---------------------------------------------------------------------------//
// KERNELS
//---------------------------------------------------------------------------//
__global__ void helix_test_kernel(FieldTestParams param,
                                  real_type*      pos_x,
                                  real_type*      pos_z,
                                  real_type*      mom_y,
                                  real_type*      mom_z,
                                  real_type*      error)
{
    UniformZField field(param.field_value);

    gpu_stepper<UniformZField, ZHelixStepper>(
        param, field, pos_x, pos_z, mom_y, mom_z, error);
}

//---------------------------------------------------------------------------//
__global__ void rk4_test_kernel(FieldTestParams param,
                                real_type*      pos_x,
                                real_type*      pos_z,
                                real_type*      mom_y,
                                real_type*      mom_z,
                                real_type*      error)
{
    UniformField field({0, 0, param.field_value});

    gpu_stepper<UniformField, RungeKuttaStepper>(
        param, field, pos_x, pos_z, mom_y, mom_z, error);
}

//---------------------------------------------------------------------------//
__global__ void dp547_test_kernel(FieldTestParams param,
                                  real_type*      pos_x,
                                  real_type*      pos_z,
                                  real_type*      mom_y,
                                  real_type*      mom_z,
                                  real_type*      error)
{
    UniformField field({0, 0, param.field_value});

    gpu_stepper<UniformField, DormandPrinceStepper>(
        param, field, pos_x, pos_z, mom_y, mom_z, error);
}

//---------------------------------------------------------------------------//
// HELP FUNCTIONS
//---------------------------------------------------------------------------//
using dvec = thrust::device_vector<celeritas::real_type>;

inline StepperTestOutput
copy_to_cpu(dvec pos_x, dvec pos_z, dvec mom_y, dvec mom_z, dvec error)
{
    // Copy result back to CPU
    StepperTestOutput result;

    result.pos_x.resize(pos_x.size());
    thrust::copy(pos_x.begin(), pos_x.end(), result.pos_x.begin());

    result.pos_z.resize(pos_z.size());
    thrust::copy(pos_z.begin(), pos_z.end(), result.pos_z.begin());

    result.mom_y.resize(mom_y.size());
    thrust::copy(mom_y.begin(), mom_y.end(), result.mom_y.begin());

    result.mom_z.resize(mom_z.size());
    thrust::copy(mom_z.begin(), mom_z.end(), result.mom_z.begin());

    result.error.resize(error.size());
    thrust::copy(error.begin(), error.end(), result.error.begin());

    return result;
}

//---------------------------------------------------------------------------//
// TESTING INTERFACE
//---------------------------------------------------------------------------//
//! Run the ZHelix stepper on device and return results
StepperTestOutput helix_test(FieldTestParams test_param)
{
    // Output data for kernel
    thrust::device_vector<real_type> pos_x(test_param.nstates, 0.0);
    thrust::device_vector<real_type> pos_z(test_param.nstates, 0.0);
    thrust::device_vector<real_type> mom_y(test_param.nstates, 0.0);
    thrust::device_vector<real_type> mom_z(test_param.nstates, 0.0);
    thrust::device_vector<real_type> error(test_param.nstates, 0.0);

    // Run kernel
    celeritas::KernelParamCalculator calc_launch_params(helix_test_kernel,
                                                        "helix_test");
    auto params = calc_launch_params(test_param.nstates);

    helix_test_kernel<<<params.blocks_per_grid, params.threads_per_block>>>(
        test_param,
        raw_pointer_cast(pos_x.data()),
        raw_pointer_cast(pos_z.data()),
        raw_pointer_cast(mom_y.data()),
        raw_pointer_cast(mom_z.data()),
        raw_pointer_cast(error.data()));
    CELER_DEVICE_CALL_PREFIX(DeviceSynchronize());

    // Copy result back to CPU
    return copy_to_cpu(pos_x, pos_z, mom_y, mom_z, error);
}

//! Run the classical Runge-Kutta stepper on device and return results
StepperTestOutput rk4_test(FieldTestParams test_param)
{
    // Output data for kernel
    thrust::device_vector<real_type> pos_x(test_param.nstates, 0.0);
    thrust::device_vector<real_type> pos_z(test_param.nstates, 0.0);
    thrust::device_vector<real_type> mom_y(test_param.nstates, 0.0);
    thrust::device_vector<real_type> mom_z(test_param.nstates, 0.0);
    thrust::device_vector<real_type> error(test_param.nstates, 0.0);

    // Run kernel
    celeritas::KernelParamCalculator calc_launch_params(rk4_test_kernel,
                                                        "rk4_test");
    auto params = calc_launch_params(test_param.nstates);

    rk4_test_kernel<<<params.blocks_per_grid, params.threads_per_block>>>(
        test_param,
        raw_pointer_cast(pos_x.data()),
        raw_pointer_cast(pos_z.data()),
        raw_pointer_cast(mom_y.data()),
        raw_pointer_cast(mom_z.data()),
        raw_pointer_cast(error.data()));
    CELER_DEVICE_CALL_PREFIX(DeviceSynchronize());

    // Copy result back to CPU
    return copy_to_cpu(pos_x, pos_z, mom_y, mom_z, error);
}

//---------------------------------------------------------------------------//
//! Run the DormandPrince547 stepper on device and return results
StepperTestOutput dp547_test(FieldTestParams test_param)
{
    // Output data for kernel
    thrust::device_vector<real_type> pos_x(test_param.nstates, 0.0);
    thrust::device_vector<real_type> pos_z(test_param.nstates, 0.0);
    thrust::device_vector<real_type> mom_y(test_param.nstates, 0.0);
    thrust::device_vector<real_type> mom_z(test_param.nstates, 0.0);
    thrust::device_vector<real_type> error(test_param.nstates, 0.0);

    // Run kernel
    celeritas::KernelParamCalculator calc_launch_params(dp547_test_kernel,
                                                        "dp547_test");
    auto params = calc_launch_params(test_param.nstates);

    dp547_test_kernel<<<params.blocks_per_grid, params.threads_per_block>>>(
        test_param,
        raw_pointer_cast(pos_x.data()),
        raw_pointer_cast(pos_z.data()),
        raw_pointer_cast(mom_y.data()),
        raw_pointer_cast(mom_z.data()),
        raw_pointer_cast(error.data()));
    CELER_DEVICE_CALL_PREFIX(DeviceSynchronize());

    // Copy result back to CPU
    return copy_to_cpu(pos_x, pos_z, mom_y, mom_z, error);
}

//---------------------------------------------------------------------------//
} // namespace celeritas_test