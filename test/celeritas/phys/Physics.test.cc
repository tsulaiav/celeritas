//----------------------------------*-C++-*----------------------------------//
// Copyright 2021-2022 UT-Battelle, LLC, and other Celeritas developers.
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//---------------------------------------------------------------------------//
//! \file celeritas/phys/Physics.test.cc
//---------------------------------------------------------------------------//
#include "Physics.test.hh"

#include "corecel/cont/Range.hh"
#include "corecel/data/CollectionStateStore.hh"
#include "celeritas/MockTestBase.hh"
#include "celeritas/em/process/EPlusAnnihilationProcess.hh"
#include "celeritas/grid/EnergyLossCalculator.hh"
#include "celeritas/grid/RangeCalculator.hh"
#include "celeritas/grid/XsCalculator.hh"
#include "celeritas/mat/MaterialParams.hh"
#include "celeritas/phys/ParticleParams.hh"
#include "celeritas/phys/PhysicsParams.hh"
#include "celeritas/phys/PhysicsParamsOutput.hh"
#include "celeritas/phys/PhysicsTrackView.hh"

#include "DiagnosticRngEngine.hh"
#include "celeritas_test.hh"

using namespace celeritas;
using namespace celeritas_test;
using MevEnergy = celeritas::units::MevEnergy;

//---------------------------------------------------------------------------//
// PHYSICS BASE CLASS
//---------------------------------------------------------------------------//

class PhysicsParamsTest : public MockTestBase
{
  public:
    const SPConstParticle& particles() { return MockTestBase::particle(); }
};

TEST_F(PhysicsParamsTest, accessors)
{
    const PhysicsParams& p = *this->physics();

    EXPECT_EQ(6, p.num_processes());
    EXPECT_EQ(2 + 1 + 3 + 2 + 2 + 1, p.num_models());
    EXPECT_EQ(3, p.max_particle_processes());

    // Test process names after construction
    std::vector<std::string> process_names;
    for (auto process_id : range(ProcessId{p.num_processes()}))
    {
        process_names.push_back(p.process(process_id)->label());
    }
    const std::string expected_process_names[]
        = {"scattering", "absorption", "purrs", "hisses", "meows", "barks"};
    EXPECT_VEC_EQ(expected_process_names, process_names);

    // Test model names after construction
    std::vector<std::string> model_names;
    std::vector<std::string> model_desc;
    for (auto model_id : range(ModelId{p.num_models()}))
    {
        const Model& m = *p.model(model_id);
        model_names.push_back(m.label());
        model_desc.push_back(m.description());
    }

    static const std::string expected_model_names[] = {"mock-model-4",
                                                       "mock-model-5",
                                                       "mock-model-6",
                                                       "mock-model-7",
                                                       "mock-model-8",
                                                       "mock-model-9",
                                                       "mock-model-10",
                                                       "mock-model-11",
                                                       "mock-model-12",
                                                       "mock-model-13",
                                                       "mock-model-14"};
    EXPECT_VEC_EQ(expected_model_names, model_names);

    static const std::string expected_model_desc[]
        = {"MockModel(4, p=0, emin=1e-06, emax=100)",
           "MockModel(5, p=1, emin=1, emax=100)",
           "MockModel(6, p=0, emin=1e-06, emax=100)",
           "MockModel(7, p=1, emin=0.001, emax=1)",
           "MockModel(8, p=1, emin=1, emax=10)",
           "MockModel(9, p=1, emin=10, emax=100)",
           "MockModel(10, p=2, emin=0.001, emax=1)",
           "MockModel(11, p=2, emin=1, emax=100)",
           "MockModel(12, p=1, emin=0.001, emax=10)",
           "MockModel(13, p=2, emin=0.001, emax=10)",
           "MockModel(14, p=3, emin=1e-05, emax=10)"};
    EXPECT_VEC_EQ(expected_model_desc, model_desc);
    PRINT_EXPECTED(model_names);

    // Test host-accessible process map
    std::vector<std::string> process_map;
    for (auto particle_id : range(ParticleId{this->particles()->size()}))
    {
        std::string prefix = this->particles()->id_to_label(particle_id);
        prefix.push_back(':');
        for (ProcessId process_id : p.processes(particle_id))
        {
            process_map.push_back(prefix + process_names[process_id.get()]);
        }
    }
    const std::string expected_process_map[] = {"gamma:scattering",
                                                "gamma:absorption",
                                                "celeriton:scattering",
                                                "celeriton:purrs",
                                                "celeriton:meows",
                                                "anti-celeriton:hisses",
                                                "anti-celeriton:meows",
                                                "electron:barks"};
    EXPECT_VEC_EQ(expected_process_map, process_map);
}

