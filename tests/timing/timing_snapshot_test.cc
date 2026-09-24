#include "dali/timing/timing_snapshot.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>

#include "phydb/phydb.h"

namespace dali {
namespace {

int endpoint_capture_slack_calls = 0;

int TwoConstraints() { return 2; }

void NoOpUpdateTiming() {}

std::vector<double> CountSlackReads(const std::vector<int> &ids) {
  endpoint_capture_slack_calls += static_cast<int>(ids.size());
  return std::vector<double>(ids.size(), -1.0);
}

void NoOpViolations(std::vector<int> &) {}

void NoOpWitness(int, std::vector<phydb::ActEdge> &) {}

bool ConstraintEndpoints(int id, phydb::PhydbPin &root,
                         phydb::PhydbPin &fast_terminal,
                         phydb::PhydbPin &slow_terminal) {
  root = phydb::PhydbPin(0, 0);
  fast_terminal = phydb::PhydbPin(1 + id * 2, 0);
  slow_terminal = phydb::PhydbPin(2 + id * 2, 0);
  return true;
}

std::unique_ptr<phydb::PhyDB> MakeEndpointCapturePhyDB() {
  auto phy_db = std::make_unique<phydb::PhyDB>();
  phydb::Macro *macro = phy_db->AddMacro("CELL");
  macro->AddPin("Y", phydb::SignalDirection::OUTPUT,
                phydb::SignalUse::SIGNAL);
  for (int id = 0; id < 5; ++id) {
    phy_db->AddComponent("u" + std::to_string(id), macro,
                         phydb::PlaceStatus::PLACED, 0, 0,
                         phydb::CompOrient::N);
  }
  phy_db->SetGetNumConstraintsCB(TwoConstraints);
  phy_db->SetUpdateTimingIncrementalCB(NoOpUpdateTiming);
  phy_db->SetGetSlackCB(CountSlackReads);
  phy_db->SetGetSlowWitnessCB(NoOpWitness);
  phy_db->SetGetFastWitnessCB(NoOpWitness);
  phy_db->SetGetViolatedTimingConstraintsCB(NoOpViolations);
  phy_db->SetGetConstraintEndpointsCB(ConstraintEndpoints);
  return phy_db;
}

int ThreeConstraints() { return 3; }

// 0 measured and met, 1 unmeasured (-inf), 2 vacuous (+inf, constant fast end)
std::vector<double> MixedSlacks(const std::vector<int> &ids) {
  std::vector<double> slacks;
  for (int id : ids) {
    if (id == 0) {
      slacks.push_back(5.0);
    } else if (id == 1) {
      slacks.push_back(-std::numeric_limits<double>::infinity());
    } else {
      slacks.push_back(std::numeric_limits<double>::infinity());
    }
  }
  return slacks;
}

bool ThirdIsVacuous(int id) { return id == 2; }

}  // namespace

TEST(TimingPathSnapshotTest, SumsStepDelays) {
  TimingPathSnapshot path;
  path.steps.push_back({"u0:Y", "u1:A", "n0", 1.25});
  path.steps.push_back({"u1:A", "u1:Y", "", 0.75});

  EXPECT_DOUBLE_EQ(path.TotalDelay(), 2.0);
}

TEST(TimingPathSnapshotTest, EmptyPathHasZeroDelay) {
  EXPECT_DOUBLE_EQ(TimingPathSnapshot().TotalDelay(), 0.0);
}

TEST(TimingPathSnapshotTest, FindsUniqueSlowOnlyRepairNetsInPathOrder) {
  TimingPathSnapshot fast_path;
  fast_path.steps.push_back({"u0:Y", "u1:A", "shared", 1.0});
  fast_path.steps.push_back({"u1:Y", "u2:A", "fast_only", 1.0});

  TimingPathSnapshot slow_path;
  slow_path.steps.push_back({"u0:Y", "u3:A", "shared", 1.0});
  slow_path.steps.push_back({"u3:Y", "u4:A", "candidate_0", 1.0});
  slow_path.steps.push_back({"u4:Y", "u5:A", "candidate_0", 1.0});
  slow_path.steps.push_back({"u5:Y", "u6:A", "", 1.0});
  slow_path.steps.push_back({"u6:Y", "u7:A", "candidate_1", 1.0});

  EXPECT_EQ(FindDelayRepairCandidateNets(fast_path, slow_path),
            (std::vector<std::string>{"candidate_0", "candidate_1"}));
}

TEST(TimingPathSnapshotTest, SharedSlowPathHasNoRepairCandidates) {
  TimingPathSnapshot fast_path;
  fast_path.steps.push_back({"u0:Y", "u1:A", "shared", 1.0});

  TimingPathSnapshot slow_path;
  slow_path.steps.push_back({"u0:Y", "u2:A", "shared", 1.0});

  EXPECT_TRUE(FindDelayRepairCandidateNets(fast_path, slow_path).empty());
}

TEST(TimingPathSnapshotTest, FindsSlowOnlyDeclaredDelaySiteCandidates) {
  TimingPathSnapshot fast_path;
  fast_path.steps.push_back(
      {"u0:Y", "u1:A", "", 1.0, "control_delay[0]:Y", "u1:A"});

  TimingPathSnapshot slow_path;
  slow_path.steps.push_back(
      {"u0:Y", "u2:A", "", 1.0, "control_delay[0]:Y", "delay_left.inv[0]:A"});
  slow_path.steps.push_back(
      {"u2:A", "u2:Y", "", 1.0, "delay_left.inv[0]:A", "delay_left.inv[0]:Y"});
  slow_path.steps.push_back(
      {"u2:Y", "u3:A", "", 1.0, "delay_right.inv[0]:Y", "delay_right.inv[0]:A"});

  const std::vector<DelayRepairSite> sites = {
      {"control", "top", "control_delay", "", "template", "PAIRS", 2,
       true},
      {"left", "top", "delay_left", "", "template", "PAIRS", 3, true},
      {"right", "top", "delay_right", "", "template", "PAIRS", 4,
       true},
      {"frozen", "top", "delay_right", "", "template", "PAIRS", 4,
       false},
  };

  EXPECT_EQ(FindDelayRepairCandidateSites(fast_path, slow_path, sites),
            (std::vector<std::string>{"left", "right"}));
}

TEST(TimingPathSnapshotTest, UnderscoreIsADelaySiteTokenBoundary) {
  TimingPathSnapshot fast_path;
  fast_path.steps.push_back(
      {"u0:Y", "u1:A", "", 1.0, "logic_0:Y", "logic_1:A"});
  TimingPathSnapshot slow_path;
  slow_path.steps.push_back(
      {"u0:Y", "u2:A", "", 1.0, "dl5_ainv_50_6:Y", "dl5_at_51_6:A"});
  const std::vector<DelayRepairSite> sites = {
      {"dl5", "", "dl5", "dl5", "delay_line", "", 0, true},
      {"dl", "", "dl", "dl", "delay_line", "", 0, true},
  };

  EXPECT_EQ(FindDelayRepairCandidateSites(fast_path, slow_path, sites),
            (std::vector<std::string>{"dl5"}));
}

TEST(TimingPathSnapshotTest, DollarIsADelaySiteTokenBoundary) {
  TimingPathSnapshot fast_path;
  fast_path.steps.push_back(
      {"u0:Y", "u1:A", "", 1.0, "logic:Y", "logic_next:A"});
  TimingPathSnapshot slow_path;
  slow_path.steps.push_back(
      {"u0:Y", "u2:A", "", 1.0, "dl5$ainv:Y", "dl5$next:A"});
  const std::vector<DelayRepairSite> sites = {
      {"dl5", "", "dl5", "dl5", "delay_line", "", 0, true},
      {"dl", "", "dl", "dl", "delay_line", "", 0, true},
  };

  EXPECT_EQ(FindDelayRepairCandidateSites(fast_path, slow_path, sites),
            (std::vector<std::string>{"dl5"}));
}

TEST(DelaySiteMetadataTest, LoadsSchemaV1SitesAndBuildsRepairPlan) {
  const std::filesystem::path metadata_path =
      std::filesystem::temp_directory_path() / "dali_delay_sites.json";
  std::ofstream output(metadata_path);
  ASSERT_TRUE(output.is_open());
  output << R"({
    "schema_version": 1,
    "time_unit": "ps",
    "parameter_semantics": "technology_specific",
    "delay_sites": [
      {
        "id": "slow_site",
        "process_name": "pipeline",
        "instance_name": "delay_instance",
        "logical_path_prefix": "delay_instance",
        "kind": "template_parameter",
        "parameter_name": "PAIRS",
        "initial_parameter_value": 5,
        "adjustable": true
      }
    ]
  })";
  output.close();

  std::vector<DelayRepairSite> sites;
  std::string error_message;
  ASSERT_TRUE(ReadDelayRepairSiteMetadata(metadata_path.string(), &sites,
                                          &error_message));
  ASSERT_EQ(sites.size(), 1U);
  EXPECT_EQ(sites[0].id, "slow_site");
  EXPECT_EQ(sites[0].parameter_name, "PAIRS");
  EXPECT_EQ(sites[0].initial_parameter_value, 5);

  TimingSnapshot snapshot;
  RelativeTimingViolationSnapshot violation;
  violation.slack = -17.5;
  violation.delay_repair_candidate_sites = {"slow_site"};
  snapshot.relative_violations.push_back(violation);
  const std::vector<TimingRepairSitePlanItem> plan =
      BuildTimingRepairSitePlan(snapshot, sites);
  ASSERT_EQ(plan.size(), 1U);
  EXPECT_EQ(plan[0].instance_name, "delay_instance");
  EXPECT_EQ(plan[0].initial_parameter_value, 5);
  EXPECT_DOUBLE_EQ(plan[0].worst_slack, -17.5);

  std::filesystem::remove(metadata_path);
}

