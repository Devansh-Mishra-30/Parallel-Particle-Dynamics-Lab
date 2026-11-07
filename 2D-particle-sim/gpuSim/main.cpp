#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#include "particle_data.h"
#include "simulator_cuda.cuh"
#include "renderer.h"
#include <filesystem>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Font.hpp>
#include <sstream>
#include <iomanip>


struct Args {
    // ---- CPU/OMP compatible benchmarking fields ----
    int substeps = 8;                 // match CPU default
    double benchSeconds = 10.0;       // run for fixed wall time like CPU
    unsigned seed = 1;                // same seed for fair comparison
    std::string spawn = "instant";    // placeholder (matches CPU CSV)

    // ---- modes ----
    bool bench = false;
    bool headless = false;
    bool csvHeader = true;

    // ---- render ----
    unsigned int width = 800;
    unsigned int height = 800;

    // ---- simulation ----
    float boundaryRadius = 350.0f;
    std::size_t particleCount = 1000;

    float gravityX = 0.0f;
    float gravityY = 100.0f;

    float fixedDt = 1.0f / 60.0f;

    // warmup frames before timing
    int warmup = 200;

    // only used for non-bench headless mode
    int steps = 2000;

    // output CSV path
    std::string csvPath = "timings_cuda.csv";
};


static std::optional<std::string> getValue(int& i, int argc, char** argv) {
    if (i + 1 >= argc) return std::nullopt;
    ++i;
    return std::string(argv[i]);
}

static Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--bench") {
            a.bench = true;
            a.headless = true;
            continue;
        }

        if (arg == "--headless") {
            a.headless = true;
            continue;
        }

        if (arg == "--no-csv-header") {
            a.csvHeader = false;
            continue;
        }

        if (arg == "--width") {
            auto v = getValue(i, argc, argv);
            if (v) a.width = static_cast<unsigned int>(std::stoul(*v));
            continue;
        }

        if (arg == "--height") {
            auto v = getValue(i, argc, argv);
            if (v) a.height = static_cast<unsigned int>(std::stoul(*v));
            continue;
        }

        if (arg == "--n") {
            auto v = getValue(i, argc, argv);
            if (v) a.particleCount = static_cast<std::size_t>(std::stoull(*v));
            continue;
        }

        if (arg == "--boundary") {
            auto v = getValue(i, argc, argv);
            if (v) a.boundaryRadius = std::stof(*v);
            continue;
        }

        if (arg == "--gx") {
            auto v = getValue(i, argc, argv);
            if (v) a.gravityX = std::stof(*v);
            continue;
        }

        if (arg == "--gy") {
            auto v = getValue(i, argc, argv);
            if (v) a.gravityY = std::stof(*v);
            continue;
        }

        if (arg == "--dt") {
            auto v = getValue(i, argc, argv);
            if (v) a.fixedDt = std::stof(*v);
            continue;
        }

        if (arg == "--warmup") {
            auto v = getValue(i, argc, argv);
            if (v) a.warmup = std::stoi(*v);
            continue;
        }

        if (arg == "--steps") {
            auto v = getValue(i, argc, argv);
            if (v) a.steps = std::stoi(*v);
            continue;
        }

        if (arg == "--csv") {
            auto v = getValue(i, argc, argv);
            if (v) a.csvPath = *v;
            continue;
        }
    }
    return a;
}

static void initParticles(std::vector<ParticleData>& particles, float boundaryRadius, unsigned seed) {
    std::srand(seed);
    const float maxInitialDist = boundaryRadius - 20.0f;

    for (std::size_t i = 0; i < particles.size(); ++i) {
        const float radius = 2.0f + static_cast<float>(std::rand() % 11);
        const float angle = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f * 3.14159265f;
        const float randUnit = static_cast<float>(std::rand()) / RAND_MAX;
        const float rDist = maxInitialDist * std::sqrt(randUnit);

        particles[i].px = rDist * std::cos(angle);
        particles[i].py = rDist * std::sin(angle);
        particles[i].vx = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 100.0f;
        particles[i].vy = 0.0f;
        particles[i].radius = radius;

        particles[i].r = static_cast<uint8_t>(std::rand() % 256);
        particles[i].g = static_cast<uint8_t>(std::rand() % 256);
        particles[i].b = static_cast<uint8_t>(std::rand() % 256);
        particles[i].a = 255;
    }
}