TEST_F(PhysicsParamsTest, output)
{
    PhysicsParamsOutput out(this->physics());
    EXPECT_EQ("physics", out.label());

    if (CELERITAS_USE_JSON)
    {
        EXPECT_EQ(
            R"json({"models":[{"label":"mock-model-4","process":0},{"label":"mock-model-5","process":0},{"label":"mock-model-6","process":1},{"label":"mock-model-7","process":2},{"label":"mock-model-8","process":2},{"label":"mock-model-9","process":2},{"label":"mock-model-10","process":3},{"label":"mock-model-11","process":3},{"label":"mock-model-12","process":4},{"label":"mock-model-13","process":4},{"label":"mock-model-14","process":5}],"options":{"eloss_calc_limit":[0.001,"MeV"],"energy_fraction":0.8,"fixed_step_limiter":0.0,"linear_loss_limit":0.01,"scaling_fraction":0.2,"scaling_min_range":0.1},"processes":[{"label":"scattering"},{"label":"absorption"},{"label":"purrs"},{"label":"hisses"},{"label":"meows"},{"label":"barks"}],"sizes":{"integral_xs":8,"model_groups":8,"model_ids":11,"process_groups":4,"process_ids":8,"reals":196,"value_grid_ids":75,"value_grids":75,"value_tables":43}})json",
            to_string(out));
    }
}

//---------------------------------------------------------------------------//
// PHYSICS TRACK VIEW (HOST)
//---------------------------------------------------------------------------//

class PhysicsTrackViewHostTest : public PhysicsParamsTest
{
    using Base         = PhysicsParamsTest;
    using RandomEngine = DiagnosticRngEngine<std::mt19937>;

  public:
    //!@{
    //! Type aliases
    using StateStore = CollectionStateStore<PhysicsStateData, MemSpace::host>;
    using ParamsHostRef = ::celeritas::HostCRef<PhysicsParamsData>;
    //!@}

    void SetUp() override
    {
        Base::SetUp();

        // Make one state per particle
        auto state_size = this->particles()->size();

        CELER_ASSERT(this->physics());
        params_ref = this->physics()->host_ref();
        state      = StateStore(*this->physics(), state_size);

        // Clear secondary data (done in pre-step kernel)
        {
            StackAllocator<Secondary> allocate(state.ref().secondaries);
            allocate.clear();
        }

        // Clear out energy deposition and secondary pointers (done in pre-step
        // kernel)
        for (auto tid : range(ThreadId(state_size)))
        {
            auto step = this->make_step_view(tid);
            step.reset_energy_deposition();
            step.secondaries({});
        }

        // Save mapping of process label -> ID
        for (auto id : range(ProcessId{this->physics()->num_processes()}))
        {
            process_names[this->physics()->process(id)->label()] = id;
        }
    }

    PhysicsTrackView make_track_view(const char* particle, MaterialId mid)
    {
        CELER_EXPECT(particle && mid);

        auto pid = this->particles()->find(particle);
        CELER_ASSERT(pid);
        CELER_ASSERT(pid.get() < state.size());

        ThreadId tid((pid.get() + 1) % state.size());

        // Construct (thread depends on particle here to shake things up) and
        // initialize
        PhysicsTrackView phys(params_ref, state.ref(), pid, mid, tid);
        phys = PhysicsTrackInitializer{};

        return phys;
    }

    PhysicsStepView make_step_view(ThreadId tid)
    {
        CELER_EXPECT(tid < state.size());
        return PhysicsStepView(params_ref, state.ref(), tid);
    }

    PhysicsStepView make_step_view(const char* particle)
    {
        auto pid = this->particles()->find(particle);
        CELER_ASSERT(pid);
        CELER_ASSERT(pid.get() < state.size());

        ThreadId tid((pid.get() + 1) % state.size());
        return this->make_step_view(tid);
    }

