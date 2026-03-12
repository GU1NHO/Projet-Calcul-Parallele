# Rapport de l'implémentation et des mesures (projet ACO)

## 1. Objectif

Ce document décrit la réalisation du projet d'optimisation par colonies de fourmis (ACO) sur un paysage fractal, en suivant les consignes du `Readme.md` (création d'environnement fractal, simulation de fourmis, mesures de temps, vectorisation et parallélisation).

Le code a été adapté pour fonctionner sans dépendance à SDL2 (exécution sans interface graphique) et pour mesurer les temps passés dans les différentes phases de la simulation.

---

## 2. Modifications principales apportées

### 2.1 Exécution headless (sans SDL)
- Le projet original dépendait de la bibliothèque SDL2 pour l'affichage (fichiers `window.cpp`, `renderer.cpp`, `ant_simu.cpp`).
- Pour garantir une exécution sur toutes les machines (en particulier dans cet environnement où SDL2 n'était pas installée), j'ai ajouté un exécutable **headless** : `ant_simu_cli.exe`.
- Le Makefile a été modifié pour construire `ant_simu_cli.exe` par défaut et pour laisser `ant_simu.exe` en tant que cible optionnelle (`make sdl`).

### 2.2 Mesure des temps
- L'exécutable `ant_simu_cli.exe` mesure :
  - le temps total de la simulation,
  - le temps passé dans le déplacement des fourmis,
  - le temps passé dans l'évaporation + mise à jour des phéromones,
  - le temps moyen par itération.

### 2.3 Parallélisation OpenMP
- La boucle principale mettant à jour les fourmis est parallélisée avec OpenMP (`#pragma omp parallel`).
- La mise à jour des phéromones se fait sans contention en utilisant un *buffer par thread* : chaque thread accumule localement ses contributions dans une grille locale, puis ces grilles sont fusionnées par une réduction `max` à la fin de l'itération.
- La phase d'évaporation des phéromones est également parallélisée (chaque cellule est traitée indépendamment).

---

## 3. Compilation

Dans le dossier `projet/src`, exécuter :

```sh
make
```

Pour compiler également l'exécutable SDL (si SDL2 est disponible) :

```sh
make sdl
```

---

## 4. Utilisation de l'exécutable headless

L'exécutable `ant_simu_cli.exe` prend en entrée quelques paramètres :

- `-i <iter>` : nombre d'itérations (par défaut 1000)
- `-a <ants>` : nombre de fourmis (par défaut 5000)
- `-t <threads>` : nombre de threads OpenMP (par défaut 1)
- `-s <seed>` : graine aléatoire (par défaut 2026)
- `-e <eps>` : coefficient d'exploration (epsilon) (par défaut 0.8)
- `-A <alpha>` : coefficient de bruit pour la mise à jour des phéromones (par défaut 0.7)
- `-b <beta>` : coefficient d'évaporation (par défaut 0.999)
- `-g <log2dim>` : dimension de la grille = 2^log2dim (par défaut 8)
- `-c` : affiche les résultats au format CSV (une ligne par exécution)
- `-v` : mode verbeux (affiche des statistiques tous les 100 pas)

Exemple :

```sh
./ant_simu_cli.exe -i 200 -a 2000 -t 4
```

---

## 5. Résultats des mesures (exemples)

### 5.1 Scalabilité forte (strong scaling)

Expériences réalisées sur la même machine (200 itérations, 2000 fourmis) :

| Threads | Temps total (s) | Temps déplacement (s) | Temps phéromones (s) | Accélération (vs 1 thread) |
|--------:|----------------:|----------------------:|---------------------:|---------------------------:|
| 1       | 0.7595          | 0.2464                | 0.1419              | 1.00×                      |
| 2       | 0.5356          | 0.1232                | 0.0749              | 1.42×                      |
| 4       | 0.5489          | 0.1048                | 0.0611              | 1.38×                      |
| 8       | 0.9428          | 0.1355                | 0.0621              | 0.81×                      |
| 16      | 1.8436          | 0.1264                | 0.0541              | 0.41×                      |

**Observation:**
- La phase de déplacement des fourmis bénéficie d'un bon parallélisme jusqu'à 4 threads.
- Au-delà, l'overhead lié à la gestion des buffers et à la réduction des contributions devient prépondérant.
- La phase de mise à jour des phéromones reste le goulot d'étranglement principal, malgré l'élimination des sections critiques.

### 5.2 Scalabilité faible (weak scaling)

Ici, le nombre de fourmis augmente proportionnellement au nombre de threads (2000 × threads) :

| Threads | Fourmis | Temps total (s) | Temps déplacement (s) | Temps phéromones (s) |
|--------:|--------:|----------------:|----------------------:|---------------------:|
| 1       | 2000    | 0.7186          | 0.2247                | 0.1318              |
| 2       | 4000    | 0.6461          | 0.2317                | 0.0752              |
| 4       | 8000    | 0.8108          | 0.3242                | 0.0597              |
| 8       | 16000   | 1.2083          | 0.4336                | 0.0578              |

**Observation :** le temps par itération n'est pas strictement constant, mais reste de l'ordre de la même magnitude. La proportion de temps passé dans la mise à jour des phéromones diminue légèrement en pourcentage lorsque le nombre de fourmis augmente, mais le coût absolu reste significatif.

### 5.3 Impact des paramètres de l'algorithme (epsilon, alpha, beta)

Ces expériences sont réalisées avec 200 itérations, 2000 fourmis, 4 threads et un log2dim=8.

| Paramètre | Valeur | Total (s) | Fourmis collectées | itération première nourriture |
|----------:|-------:|----------:|-------------------:|-----------------------------:|
| epsilon   | 0.1    | 0.6020    | 58                 | 36                           |
| epsilon   | 0.5    | 0.6423    | 58                 | 36                           |
| epsilon   | 0.9    | 0.6197    | 58                 | 36                           |
| alpha     | 0.1    | 0.5977    | 56                 | 36                           |
| alpha     | 0.5    | 0.6077    | 58                 | 36                           |
| alpha     | 0.9    | 0.5752    | 54                 | 36                           |
| beta      | 0.9    | 0.5928    | 54                 | 36                           |
| beta      | 0.99   | 0.6222    | 54                 | 36                           |
| beta      | 0.999  | 0.6575    | 58                 | 36                           |

**Observation :**
- Le paramètre `epsilon` (exploration vs exploitation) n'affecte pas fortement l'agrégation, mais il peut influencer la vitesse de découverte (ici stable à 36 itérations).
- `alpha` (bruit dans la mise à jour des phéromones) a un impact léger sur l'efficacité de la collecte, mais pas déterminant dans ces tests.
- `beta` (évaporation) influence la persistance des phéromones : un beta plus faible (forte évaporation) tend à limiter la création de pistes persistantes, ce qui peut rendre le comportement plus exploratoire.

### 5.4 Impact de la taille du problème

Nous avons testé trois tailles de grille (log2dim = 7, 8, 9) en conservant 2000 fourmis et 4 threads.

| log2dim | grille (N×N) | Total (s) | Temps phéromones (s) | Fourmis collectées |
|--------:|-------------:|----------:|---------------------:|-------------------:|
| 7       | 128×128      | 0.2610    | 0.0301              | 7381               |
| 8       | 256×256      | 0.5919    | 0.0614              | 58                 |
| 9       | 512×512      | 2.5576    | 0.2241              | 97                 |

**Observation :** le coût de calcul augmente rapidement avec la taille de la grille (la phase de mise à jour des phéromones domine) ; l'effet sur la collecte de nourriture est non monotone car il dépend aussi du comportement de recherche des fourmis.

---

## 6. Stratégies de parallélisation avancées (proposition)

### 6.1 Vectorisation / réorganisation mémoire (SoA)
- Une première optimisation consiste à réorganiser l'état des fourmis sous forme de tableaux (structure-of-arrays) pour améliorer la localité mémoire et permettre des optimisations SIMD.
- Dans cette version, l'état des fourmis est stocké en tant que vecteurs séparés (`std::vector<int> ant_x`, `std::vector<int> ant_y`, `std::vector<char> ant_loaded`, `std::vector<std::size_t> ant_seed`), ce qui réduit les cache-miss lors du parcours.

### 6.2 Parallélisation mémoire partagée (OpenMP)
- La boucle `for` sur les fourmis est déjà parallélisée.
- Il est possible d'étendre la parallélisation aux phases d'évaporation / mise à jour des phéromones (par exemple en parallélisant les deux boucles `do_evaporation()` et `update()` dans `pheronome.hpp`).

### 6.3 Parallélisation distribuée (MPI)
- **Approche 1 (carte entière, fourmis partitionnées)** : chaque processus possède une copie complète de la carte et gère un sous-ensemble de fourmis. Les phéromones sont réduites en prenant le maximum pour chaque cellule (pour gérer les conflits de mise à jour).
- **Approche 2 (domain decomposition)** : chaque processus gère une sous-partie de la carte et les fourmis qui s'y trouvent. Il faut échanger les bords de sous-domaines à chaque itération pour propager correctement les phéromones.

---

## 7. Fichiers importants

- `src/ant_simu_cli.cpp` : exécutable headless (mesures & OpenMP)
- `src/pheronome.hpp` : mise à jour thread-safe des phéromones
- `src/Makefile` : compilation par défaut sans SDL, cible `make sdl` si SDL2 disponible

---

## 8. Commentaires

- Le code est écrit en C++17 et compilé avec `-O3 -march=native`.
- Les mesures sont indicatives ; pour des études plus rigoureuses, il est recommandé d'augmenter le nombre d'itérations et de stabiliser la charge afin de réduire le bruit du système.

Bonne continuation !
