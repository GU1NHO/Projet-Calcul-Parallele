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
- La boucle principale mettant à jour les fourmis est parallélisée avec OpenMP (`#pragma omp parallel for`).
- La mise à jour des phéromones par fourmi est rendue sûre pour le parallélisme en faisant un `max` atomique/critique sur la case modifiée (voir `pheronome.hpp`).

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
- `-v` : mode verbeux (affiche des statistiques tous les 100 pas)

Exemple :

```sh
./ant_simu_cli.exe -i 200 -a 2000 -t 4
```

---

## 5. Résultats des mesures (exemples)

Expériences réalisées sur la même machine (200 itérations, 2000 fourmis) :

| Threads | Temps total (s) | Temps déplacement (s) | Temps phéromones (s) | Accélération totale (vs 1 thread) |
|--------:|----------------:|----------------------:|---------------------:|----------------------------------:|
| 1       | 0.2855          | 0.1653                | 0.1013              | 1.00×                             |
| 2       | 0.2495          | 0.1185                | 0.1149              | 1.14×                             |
| 4       | 0.2153          | 0.0849                | 0.1143              | 1.33×                             |
| 8       | 0.2079          | 0.0703                | 0.1193              | 1.37×                             |

**Observation:** La parallélisation améliore principalement la phase de déplacement des fourmis, mais la phase de mise à jour des phéromones reste majoritaire et limite l'accélération.

---

## 6. Stratégies de parallélisation avancées (proposition)

### 6.1 Vectorisation / réorganisation mémoire (SoA)
- Une première optimisation consiste à réorganiser l'état des fourmis sous forme de tableaux (structure-of-arrays) pour améliorer la localité et permettre des optimisations SIMD.
- Dans cette version, la structure `std::vector<position_t>` + `std::vector<bool>` + `std::vector<std::size_t>` est utilisée, ce qui se rapproche d'un modèle vectorisé.

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