    ParticleProcessId
    find_ppid(const PhysicsTrackView& track, const char* label) const
    {
        auto iter = process_names.find(label);
        CELER_VALIDATE(iter != process_names.end(),
                       << "No process named " << label);
        ProcessId pid = iter->second;
        for (auto pp_id :
             range(ParticleProcessId{track.num_particle_processes()}))
        {
            if (track.process(pp_id) == pid)
                return pp_id;
        }
        return {};
    }

    RandomEngine& rng() { return rng_; }

    ParamsHostRef                    params_ref;
    StateStore                       state;
    std::map<std::string, ProcessId> process_names;
    RandomEngine                     rng_;
};

TEST_F(PhysicsTrackViewHostTest, track_view)
{
    PhysicsTrackView gamma = this->make_track_view("gamma", MaterialId{0});
    PhysicsTrackView celer = this->make_track_view("celeriton", MaterialId{1});
    const PhysicsTrackView& gamma_cref = gamma;

    // Interaction MFP
    {
        EXPECT_FALSE(gamma_cref.has_interaction_mfp());

        gamma.interaction_mfp(1.234);
        celer.interaction_mfp(2.345);
        EXPECT_DOUBLE_EQ(1.234, gamma_cref.interaction_mfp());
        EXPECT_DOUBLE_EQ(2.345, celer.interaction_mfp());
    }

    // Model/action ID conversion
    for (ModelId m : range(ModelId{this->physics()->num_models()}))
    {
        ActionId a = gamma_cref.model_to_action(m);
        EXPECT_EQ(m.unchecked_get(),
                  gamma_cref.action_to_model(a).unchecked_get());
    }
}

TEST_F(PhysicsTrackViewHostTest, step_view)
{
    PhysicsStepView        gamma      = this->make_step_view(ThreadId{0});
    PhysicsStepView        celer      = this->make_step_view(ThreadId{1});
    const PhysicsStepView& gamma_cref = gamma;

    // Cross sections
    {
        gamma.per_process_xs(ParticleProcessId{0}) = 1.2;
        gamma.per_process_xs(ParticleProcessId{1}) = 10.0;
        celer.per_process_xs(ParticleProcessId{0}) = 100.0;
        EXPECT_DOUBLE_EQ(1.2, gamma_cref.per_process_xs(ParticleProcessId{0}));
        EXPECT_DOUBLE_EQ(10.0, gamma_cref.per_process_xs(ParticleProcessId{1}));
        EXPECT_DOUBLE_EQ(100.0, celer.per_process_xs(ParticleProcessId{0}));
    }

    // Energy deposition
    {
        using Energy = PhysicsTrackView::Energy;
        gamma.reset_energy_deposition();
        gamma.deposit_energy(Energy(2.5));
        EXPECT_DOUBLE_EQ(2.5, value_as<Energy>(gamma_cref.energy_deposition()));
        // Allow zero-energy deposition
        EXPECT_NO_THROW(gamma.deposit_energy(zero_quantity()));
        EXPECT_DOUBLE_EQ(2.5, value_as<Energy>(gamma_cref.energy_deposition()));
        gamma.reset_energy_deposition();
        EXPECT_DOUBLE_EQ(0.0, value_as<Energy>(gamma_cref.energy_deposition()));
    }

    // Secondaries
    {
        EXPECT_EQ(0, gamma_cref.secondaries().size());
        std::vector<Secondary> temp(3);
        gamma.secondaries(make_span(temp));
        auto actual = gamma_cref.secondaries();
        EXPECT_EQ(3, actual.size());
        EXPECT_EQ(temp.data(), actual.data());
    }
}