TEST(TimingSnapshotTest, RanksCandidatesUsingAllConstraintRoles) {
  RelativeTimingConstraintSnapshot worst;
  worst.constraint_id = 3;
  worst.slack = -10.0;
  worst.slow_path.steps.push_back({"a", "b", "safe", 1.0});
  worst.slow_path.steps.push_back({"b", "c", "mixed", 1.0});

  RelativeTimingConstraintSnapshot other;
  other.constraint_id = 7;
  other.slack = -2.0;
  other.fast_path.steps.push_back({"d", "e", "mixed", 1.0});

  const std::vector<TimingNetCandidateSnapshot> candidates =
      BuildDelayRepairCandidates({worst, other});
  ASSERT_EQ(candidates.size(), 2U);
  EXPECT_EQ(candidates[0].net_name, "safe");
  EXPECT_DOUBLE_EQ(candidates[0].improved_negative_slack, 10.0);
  EXPECT_TRUE(candidates[0].degraded_constraint_ids.empty());
  EXPECT_EQ(candidates[1].net_name, "mixed");
  EXPECT_DOUBLE_EQ(candidates[1].degraded_negative_slack, 2.0);
  EXPECT_EQ(candidates[1].degraded_constraint_ids, (std::vector<int>{7}));
}

TEST(TimingSnapshotTest, CollectsUniqueCandidateDriverAndLoadPins) {
  RelativeTimingConstraintSnapshot first;
  first.constraint_id = 1;
  first.slack = -3.0;
  first.slow_path.steps.push_back({"driver:Y", "load_1:A", "branch", 1.0,
                                   "top.driver.Y", "top.load_1.A",
                                   "top.branch"});

  RelativeTimingConstraintSnapshot second;
  second.constraint_id = 2;
  second.slack = -2.0;
  second.slow_path.steps.push_back({"driver:Y", "load_2:A", "branch", 1.0,
                                    "top.driver.Y", "top.load_2.A",
                                    "top.branch"});
  second.slow_path.steps.push_back({"driver:Y", "load_1:A", "branch", 1.0,
                                    "top.driver.Y", "top.load_1.A",
                                    "top.branch"});

  const std::vector<TimingNetCandidateSnapshot> candidates =
      BuildDelayRepairCandidates({first, second});
  ASSERT_EQ(candidates.size(), 1U);
  EXPECT_EQ(candidates[0].driver_pins, (std::vector<std::string>{"driver:Y"}));
  EXPECT_EQ(candidates[0].load_pins,
            (std::vector<std::string>{"load_1:A", "load_2:A"}));
  EXPECT_EQ(candidates[0].logical_net_names,
            (std::vector<std::string>{"top.branch"}));
  EXPECT_EQ(candidates[0].logical_driver_pins,
            (std::vector<std::string>{"top.driver.Y"}));
  EXPECT_EQ(candidates[0].logical_load_pins,
            (std::vector<std::string>{"top.load_1.A", "top.load_2.A"}));
}

