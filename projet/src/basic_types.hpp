#ifndef _BASIC_TYPES_HPP_
#define _BASIC_TYPES_HPP_

// Définitions de types minimales pour éviter la dépendance à SDL2 dans l'exécution sans interface graphique.
// position_t est utilisé pour stocker des coordonnées dans la grille.
struct position_t { int x; int y; };

inline bool operator == ( const position_t& pos1, const position_t& pos2 )
{
    return (pos1.x == pos2.x ) and (pos1.y == pos2.y);
}

using dimension_t=std::pair<std::size_t,std::size_t>;


#endif