TEST_F(PhysicsTrackViewHostTest, processes)
{
    // Gamma
    {
        const PhysicsTrackView phys
            = this->make_track_view("gamma", MaterialId{0});

        EXPECT_EQ(2, phys.num_particle_processes());
        const ParticleProcessId scat_ppid{0};
        const ParticleProcessId abs_ppid{1};
        EXPECT_EQ(ProcessId{0}, phys.process(scat_ppid));
        EXPECT_EQ(ProcessId{1}, phys.process(abs_ppid));
        EXPECT_TRUE(phys.has_at_rest());
    }

    // Celeriton
    {
        const PhysicsTrackView phys
            = this->make_track_view("celeriton", MaterialId{0});

        EXPECT_EQ(3, phys.num_particle_processes());
        const ParticleProcessId scat_ppid{0};
        const ParticleProcessId purr_ppid{1};
        const ParticleProcessId meow_ppid{2};
        EXPECT_EQ(ProcessId{0}, phys.process(scat_ppid));
        EXPECT_EQ(ProcessId{2}, phys.process(purr_ppid));
        EXPECT_EQ(ProcessId{4}, phys.process(meow_ppid));
        EXPECT_TRUE(phys.has_at_rest());
    }

    // Anti-celeriton
    {
        const PhysicsTrackView phys
            = this->make_track_view("anti-celeriton", MaterialId{1});

        EXPECT_EQ(2, phys.num_particle_processes());
        const ParticleProcessId hiss_ppid{0};
        const ParticleProcessId meow_ppid{1};
        EXPECT_EQ(ProcessId{3}, phys.process(hiss_ppid));
        EXPECT_EQ(ProcessId{4}, phys.process(meow_ppid));
        EXPECT_TRUE(phys.has_at_rest());
    }

    // Electron
    {
        // No at-rest interaction
        const PhysicsTrackView phys
            = this->make_track_view("electron", MaterialId{1});
        EXPECT_FALSE(phys.has_at_rest());
    }
}

TEST_F(PhysicsTrackViewHostTest, value_grids)
{
    std::vector<int> grid_ids;

    for (const char* particle : {"gamma", "celeriton", "anti-celeriton"})
    {
        for (auto mat_id : range(MaterialId{this->material()->size()}))
        {
            const PhysicsTrackView phys
                = this->make_track_view(particle, mat_id);

            for (auto pp_id :
                 range(ParticleProcessId{phys.num_particle_processes()}))
            {
                for (ValueGridType vgt : range(ValueGridType::size_))
                {
                    auto id = phys.value_grid(vgt, pp_id);
                    grid_ids.push_back(id ? id.get() : -1);
                }
            }
        }
    }

    // Grid IDs should be unique if they exist. Gammas should have fewer
    // because there aren't any slowing down/range limiters.
    static const int expected_grid_ids[]
        = {0,  -1, -1, -1, 3,  -1, -1, -1, 1,  -1, -1, -1, 4,  -1, -1, -1, 2,
           -1, -1, -1, 5,  -1, -1, -1, 6,  -1, -1, -1, 9,  10, 11, -1, 18, -1,
           -1, -1, 7,  -1, -1, -1, 12, 13, 14, -1, 19, -1, -1, -1, 8,  -1, -1,
           -1, 15, 16, 17, -1, 20, -1, -1, -1, 21, 22, 23, -1, 30, -1, -1, -1,
           24, 25, 26, -1, 31, -1, -1, -1, 27, 28, 29, -1, 32, -1, -1, -1};
    EXPECT_VEC_EQ(expected_grid_ids, grid_ids);
}

TEST_F(PhysicsTrackViewHostTest, calc_xs)
{
    // Cross sections: same across particle types, constant in energy, scale
    // according to material number density
    std::vector<real_type> xs;
    for (const char* particle : {"gamma", "celeriton"})
    {
        for (auto mat_id : range(MaterialId{this->material()->size()}))
        {
            const PhysicsTrackView phys
                = this->make_track_view(particle, mat_id);
            auto scat_ppid = this->find_ppid(phys, "scattering");
            auto id = phys.value_grid(ValueGridType::macro_xs, scat_ppid);
            ASSERT_TRUE(id);
            auto calc_xs = phys.make_calculator<XsCalculator>(id);
            xs.push_back(calc_xs(MevEnergy{1.0}));
        }
    }

    const double expected_xs[] = {0.0001, 0.001, 0.1, 0.0001, 0.001, 0.1};
    EXPECT_VEC_SOFT_EQ(expected_xs, xs);
}