TEST(TimingSnapshotTest, ReversesRolesForFastPathPlacement) {
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = 11;
  constraint.slack = -4.0;
  constraint.fast_path.steps.push_back({"a", "b", "accelerate", 1.0});
  constraint.slow_path.steps.push_back({"a", "c", "delay", 1.0});

  const std::vector<TimingNetCandidateSnapshot> candidates =
      BuildFastPathPlacementCandidates({constraint});
  ASSERT_EQ(candidates.size(), 1U);
  EXPECT_EQ(candidates[0].net_name, "accelerate");
  EXPECT_EQ(candidates[0].improved_constraint_ids, (std::vector<int>{11}));
  EXPECT_TRUE(candidates[0].degraded_constraint_ids.empty());
  EXPECT_DOUBLE_EQ(candidates[0].improved_path_delay, 1.0);
}

TEST(TimingSnapshotTest, PrioritizesSafeHighDelayPlacementCandidates) {
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = 13;
  constraint.slack = -3.0;
  constraint.fast_path.steps.push_back({"a", "b", "short", 1.0});
  constraint.fast_path.steps.push_back({"b", "c", "long", 5.0});

  const std::vector<TimingNetCandidateSnapshot> candidates =
      BuildFastPathPlacementCandidates({constraint});
  ASSERT_EQ(candidates.size(), 2U);
  EXPECT_EQ(candidates[0].net_name, "long");
  EXPECT_DOUBLE_EQ(candidates[0].improved_path_delay, 5.0);
}

