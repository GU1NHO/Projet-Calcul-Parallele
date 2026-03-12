#include <vector>
#include <iostream>
#include <chrono>
#include <omp.h>

#include "fractal_land.hpp"
#include "pheronome.hpp"
#include "ant.hpp"
#include "rand_generator.hpp"
#include "basic_types.hpp"

struct SimulationParams {
    std::size_t seed = 2026;
    int log2dim = 8;
    unsigned long nbSeeds = 2;
    double deviation = 1.0;
    int nb_ants = 5000;
    double eps = 0.8;
    double alpha = 0.7;
    double beta = 0.999;
    int iter_max = 1000;
    int num_threads = 1;
    bool verbose = false;
};

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "  -i <iter>        : nombre d'iterations (defaut 1000)\n"
              << "  -a <ants>        : nombre de fourmis (defaut 5000)\n"
              << "  -t <threads>     : nombre de threads OpenMP (defaut 1)\n"
              << "  -s <seed>        : graine aleatoire (defaut 2026)\n"
              << "  -v               : verbose (statistiques par iteration)\n";
}

int main(int argc, char* argv[])
{
    SimulationParams params;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i+1 < argc) params.iter_max = std::atoi(argv[++i]);
        else if (arg == "-a" && i+1 < argc) params.nb_ants = std::atoi(argv[++i]);
        else if (arg == "-t" && i+1 < argc) params.num_threads = std::atoi(argv[++i]);
        else if (arg == "-s" && i+1 < argc) params.seed = std::atoi(argv[++i]);
        else if (arg == "-v") params.verbose = true;
        else {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (params.num_threads <= 0) params.num_threads = 1;
    omp_set_num_threads(params.num_threads);

    // Initialisation du terrain fractal.
    auto t1 = std::chrono::steady_clock::now();
    fractal_land land(params.log2dim, params.nbSeeds, params.deviation, params.seed);
    double max_val = 0.0;
    double min_val = 0.0;
    for ( fractal_land::dim_t i = 0; i < land.dimensions(); ++i )
        for ( fractal_land::dim_t j = 0; j < land.dimensions(); ++j ) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }
    double delta = max_val - min_val;
    for ( fractal_land::dim_t i = 0; i < land.dimensions(); ++i )
        for ( fractal_land::dim_t j = 0; j < land.dimensions(); ++j )  {
            land(i,j) = (land(i,j)-min_val)/delta;
        }

    // Parametres du modele
    position_t pos_nest{static_cast<int>(land.dimensions()/2), static_cast<int>(land.dimensions()/2)};
    position_t pos_food{static_cast<int>(land.dimensions()*0.5), static_cast<int>(land.dimensions()*0.5)};

    pheronome phen( land.dimensions(), pos_food, pos_nest, params.alpha, params.beta );
    ant::set_exploration_coef(params.eps);

    // Initialisation de la population de fourmis
    std::vector<position_t> ant_positions(params.nb_ants);
    std::vector<bool> ant_loaded(params.nb_ants, false);
    std::vector<std::size_t> ant_seeds(params.nb_ants);

    for (int i = 0; i < params.nb_ants; ++i) {
        ant_seeds[i] = params.seed + i;
        ant_positions[i].x = rand_int32(0, land.dimensions()-1, ant_seeds[i]);
        ant_positions[i].y = rand_int32(0, land.dimensions()-1, ant_seeds[i]);
    }

    std::size_t food_quantity = 0;
    std::size_t it_first_food = 0;

    // Profiling time counters
    std::chrono::duration<double> time_move(0);
    std::chrono::duration<double> time_pheromone(0);
    std::chrono::duration<double> time_total(0);

    for (int iter = 1; iter <= params.iter_max; ++iter) {
        auto t_iter_start = std::chrono::steady_clock::now();

        // Deplacement des fourmis
        auto t_move_start = std::chrono::steady_clock::now();

        #pragma omp parallel for schedule(static)
        for (int a = 0; a < params.nb_ants; ++a) {
            double consumed_time = 0.;
            while (consumed_time < 1.) {
                const bool loaded = ant_loaded[a];
                int ind_pher = loaded ? 1 : 0;
                position_t old_pos = ant_positions[a];
                position_t new_pos = old_pos;

                double max_phen = std::max({
                    phen(old_pos.x - 1, old_pos.y)[ind_pher],
                    phen(old_pos.x + 1, old_pos.y)[ind_pher],
                    phen(old_pos.x, old_pos.y - 1)[ind_pher],
                    phen(old_pos.x, old_pos.y + 1)[ind_pher]});

                double choix = rand_double(0., 1., ant_seeds[a]);
                if ((choix > params.eps) || (max_phen <= 0.)) {
                    do {
                        new_pos = old_pos;
                        int d = rand_int32(1, 4, ant_seeds[a]);
                        if (d == 1) new_pos.x -= 1;
                        if (d == 2) new_pos.y -= 1;
                        if (d == 3) new_pos.x += 1;
                        if (d == 4) new_pos.y += 1;
                    } while (phen[new_pos][ind_pher] == -1);
                } else {
                    if (phen(old_pos.x - 1, old_pos.y)[ind_pher] == max_phen)
                        new_pos.x -= 1;
                    else if (phen(old_pos.x + 1, old_pos.y)[ind_pher] == max_phen)
                        new_pos.x += 1;
                    else if (phen(old_pos.x, old_pos.y - 1)[ind_pher] == max_phen)
                        new_pos.y -= 1;
                    else
                        new_pos.y += 1;
                }

                consumed_time += land(new_pos.x, new_pos.y);

                // Mise à jour locale du pheromone (prise en compte du maximum si plusieurs fourmis concurrentes)
                phen.mark_pheronome(new_pos);

                ant_positions[a] = new_pos;

                if (new_pos == pos_nest) {
                    if (loaded) {
                        #pragma omp atomic
                        ++food_quantity;
                        if (it_first_food == 0) it_first_food = iter;
                    }
                    ant_loaded[a] = false;
                }
                if (new_pos == pos_food) {
                    ant_loaded[a] = true;
                }
            }
        }

        auto t_move_end = std::chrono::steady_clock::now();
        time_move += (t_move_end - t_move_start);

        // Evaporation + mise à jour des pheromones
        auto t_pher_start = std::chrono::steady_clock::now();
        phen.do_evaporation();
        phen.update();
        auto t_pher_end = std::chrono::steady_clock::now();
        time_pheromone += (t_pher_end - t_pher_start);

        auto t_iter_end = std::chrono::steady_clock::now();
        time_total += (t_iter_end - t_iter_start);

        if (params.verbose && (iter % 100 == 0)) {
            double avg_iter = std::chrono::duration<double>(t_iter_end - t_iter_start).count();
            std::cout << "Iter " << iter << " | food=" << food_quantity << " | iter_time=" << avg_iter << "s\n";
        }
    }

    auto t2 = std::chrono::steady_clock::now();
    auto total = std::chrono::duration<double>(t2 - t1).count();

    std::cout << "--- Simulation terminee ---\n";
    std::cout << "threads: " << params.num_threads << " | ants: " << params.nb_ants << " | iterations: " << params.iter_max << "\n";
    std::cout << "nourriture collecte: " << food_quantity << " (premiere fois a l'iteration " << it_first_food << ")\n";
    std::cout << "temps total: " << total << " s\n";
    std::cout << "temps de deplacement total: " << time_move.count() << " s\n";
    std::cout << "temps de pheromone total: " << time_pheromone.count() << " s\n";
    std::cout << "temps moyen par iteration: " << (time_total.count() / params.iter_max) << " s\n";

    return 0;
}