TEST_F(PhysicsTrackViewHostTest, calc_eloss_range)
{
    // Default range and scaling
    EXPECT_SOFT_EQ(0.1 * units::centimeter,
                   params_ref.scalars.scaling_min_range);
    EXPECT_SOFT_EQ(0.2, params_ref.scalars.scaling_fraction);
    std::vector<real_type> eloss;
    std::vector<real_type> range;
    std::vector<real_type> step;

    // Range: increases with energy, constant with material. Stopped particle
    // range and step will be zero.
    for (const char* particle : {"celeriton", "anti-celeriton"})
    {
        const PhysicsTrackView phys
            = this->make_track_view(particle, MaterialId{0});
        auto ppid = phys.eloss_ppid();
        ASSERT_TRUE(ppid);

        auto eloss_id = phys.value_grid(ValueGridType::energy_loss, ppid);
        ASSERT_TRUE(eloss_id);
        auto calc_eloss = phys.make_calculator<EnergyLossCalculator>(eloss_id);

        auto range_id = phys.value_grid(ValueGridType::range, ppid);
        ASSERT_TRUE(range_id);
        auto calc_range = phys.make_calculator<RangeCalculator>(range_id);
        for (real_type energy : {1e-6, 0.01, 1.0, 1e2})
        {
            eloss.push_back(calc_eloss(MevEnergy{energy}));
            range.push_back(calc_range(MevEnergy{energy}));
            step.push_back(phys.range_to_step(range.back()));
        }
    }

    static const double expected_eloss[]
        = {0.6, 0.6, 0.6, 0.6, 0.7, 0.7, 0.7, 0.7};
    static const double expected_range[] = {5.2704627669473e-05,
                                            0.016666666666667,
                                            1.6666666666667,
                                            166.66666666667,
                                            4.5175395145263e-05,
                                            0.014285714285714,
                                            1.4285714285714,
                                            142.85714285714};
    static const double expected_step[]  = {5.2704627669473e-05,
                                           0.016666666666667,
                                           0.48853333333333,
                                           33.493285333333,
                                           4.5175395145263e-05,
                                           0.014285714285714,
                                           0.44011428571429,
                                           28.731372571429};
    EXPECT_VEC_SOFT_EQ(expected_eloss, eloss);
    EXPECT_VEC_SOFT_EQ(expected_range, range);
    EXPECT_VEC_SOFT_EQ(expected_step, step);
}

TEST_F(PhysicsTrackViewHostTest, use_integral)
{
    {
        // No energy loss tables
        const auto phys = this->make_track_view("celeriton", MaterialId{2});
        auto       ppid = this->find_ppid(phys, "scattering");
        ASSERT_TRUE(ppid);
        EXPECT_FALSE(phys.integral_xs_process(ppid));

        MaterialView material = this->material()->get(MaterialId{2});
        EXPECT_SOFT_EQ(0.1, phys.calc_xs(ppid, material, MevEnergy{1.0}));
    }
    {
        // Energy loss tables and energy-dependent macro xs
        std::vector<real_type> xs, max_xs;
        const auto phys = this->make_track_view("electron", MaterialId{2});
        auto       ppid = this->find_ppid(phys, "barks");
        ASSERT_TRUE(ppid);
        const auto& integral_proc = phys.integral_xs_process(ppid);
        EXPECT_TRUE(integral_proc);

        MaterialView material = this->material()->get(MaterialId{2});
        for (real_type energy : {0.001, 0.01, 0.1, 0.11, 10.0})
        {
            xs.push_back(phys.calc_xs(ppid, material, MevEnergy{energy}));
            max_xs.push_back(phys.calc_max_xs(
                integral_proc, ppid, material, MevEnergy{energy}));
        }
        const double expected_xs[] = {0.6, 36. / 55, 1.2, 1979. / 1650, 0.6};
        const double expected_max_xs[] = {0.6, 36. / 55, 1.2, 1.2, 357. / 495};
        EXPECT_VEC_SOFT_EQ(expected_xs, xs);
        EXPECT_VEC_SOFT_EQ(expected_max_xs, max_xs);
    }
}

TEST_F(PhysicsTrackViewHostTest, model_finder)
{
    const PhysicsTrackView phys
        = this->make_track_view("celeriton", MaterialId{0});
    auto purr_ppid = this->find_ppid(phys, "purrs");
    ASSERT_TRUE(purr_ppid);
    auto find_model = phys.make_model_finder(purr_ppid);

    // See expected_model_names above
    EXPECT_FALSE(find_model(MevEnergy{0.999e-3}));
    EXPECT_EQ(3, find_model(MevEnergy{0.5}).unchecked_get());
    EXPECT_EQ(4, find_model(MevEnergy{5}).unchecked_get());
    EXPECT_EQ(5, find_model(MevEnergy{50}).unchecked_get());
    EXPECT_FALSE(find_model(MevEnergy{100.1}));
}