TEST(TimingSnapshotTest, WritesRepairPlanJson) {
  TimingSnapshot snapshot;
  snapshot.has_critical_cycle = true;
  snapshot.critical_cycle_period = 17.5;
  snapshot.critical_cycle_unroll_factor = 3;
  snapshot.relative_constraint_count = 4;
  snapshot.worst_relative_slack = -10.0;
  snapshot.relative_total_negative_slack = -12.0;
  RelativeTimingViolationSnapshot violation;
  violation.constraint_id = 3;
  violation.slack = -10.0;
  violation.fast_path.steps.push_back({"fast_driver:Y", "fast_load:A",
                                       "fast_net", 1.25, "top.fast_driver.Y",
                                       "top.fast_load.A", "top.fast_net"});
  violation.slow_path.steps.push_back({"slow_driver:Y", "slow_load:A",
                                       "slow_net", 2.5, "top.slow_driver.Y",
                                       "top.slow_load.A", "top.slow_net"});
  violation.delay_repair_candidate_nets = {"slow_net"};
  violation.delay_repair_candidate_sites = {"dl1"};
  snapshot.relative_violations.push_back(violation);
  TimingNetCandidateSnapshot candidate;
  candidate.net_name = "net\\\"0";
  candidate.improved_constraint_ids = {3};
  candidate.degraded_constraint_ids = {7};
  candidate.improved_negative_slack = 10.0;
  candidate.degraded_negative_slack = 2.0;
  candidate.improved_path_delay = 4.0;
  candidate.degraded_path_delay = 1.0;
  candidate.driver_pins = {"driver:Y"};
  candidate.load_pins = {"load:A"};
  candidate.logical_net_names = {"top.net"};
  candidate.logical_driver_pins = {"top.driver.Y"};
  candidate.logical_load_pins = {"top.load.A"};
  snapshot.delay_repair_candidates.push_back(candidate);
  // A site whose constraints all pass must still be reported, because a
  // sizing loop can only shrink what it can see.
  DelaySiteSlackSnapshot passing_site;
  passing_site.site_id = "dl_spare";
  passing_site.worst_slack = 262.5;
  passing_site.constraint_count = 2;
  passing_site.violating_count = 0;
  snapshot.delay_site_slack.push_back(passing_site);
  DelaySiteSlackSnapshot failing_site;
  failing_site.site_id = "dl_short";
  failing_site.worst_slack = -91.5;
  failing_site.constraint_count = 3;
  failing_site.violating_count = 3;
  snapshot.delay_site_slack.push_back(failing_site);

  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "dali_timing_repair_plan.json";

  ASSERT_TRUE(WriteTimingRepairPlanJson(snapshot, output_path.string()));
  std::ifstream input(output_path);
  std::ostringstream contents;
  contents << input.rdbuf();
  EXPECT_NE(contents.str().find("\"net_name\": \"net\\\\\\\"0\""),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"schema_version\": 7"), std::string::npos);
  EXPECT_NE(contents.str().find("\"has_critical_cycle\": true"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"critical_cycle_period\": 17.5"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"critical_cycle_unroll_factor\": 3"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"relative_constraint_count\": 4"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"relative_violations\": ["),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"site_id\": \"dl_spare\""),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"worst_slack\": 262.5"), std::string::npos);
  EXPECT_NE(contents.str().find("\"site_id\": \"dl_short\""),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"violating_count\": 0"), std::string::npos);
  EXPECT_NE(contents.str().find("\"constraint_id\": 3"), std::string::npos);
  EXPECT_NE(contents.str().find("\"slack\": -10"), std::string::npos);
  EXPECT_NE(contents.str().find("\"total_delay\": 1.25"), std::string::npos);
  EXPECT_NE(contents.str().find("\"logical_net_name\": \"top.fast_net\""),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"delay_repair_candidate_nets\": "
                                "[\"slow_net\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"delay_repair_candidate_sites\": "
                                "[\"dl1\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"logical_net_names\": [\"top.net\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"driver_pins\": [\"driver:Y\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"load_pins\": [\"load:A\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"logical_driver_pins\": [\"top.driver.Y\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"logical_load_pins\": [\"top.load.A\"]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"improved_constraint_ids\": [3]"),
            std::string::npos);
  EXPECT_NE(contents.str().find("\"fast_path_placement_candidates\": ["),
            std::string::npos);
  std::filesystem::remove(output_path);
}