static void appendBenchRow(const Args& a, float msPerStep) {
    const bool needHeader =
        a.csvHeader &&
        (!std::filesystem::exists(a.csvPath) || std::filesystem::file_size(a.csvPath) == 0);

    std::ofstream out(a.csvPath, std::ios::app);
    if (!out) {
        std::cerr << "Failed to open CSV: " << a.csvPath << "\n";
        return;
    }

    if (needHeader) {
        out << "mode,N,dt,warmup,steps,ms_per_step,steps_per_sec\n";
    }

    double stepsPerSec = 0.0;
    if (msPerStep > 0.0f) stepsPerSec = 1000.0 / static_cast<double>(msPerStep);

    out << "cuda"
        << "," << a.particleCount
        << "," << a.fixedDt
        << "," << a.warmup
        << "," << a.steps
        << "," << msPerStep
        << "," << stepsPerSec
        << "\n";
}

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);

    std::vector<ParticleData> particles(args.particleCount);
    const unsigned seed = static_cast<unsigned>(std::time(nullptr));
    initParticles(particles, args.boundaryRadius, seed);

    Simulator simulator(particles.data(), args.particleCount, args.boundaryRadius, args.gravityX, args.gravityY);

    if (args.bench) {
        const float msPerStep = simulator.benchmarkSteps(args.fixedDt, args.warmup, args.steps);
        appendBenchRow(args, msPerStep);
        std::cout << "CUDA bench: N=" << args.particleCount << " ms/step=" << msPerStep << "\n";
        return 0;
    }

    if (args.headless) {
        for (int i = 0; i < args.steps; ++i) {
            simulator.update(args.fixedDt, false);
        }
        return 0;
    }

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(args.width, args.height)), "GPU Particle Simulation");

    sf::View customView(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(static_cast<float>(args.width), static_cast<float>(args.height))
    );
    window.setView(customView);
    window.setFramerateLimit(60);

    sf::Font font;
    bool fontOk = font.openFromFile("assets/ABeeZee-Regular.ttf");

    sf::Text hud(font, "", 18);
    hud.setFillColor(sf::Color::White);
    hud.setPosition({-390.f, -390.f}); // top-left in centered view
    hud.setCharacterSize(18);
    if (!font.openFromFile("assets/ABeeZee-Regular.ttf")) {
        // If font fails, you can still run without overlay
    }

    

    Renderer renderer(args.particleCount);

    sf::Clock clock;
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (auto key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                }
            }
        }

        const float dt = clock.restart().asSeconds();

        static float fpsSmoothed = 0.f;
        static float hudTimer = 0.f;

        hudTimer += dt;

        float fps = (dt > 0.f) ? (1.f / dt) : 0.f;
        fpsSmoothed = (fpsSmoothed == 0.f) ? fps : (0.9f * fpsSmoothed + 0.1f * fps);

        if (fontOk && hudTimer >= 0.25f) {
            hudTimer = 0.f;
            std::ostringstream ss;
            ss << "FPS: " << std::fixed << std::setprecision(0) << fpsSmoothed << "\n"
            << "# Particles: " << simulator.getParticleCount();
            hud.setString(ss.str());
        }

        simulator.update(dt, true);

        window.clear(sf::Color::Black);
        renderer.drawParticles(window, simulator.getParticlesHost(), simulator.getParticleCount());
        if (fontOk) window.draw(hud);
        window.display();

    }

    return 0;
}
