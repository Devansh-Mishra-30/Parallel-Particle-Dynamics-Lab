#include "lab_gui/workbench_predictions.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    using namespace lab_gui;
    WorkbenchParameters baseline;
    baseline.restitution = 0.528f;
    WorkbenchParameters modified = baseline;
    require(!parameters_differ(baseline, modified), "Identical baseline must match");
    require(predict_changes(baseline, modified, PhysicsScenario::Collision,
                           InvestigationMode::PhysicsVerification).empty(),
            "No predictions for identical configuration");

    modified.restitution = 0.8f;
    auto notes = predict_changes(baseline, modified, PhysicsScenario::Collision,
                                 InvestigationMode::PhysicsVerification);
    require(parameters_differ(baseline, modified), "Changed restitution must be detected");
    require(notes.size() == 1, "Expected one prediction for changed restitution");
    require(notes[0].parameter.find("restitution") != std::string::npos,
            "Restitution explanation missing");
    require(notes[0].expectation.find("Less kinetic") != std::string::npos,
            "Higher restitution should indicate less loss");

    modified.substeps = 16;
    modified.gravity_y = -9.81f;
    modified.particle_count = 128;
    notes = predict_changes(baseline, modified, PhysicsScenario::Gravity,
                             InvestigationMode::NumericalAccuracy);
    require(notes.size() == 4, "Expected four independent parameter predictions");
    require(notes[2].parameter == "Integration substeps", "Expected a substeps explanation");
    require(notes[1].inspect.find("BEFORE wall contact") != std::string::npos,
            "Analytical gravity reference must warn about boundary contact");

    modified = baseline;
    modified.restitution = 0.2f;
    notes = predict_changes(baseline, modified, PhysicsScenario::Collision,
                             InvestigationMode::PhysicsVerification);
    require(notes.size() == 1, "Expected one prediction for lower restitution");
    require(notes[0].expectation.find("More kinetic") != std::string::npos,
            "Lower restitution should indicate more energy loss");

    modified = baseline;
    modified.wall_restitution = 0.9f;
    modified.frame_dt = 0.02f;
    modified.contact_iterations = 8;
    modified.stabilization = 0.8f;
    modified.boundary_radius = 1.8f;
    modified.integrator = 1;
    notes = predict_changes(baseline, modified, PhysicsScenario::DenseContacts,
                             InvestigationMode::NumericalAccuracy);
    require(notes.size() == 6, "Six numerical parameter predictions expected");

    modified = baseline;
    modified.radius_1 = 0.08f;
    modified.mass_1 = 20.0f;
    modified.impact_offset = 0.04f;
    notes = predict_changes(baseline, modified, PhysicsScenario::Collision,
                             InvestigationMode::PhysicsVerification);
    require(notes.size() == 2, "Mass/radius + impact offset should yield two explanations");
    modified = baseline;
    require(!parameters_differ(baseline, modified), "Restoring baseline clears delta");
    std::cout << "Stage 3: parameter baseline and prediction rules passed.\n";
    return 0;
}