static RelativeTimingConstraintSnapshot ConstraintWithIdentity(
    int id, const std::string &root, const std::string &slow_terminal,
    const std::string &fast_terminal) {
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = id;
  constraint.slack = -10.0 + id;
  constraint.fast_path.root_pin = root;
  constraint.slow_path.root_pin = root;
  constraint.fast_path.terminal_pin = fast_terminal;
  constraint.slow_path.terminal_pin = slow_terminal;
  return constraint;
}

TEST(TimingSnapshotTest, WritesConstraintIdentitiesInConstraintIdOrder) {
  TimingSnapshot snapshot;
  snapshot.relative_constraint_count = 2;
  snapshot.relative_constraints = {
      ConstraintWithIdentity(1, "root1:Y", "slow1:D", "fast1:D"),
      ConstraintWithIdentity(0, "root0:Y", "slow0:D", "fast0:D")};
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "dali_timing_constraint_identities.json";

  ASSERT_TRUE(
      WriteTimingConstraintIdentitiesJson(snapshot, output_path.string()));
  std::ifstream input(output_path);
  std::ostringstream contents;
  contents << input.rdbuf();
  const std::string text = contents.str();
  EXPECT_LT(text.find("root0:Y|slow0:D|fast0:D"),
            text.find("root1:Y|slow1:D|fast1:D"));
  EXPECT_NE(text.find("\"constraint_count\": 2"), std::string::npos);
  EXPECT_NE(text.find("\"slack\": -10"), std::string::npos);
  std::filesystem::remove(output_path);
}