TEST_F(PhysicsTrackViewHostTest, element_selector)
{
    MevEnergy  energy{2};
    MaterialId mid{2};

    // Get the sampled process (constant micro xs)
    const PhysicsTrackView phys      = this->make_track_view("celeriton", mid);
    auto                   purr_ppid = this->find_ppid(phys, "purrs");
    ASSERT_TRUE(purr_ppid);

    // Find the model that applies at the given energy
    auto find_model = phys.make_model_finder(purr_ppid);
    auto pmid       = find_model(energy);
    ASSERT_TRUE(pmid);

    // Sample from material composed of three elements (PMF = [0.1, 0.3, 0.6])
    {
        auto table_id = phys.value_table(pmid);
        EXPECT_TRUE(table_id);
        auto select_element = phys.make_element_selector(table_id, energy);
        std::vector<int> counts(this->material()->get(mid).num_elements());
        for (CELER_MAYBE_UNUSED auto i : range(1e5))
        {
            const auto elcomp_id = select_element(this->rng());
            ASSERT_LT(elcomp_id.get(), counts.size());
            ++counts[elcomp_id.get()];
        }
        static const int expected_counts[] = {10210, 30025, 59765};
        EXPECT_VEC_EQ(expected_counts, counts);
    }

    // Material composed of a single element
    {
        PhysicsTrackView phys
            = this->make_track_view("celeriton", MaterialId{1});
        auto table_id = phys.value_table(pmid);
        EXPECT_FALSE(table_id);
    }
}

TEST_F(PhysicsTrackViewHostTest, cuda_surrogate)
{
    std::vector<real_type> step;
    for (const char* particle : {"gamma", "anti-celeriton"})
    {
        PhysicsTrackView phys = this->make_track_view(particle, MaterialId{1});
        PhysicsStepView  pstep = this->make_step_view(particle);

        for (real_type energy : {1e-5, 1e-3, 1., 100., 1e5})
        {
            step.push_back(
                celeritas_test::calc_step(phys, pstep, MevEnergy{energy}));
        }
    }

    const double expected_step[] = {166.6666666667,
                                    166.6666666667,
                                    166.6666666667,
                                    166.6666666667,
                                    inf,
                                    1.428571428571e-05,
                                    0.0001428571428571,
                                    0.1325714285714,
                                    3.016582857143,
                                    3.016582857143};
    EXPECT_VEC_SOFT_EQ(expected_step, step);
}

//---------------------------------------------------------------------------//
// PHYSICS TRACK VIEW (DEVICE)
//---------------------------------------------------------------------------//

#define PHYS_DEVICE_TEST TEST_IF_CELER_DEVICE(PhysicsTrackViewDeviceTest)
class PHYS_DEVICE_TEST : public PhysicsParamsTest
{
    using Base = PhysicsParamsTest;

  public:
    //!@{
    //! Type aliases
    using StateStore = CollectionStateStore<PhysicsStateData, MemSpace::device>;
    //!@}

    void SetUp() override
    {
        Base::SetUp();

        CELER_ASSERT(this->physics());
    }

    StateStore states;
    celeritas::StateCollection<PhysTestInit, Ownership::value, MemSpace::device>
        inits;
};

TEST_F(PHYS_DEVICE_TEST, all)
{
    // Construct initial conditions
    {
        celeritas::StateCollection<PhysTestInit, Ownership::value, MemSpace::host>
            temp_inits;

        auto         init_builder = make_builder(&temp_inits);
        PhysTestInit thread_init;
        for (unsigned int matid : {0, 2})
        {
            thread_init.mat = MaterialId{matid};
            for (real_type energy : {1e-5, 1e-3, 1., 100., 1e5})
            {
                thread_init.energy = MevEnergy{energy};
                for (unsigned int particle : {0, 1, 2})
                {
                    thread_init.particle = ParticleId{particle};
                    init_builder.push_back(thread_init);
                }
            }
        }
        this->inits = temp_inits;
    }

    states = StateStore(*this->physics(), this->inits.size());
    celeritas::DeviceVector<real_type> step(this->states.size());

    PTestInput inp;
    inp.params = this->physics()->device_ref();
    inp.states = this->states.ref();
    inp.inits  = this->inits;
    inp.result = step.device_ref();

    phys_cuda_test(inp);
    std::vector<real_type> step_host(step.size());
    step.copy_to_host(make_span(step_host));
    // clang-format off
    const double expected_step_host[] = {
        1666.666666667, 0.0001666666666667, 0.0001428571428571, 1666.666666667,
        0.001666666666667, 0.001428571428571, 1666.666666667, 0.4885333333333,
        0.4401142857143, 1666.666666667, 33.49328533333, 28.73137257143, inf,
        33.49328533333, 28.73137257143, 1.666666666667, 1.666666666667e-07,
        1.428571428571e-07, 1.666666666667, 1.666666666667e-06,
        1.428571428571e-06, 1.666666666667, 0.001666666666667,
        0.001428571428571, 1.666666666667, 0.1453333333333, 0.1325714285714,
        inf, 0.1453333333333, 0.1325714285714};
    // clang-format on
    EXPECT_VEC_SOFT_EQ(expected_step_host, step_host);
}

