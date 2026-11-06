#include "lab_gui/workbench_evidence.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    using namespace lab_gui;
    if (median_measurement({7.0, 1.0, 3.0}) != 3.0) return 1;
    if (median_measurement({5.0, 1.0, 7.0, 3.0}) != 4.0) return 2;
    if (!std::isnan(median_measurement({}))) return 3;
    if (safe_speedup(20.0, 5.0) != 4.0 || safe_speedup(20.0, 0.0) != 0.0) return 4;
    WorkbenchEvidence sample;
    if (export_evidence_csv(sample, "never_create_stage4.csv")) return 5;
    sample.kind = EvidenceStudy::ContactSensitivity;
    sample.valid = true;
    sample.scenario = "dense_contacts";
    sample.protocol = "identical duration";
    EvidenceRow row; row.particles = 48; row.substeps = 12; row.contact_iterations = 8;
    row.maximum_penetration_m = 0.00025f;
    sample.rows.push_back(row);
    if (!export_evidence_csv(sample, "stage4_evidence_test.csv")) return 6;
    std::cout << "Stage 4 evidence metadata/CSV helpers verified\n";
}