TEST(TimingSnapshotTest, RefusesIncompleteOrRenumberedConstraintIdentitySets) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "dali_bad_timing_constraint_identities.json";
  TimingSnapshot missing;
  missing.relative_constraint_count = 2;
  missing.relative_constraints = {
      ConstraintWithIdentity(0, "root0:Y", "slow0:D", "fast0:D")};
  EXPECT_FALSE(
      WriteTimingConstraintIdentitiesJson(missing, output_path.string()));

  TimingSnapshot duplicate_id;
  duplicate_id.relative_constraint_count = 2;
  duplicate_id.relative_constraints = {
      ConstraintWithIdentity(0, "root0:Y", "slow0:D", "fast0:D"),
      ConstraintWithIdentity(0, "root1:Y", "slow1:D", "fast1:D")};
  EXPECT_FALSE(WriteTimingConstraintIdentitiesJson(duplicate_id,
                                                   output_path.string()));
}

TEST(TimingSnapshotTest, RefusesMissingRootsAndDuplicateSemanticIdentities) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() /
      "dali_ambiguous_timing_constraint_identities.json";
  TimingSnapshot mismatched_roots;
  mismatched_roots.relative_constraint_count = 1;
  RelativeTimingConstraintSnapshot mismatched =
      ConstraintWithIdentity(0, "root:Y", "slow:D", "fast:D");
  mismatched.slow_path.root_pin = "other:Y";
  mismatched_roots.relative_constraints = {mismatched};
  EXPECT_FALSE(WriteTimingConstraintIdentitiesJson(mismatched_roots,
                                                   output_path.string()));

  TimingSnapshot duplicate_identity;
  duplicate_identity.relative_constraint_count = 2;
  duplicate_identity.relative_constraints = {
      ConstraintWithIdentity(0, "root:Y", "slow:D", "fast:D"),
      ConstraintWithIdentity(1, "root:Y", "slow:D", "fast:D")};
  EXPECT_FALSE(WriteTimingConstraintIdentitiesJson(duplicate_identity,
                                                   output_path.string()));
}

TEST(TimingSnapshotTest, CanonicalizesOnlyRegisteredReplaceableSiteEndpoints) {
  TimingSnapshot before;
  before.relative_constraint_count = 2;
  before.relative_constraints = {
      ConstraintWithIdentity(0, "loop:Y", "dl0_ainv_513_6:Y", "g0:Y"),
      ConstraintWithIdentity(1, "logic:Y", "sink:D", "other_dl0_cell:Y")};
  TimingSnapshot after;
  after.relative_constraint_count = 2;
  after.relative_constraints = {
      ConstraintWithIdentity(0, "loop:Y", "dl0_ainv_523_6:Y", "g0:Y"),
      ConstraintWithIdentity(1, "logic:Y", "sink:D", "other_dl0_cell:Y")};

  CanonicalizeReplaceableSiteEndpoints(&before, {"dl0"});
  CanonicalizeReplaceableSiteEndpoints(&after, {"dl0"});

  EXPECT_EQ(before.relative_constraints[0].SemanticIdentity(),
            "loop:Y|delay-site:dl0:Y|g0:Y");
  EXPECT_EQ(before.relative_constraints[0].SemanticIdentity(),
            after.relative_constraints[0].SemanticIdentity());
  EXPECT_EQ(before.relative_constraints[1].SemanticIdentity(),
            "logic:Y|sink:D|other_dl0_cell:Y");
}

