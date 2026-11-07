#include <SFML/Graphics.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <cmath>


#include "renderer.h"
#include "simulator.h"

namespace perf {

static inline bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

static inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const double idx = (p / 100.0) * (static_cast<double>(v.size() - 1));
    const size_t i0 = static_cast<size_t>(std::floor(idx));
    const size_t i1 = std::min(i0 + 1, v.size() - 1);
    const double frac = idx - static_cast<double>(i0);
    return v[i0] * (1.0 - frac) + v[i1] * frac;
}

static inline bool is_finite_vec(const sf::Vector2f& v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

static inline int count_nan_particles(const Simulator& sim) {
    int c = 0;
    const auto& particles = sim.getParticles();
    for (const auto& p : particles) {
        const sf::Vector2f pos = p.getPosition();
        if (!is_finite_vec(pos)) c++;
    }
    return c;
}

static inline bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

struct Args {
    bool bench = false;
    bool log_demo = false;

    int n = 20000;
    int substeps = 8;
    float seconds = 10.0f;
    int seed = 1;

    std::string csv_path;
    std::string spawn = "gradual"; // gradual | instant
    std::string backend = "cpu";   // cpu now, keep same column for omp/cuda
};

static inline void print_help() {
    std::cout
        << "Usage:\n"
        << "  Demo:\n"
        << "    ./build/simulator [N]\n"
        << "    ./build/simulator --n 20000 --substeps 8\n"
        << "\n"
        << "  Headless benchmark:\n"
        << "    ./build/simulator --bench --spawn instant --n 20000 --seconds 10 --substeps 8 --seed 1 --csv results.csv\n"
        << "\n"
        << "Flags:\n"
        << "  --bench              Run headless benchmark (no window, no renderer)\n"
        << "  --log-demo           In demo mode, append 1 row per second to csv (fps + particle count)\n"
        << "  --n <int>            Target particle count\n"
        << "  --substeps <int>     Substeps per frame\n"
        << "  --seconds <float>    Benchmark duration (bench mode)\n"
        << "  --seed <int>         RNG seed\n"
        << "  --spawn <mode>       gradual | instant\n"
        << "  --csv <path>         CSV output path\n";
}

static inline Args parse_args(int argc, char* argv[]) {
    Args a;

    if (argc > 1) {
        bool has_flag = false;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] == '-') { has_flag = true; break; }
        }
        if (!has_flag) {
            a.n = std::atoi(argv[1]);
            return a;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string s = argv[i];

        if (s == "--help") {
            print_help();
            std::exit(0);
        }
        if (s == "--bench") {
            a.bench = true;
            continue;
        }
        if (s == "--log-demo") {
            a.log_demo = true;
            continue;
        }
        if (s == "--n" && i + 1 < argc) {
            a.n = std::atoi(argv[++i]);
            continue;
        }
        if (s == "--substeps" && i + 1 < argc) {
            a.substeps = std::atoi(argv[++i]);
            continue;
        }
        if (s == "--seconds" && i + 1 < argc) {
            a.seconds = std::stof(argv[++i]);
            continue;
        }
        if (s == "--seed" && i + 1 < argc) {
            a.seed = std::atoi(argv[++i]);
            continue;
        }
        if (s == "--spawn" && i + 1 < argc) {
            a.spawn = argv[++i];
            continue;
        }
        if (s == "--csv" && i + 1 < argc) {
            a.csv_path = argv[++i];
            continue;
        }
    }

    return a;
}

static inline void write_bench_header_if_needed(const std::string& path) {
    if (path.empty()) return;
    if (file_exists(path)) return;

    std::ofstream out(path, std::ios::out);
    out << "backend,mode,spawn,seed,n_target,n_final,substeps,dt,seconds,frames,"
           "avg_fps,avg_ms_frame,avg_ms_evolve,p50_ms_evolve,p95_ms_evolve,max_ms_evolve,nan_count\n";
}

static inline void append_bench_row(
    const std::string& path,
    const std::string& backend,
    const std::string& mode,
    const std::string& spawn,
    int seed,
    int n_target,
    int n_final,
    int substeps,
    float dt,
    float seconds,
    long long frames,
    double avg_fps,
    double avg_ms_frame,
    double avg_ms_evolve,
    double p50_ms_evolve,
    double p95_ms_evolve,
    double max_ms_evolve,
    int nan_count
) {
    if (path.empty()) return;
    write_bench_header_if_needed(path);

    std::ofstream out(path, std::ios::app);
    out << backend << ","
        << mode << ","
        << spawn << ","
        << seed << ","
        << n_target << ","
        << n_final << ","
        << substeps << ","
        << dt << ","
        << seconds << ","
        << frames << ","
        << avg_fps << ","
        << avg_ms_frame << ","
        << avg_ms_evolve << ","
        << p50_ms_evolve << ","
        << p95_ms_evolve << ","
        << max_ms_evolve << ","
        << nan_count
        << "\n";
}