//---------------------------------------------------------------------------//
// TEST POSITRON ANNIHILATION
//---------------------------------------------------------------------------//

class EPlusAnnihilationTest : public PhysicsParamsTest
{
  public:
    SPConstMaterial build_material() override;
    SPConstParticle build_particle() override;
    SPConstPhysics  build_physics() override;
};

//---------------------------------------------------------------------------//
auto EPlusAnnihilationTest::build_material() -> SPConstMaterial
{
    using namespace celeritas::units;
    using namespace celeritas::constants;

    MaterialParams::Input mi;
    mi.elements  = {{19, AmuMass{39.0983}, "K"}};
    mi.materials = {{1e-5 * na_avogadro,
                     293.,
                     MatterState::solid,
                     {{ElementId{0}, 1.0}},
                     "K"}};

    return std::make_shared<MaterialParams>(std::move(mi));
}

//---------------------------------------------------------------------------//
auto EPlusAnnihilationTest::build_particle() -> SPConstParticle
{
    using namespace celeritas::units;
    namespace pdg = celeritas::pdg;

    constexpr auto zero   = celeritas::zero_quantity();
    constexpr auto stable = celeritas::ParticleRecord::stable_decay_constant();

    return std::make_shared<ParticleParams>(
        ParticleParams::Input{{"positron",
                               pdg::positron(),
                               MevMass{0.5109989461},
                               ElementaryCharge{1},
                               stable},
                              {"gamma", pdg::gamma(), zero, zero, stable}});
}

//---------------------------------------------------------------------------//
auto EPlusAnnihilationTest::build_physics() -> SPConstPhysics
{
    PhysicsParams::Input physics_inp;
    physics_inp.materials      = this->material();
    physics_inp.particles      = this->particles();
    physics_inp.options        = this->build_physics_options();
    physics_inp.action_manager = this->action_mgr().get();

    EPlusAnnihilationProcess::Options epgg_options;
    epgg_options.use_integral_xs = true;

    physics_inp.processes.push_back(std::make_shared<EPlusAnnihilationProcess>(
        physics_inp.particles, epgg_options));
    return std::make_shared<PhysicsParams>(std::move(physics_inp));
}

TEST_F(EPlusAnnihilationTest, accessors)
{
    const PhysicsParams& p = *this->physics();

    EXPECT_EQ(1, p.num_processes());
    EXPECT_EQ(1, p.num_models());
    EXPECT_EQ(1, p.max_particle_processes());
}

TEST_F(EPlusAnnihilationTest, host_track_view)
{
    CollectionStateStore<PhysicsStateData, MemSpace::host> state{
        *this->physics(), 1};
    ::celeritas::HostCRef<PhysicsParamsData> params_ref{
        this->physics()->host_ref()};

    const auto pid = this->particles()->find("positron");
    ASSERT_TRUE(pid);
    const ParticleProcessId ppid{0};
    const MaterialId        matid{0};

    PhysicsTrackView phys(params_ref, state.ref(), pid, matid, ThreadId{0});
    phys = PhysicsTrackInitializer{};

    // e+ annihilation should have nonzero "inline" cross section for all
    // energies
    EXPECT_EQ(ModelId{0}, phys.hardwired_model(ppid, MevEnergy{0.1234}));
    EXPECT_EQ(ModelId{0}, phys.hardwired_model(ppid, MevEnergy{0}));

    // Check cross section
    MaterialView material_view = this->material()->get(MaterialId{0});
    EXPECT_SOFT_EQ(5.1172452607412999e-05,
                   phys.calc_xs(ppid, material_view, MevEnergy{0.1}));
}