TEST(TimingSnapshotBuilderTest, VacuousForksAreNeitherViolationsNorUnmeasured) {
  std::unique_ptr<phydb::PhyDB> phy_db = MakeEndpointCapturePhyDB();
  // ConstraintEndpoints maps constraint id to components 1+2*id and 2+2*id;
  // the helper builds five, enough for two constraints. Add the third's.
  phydb::Macro *macro = phy_db->GetMacroPtr("CELL");
  for (int id = 5; id < 7; ++id) {
    phy_db->AddComponent("u" + std::to_string(id), macro,
                         phydb::PlaceStatus::PLACED, 0, 0,
                         phydb::CompOrient::N);
  }
  phy_db->SetGetNumConstraintsCB(ThreeConstraints);
  phy_db->SetGetSlackCB(MixedSlacks);

  // Without the timer's callback a +inf slack is still unknown: counted
  // unmeasured, exactly as before.
  const TimingSnapshot before = TimingSnapshotBuilder(phy_db.get()).Capture();
  ASSERT_EQ(before.relative_constraints.size(), 3U);
  EXPECT_EQ(before.relative_vacuous_count, 0);
  EXPECT_EQ(before.relative_unmeasured_count, 2);

  phy_db->SetIsForkVacuousCB(ThirdIsVacuous);
  const TimingSnapshot after = TimingSnapshotBuilder(phy_db.get()).Capture();
  ASSERT_EQ(after.relative_constraints.size(), 3U);
  EXPECT_EQ(after.relative_vacuous_count, 1);
  EXPECT_EQ(after.relative_unmeasured_count, 1);
  EXPECT_TRUE(after.relative_constraints[2].vacuous);
  ASSERT_EQ(after.relative_violations.size(), 1U);
  EXPECT_EQ(after.relative_violations[0].constraint_id, 1);
  EXPECT_EQ(after.worst_relative_constraint_id, 0);

  const TimingSnapshot identities =
      TimingSnapshotBuilder(phy_db.get()).CaptureConstraintIdentities();
  EXPECT_EQ(identities.relative_vacuous_count, 1);
  for (const auto &violation : identities.relative_violations) {
    EXPECT_NE(violation.constraint_id, 2);
  }
}

TEST(TimingSnapshotBuilderTest, EndpointIdentitiesDoNotReadSlack) {
  std::unique_ptr<phydb::PhyDB> phy_db = MakeEndpointCapturePhyDB();
  endpoint_capture_slack_calls = 0;

  const TimingSnapshot endpoints =
      TimingSnapshotBuilder(phy_db.get()).CaptureConstraintEndpointIdentities();

  ASSERT_EQ(endpoints.relative_constraints.size(), 2U);
  EXPECT_EQ(endpoint_capture_slack_calls, 0);
  EXPECT_EQ(endpoints.relative_constraints[0].SemanticIdentity(),
            "u0:Y|u2:Y|u1:Y");
  EXPECT_EQ(endpoints.relative_constraints[1].SemanticIdentity(),
            "u0:Y|u4:Y|u3:Y");

  const TimingSnapshot timed =
      TimingSnapshotBuilder(phy_db.get()).CaptureConstraintIdentities();
  EXPECT_EQ(endpoint_capture_slack_calls, 2);
  ASSERT_EQ(timed.relative_constraints.size(), 2U);
  EXPECT_EQ(timed.relative_constraints[0].SemanticIdentity(),
            endpoints.relative_constraints[0].SemanticIdentity());
  EXPECT_EQ(timed.relative_constraints[1].SemanticIdentity(),
            endpoints.relative_constraints[1].SemanticIdentity());
}

} // namespace dali