static inline void write_demo_header_if_needed(const std::string& path) {
    if (path.empty()) return;
    if (file_exists(path)) return;

    std::ofstream out(path, std::ios::out);
    out << "backend,mode,spawn,seed,n_target,n_current,substeps,dt,wall_time_s,fps\n";
}

static inline void append_demo_row(
    const std::string& path,
    const std::string& backend,
    const std::string& spawn,
    int seed,
    int n_target,
    int n_current,
    int substeps,
    float dt,
    float wall_time_s,
    float fps
) {
    if (path.empty()) return;
    write_demo_header_if_needed(path);

    std::ofstream out(path, std::ios::app);
    out << backend << ","
        << "demo" << ","
        << spawn << ","
        << seed << ","
        << n_target << ","
        << n_current << ","
        << substeps << ","
        << dt << ","
        << wall_time_s << ","
        << fps
        << "\n";
}

} // namespace perf

int main(int argc, char* argv[]) {
    const perf::Args args = perf::parse_args(argc, argv);

    constexpr int window_height = 950;
    constexpr int window_width  = 950;

    const sf::Vector2f boundary_center({
        static_cast<float>(window_width) * 0.5f,
        static_cast<float>(window_height) * 0.5f
    });
    const float boundary_radius = 400.0f;

    const float particle_radius_max = 10.0f;
    const float particle_radius_min = 2.0f;
    const float init_velocity_max   = 500.0f;
    const float init_velocity_min   = -500.0f;

    const sf::Vector2f born_position(boundary_center);
    const sf::Vector2f gravity(0.0f, -980.0f);

    const int frame_rate = 60;
    const float DT = 1.0f / static_cast<float>(frame_rate);

    std::mt19937 gen(static_cast<uint32_t>(args.seed));
    std::uniform_real_distribution<float> vel_dist(init_velocity_min, init_velocity_max);
    std::uniform_real_distribution<float> radius_dist(particle_radius_min, particle_radius_max);
    std::uniform_int_distribution<uint8_t> color_dist(0, 255);

    auto spawn_one = [&](Simulator& simulator) {
        Particle p;
        p.setRadius(radius_dist(gen));
        p.setColor({color_dist(gen), color_dist(gen), color_dist(gen)});

        // Random position inside circle, avoiding boundary overlap
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);
        const float theta = 2.0f * 3.1415926535f * u01(gen);
        const float r = (boundary_radius - 2.0f * particle_radius_max) * std::sqrt(u01(gen));

        const sf::Vector2f spawn_pos =
            boundary_center + sf::Vector2f(r * std::cos(theta), r * std::sin(theta));

        // Random initial velocity
        const sf::Vector2f v(vel_dist(gen), vel_dist(gen));

        // Verlet needs previous position too
        const sf::Vector2f prev_pos = spawn_pos - v * DT;

        p.setPosition(prev_pos, spawn_pos);

        simulator.addParticle(p);
    };

    try {
        if (args.bench) {
            Simulator simulator(DT, args.n, boundary_radius, boundary_center, gravity);
            simulator.setSubsteps(args.substeps);

            if (args.spawn == "instant") {
                while (simulator.getNumParticles() < args.n) spawn_one(simulator);
            }

            using clock = std::chrono::steady_clock;
            const auto t0 = clock::now();

            long long frames = 0;
            std::vector<double> evolve_ms;
            evolve_ms.reserve(static_cast<size_t>(args.seconds * frame_rate + 64));

            while (true) {
                if (args.spawn == "gradual" && simulator.getNumParticles() < args.n) {
                    spawn_one(simulator);
                }

                const auto s0 = clock::now();
                simulator.evolve();
                const auto s1 = clock::now();

                const std::chrono::duration<double, std::milli> step = s1 - s0;
                evolve_ms.push_back(step.count());

                frames++;

                const auto now = clock::now();
                const std::chrono::duration<double> elapsed = now - t0;
                if (elapsed.count() >= args.seconds) break;
            }

            const auto t1 = clock::now();
            const std::chrono::duration<double> total = t1 - t0;

            const double seconds = total.count();
            const double avg_fps = (seconds > 0.0) ? (static_cast<double>(frames) / seconds) : 0.0;
            const double avg_ms_frame = (avg_fps > 0.0) ? (1000.0 / avg_fps) : 0.0;

            double sum_ms = 0.0;
            double max_ms = 0.0;
            for (double v : evolve_ms) {
                sum_ms += v;
                if (v > max_ms) max_ms = v;
            }
            const double avg_ms_evolve = evolve_ms.empty() ? 0.0 : (sum_ms / static_cast<double>(evolve_ms.size()));
            const double p50 = perf::percentile(evolve_ms, 50.0);
            const double p95 = perf::percentile(evolve_ms, 95.0);

            const int nan_count = perf::count_nan_particles(simulator);

            std::cout << "BENCH DONE\n";
            std::cout << "backend=" << args.backend
                      << " n_target=" << args.n
                      << " n_final=" << simulator.getNumParticles()
                      << " substeps=" << args.substeps
                      << " dt=" << DT
                      << " seconds=" << seconds
                      << " frames=" << frames
                      << " avg_fps=" << avg_fps
                      << " avg_ms_frame=" << avg_ms_frame
                      << " avg_ms_evolve=" << avg_ms_evolve
                      << " p50_ms_evolve=" << p50
                      << " p95_ms_evolve=" << p95
                      << " max_ms_evolve=" << max_ms
                      << " nan_count=" << nan_count
                      << "\n";

            perf::append_bench_row(
                args.csv_path,
                args.backend,
                "bench",
                args.spawn,
                args.seed,
                args.n,
                simulator.getNumParticles(),
                args.substeps,
                DT,
                static_cast<float>(seconds),
                frames,
                avg_fps,
                avg_ms_frame,
                avg_ms_evolve,
                p50,
                p95,
                max_ms,
                nan_count
            );

            return 0;
        }

        // Demo mode (window)
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 1;

        sf::RenderWindow window(
            sf::VideoMode({window_width, window_height}),
            "Particle Simulation",
            sf::Style::Default,
            sf::State::Windowed,
            settings
        );

        window.setFramerateLimit(frame_rate);

        sf::View view = window.getDefaultView();
        view.setSize({static_cast<float>(window_width), static_cast<float>(-window_height)});

        const int particle_count = args.n;

        Renderer renderer{window, particle_count};

        Simulator simulator(DT, particle_count, boundary_radius, boundary_center, gravity);
        simulator.setSubsteps(args.substeps);

        sf::Clock fps_clock;
        sf::Clock update_clock;
        sf::Clock wall_clock;

        sf::Font font("assets/ABeeZee-Regular.ttf");
        sf::Text fpsText(font);
        fpsText.setCharacterSize(20);
        fpsText.setFillColor(sf::Color::Black);
        fpsText.setOutlineColor(sf::Color::White);
        fpsText.setOutlineThickness(1.0f);
        fpsText.setPosition({10, 10});

        sf::Text objectCountText(font);
        objectCountText.setCharacterSize(20);
        objectCountText.setFillColor(sf::Color::Black);
        objectCountText.setOutlineColor(sf::Color::White);
        objectCountText.setOutlineThickness(1.0f);
        objectCountText.setPosition({10, 30});

        float last_logged_s = 0.0f;
        float last_fps_value = 0.0f;

        while (window.isOpen()) {
            if (update_clock.getElapsedTime().asSeconds() >= 0.1f) {
                const float currentTime = fps_clock.getElapsedTime().asSeconds();
                const float fps = (currentTime > 0.0f) ? (1.0f / currentTime) : 0.0f;
                last_fps_value = fps;
                fpsText.setString("FPS: " + std::to_string(static_cast<int>(fps)));
                update_clock.restart();
            }
            fps_clock.restart();

            objectCountText.setString("# Particles: " + std::to_string(simulator.getNumParticles()));

            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) window.close();
                }
            }

            if (args.spawn == "instant") {
                while (simulator.getNumParticles() < particle_count) spawn_one(simulator);
            } else {
                if (simulator.getNumParticles() < particle_count) spawn_one(simulator);
            }

            simulator.evolve();

            window.clear(sf::Color::White);
            window.setView(view);
            renderer.render(simulator);
            window.setView(window.getDefaultView());
            window.draw(fpsText);
            window.draw(objectCountText);
            window.display();

            if (args.log_demo && !args.csv_path.empty()) {
                const float t = wall_clock.getElapsedTime().asSeconds();
                if (t - last_logged_s >= 1.0f) {
                    perf::append_demo_row(
                        args.csv_path,
                        args.backend,
                        args.spawn,
                        args.seed,
                        particle_count,
                        simulator.getNumParticles(),
                        args.substeps,
                        DT,
                        t,
                        last_fps_value
                    );
                    last_logged_s = t;
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
