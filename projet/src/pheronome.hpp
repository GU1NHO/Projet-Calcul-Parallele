#ifndef _PHERONOME_HPP_
#define _PHERONOME_HPP_
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>
#include <omp.h>
#include "basic_types.hpp"

/**
 * @brief Carte des phéronomes
 * @details Gère une carte des phéronomes avec leurs mis à jour ( dont l'évaporation )
 *
 */
class pheronome {
public:
    using size_t      = unsigned long;
    using pheronome_t = std::array< double, 2 >;

    /**
     * @brief Construit une carte initiale des phéronomes
     * @details La carte des phéronomes est initialisées à zéro ( neutre )
     *          sauf pour les bords qui sont marqués comme indésirables
     *
     * @param dim Nombre de cellule dans chaque direction
     * @param alpha Paramètre de bruit
     * @param beta Paramêtre d'évaporation
     */
    pheronome( size_t dim, const position_t& pos_food, const position_t& pos_nest,
               double alpha = 0.7, double beta = 0.9999 )
        : m_dim( dim ),
          m_stride( dim + 2 ),
          m_alpha(alpha), m_beta(beta),
          m_map_of_pheronome( m_stride * m_stride, {{0., 0.}} ),
          m_buffer_pheronome( ),
          m_pos_nest( pos_nest ),
          m_pos_food( pos_food ) 
          {
        m_map_of_pheronome[index(pos_food)][0] = 1.;
        m_map_of_pheronome[index(pos_nest)][1] = 1.;
        cl_update( );
        m_buffer_pheronome = m_map_of_pheronome;
    }
    pheronome( const pheronome& ) = delete;
    pheronome( pheronome&& )      = delete;
    ~pheronome( )                 = default;

    pheronome_t& operator( )( size_t i, size_t j ) {
        return m_map_of_pheronome[( i + 1 ) * m_stride + ( j + 1 )];
    }

    const pheronome_t& operator( )( size_t i, size_t j ) const {
        return m_map_of_pheronome[( i + 1 ) * m_stride + ( j + 1 )];
    }

    pheronome_t get( int i, int j ) const {
        if (i < 0 || j < 0 || i >= static_cast<int>(m_dim) || j >= static_cast<int>(m_dim))
            return {{-1., -1.}};
        return (*this)(static_cast<size_t>(i), static_cast<size_t>(j));
    }

    pheronome_t& operator[] ( const position_t& pos ) {
      return m_map_of_pheronome[index(pos)];
    }

    const pheronome_t& operator[] ( const position_t& pos ) const {
      return m_map_of_pheronome[index(pos)];
    }

    void do_evaporation( ) {
        // Parallel evaporation of pheromones is embarrassingly parallel.
        const std::size_t dim = m_dim;
        #pragma omp parallel for collapse(2) schedule(static)
        for ( std::size_t i = 1; i <= dim; ++i ) {
            for ( std::size_t j = 1; j <= dim; ++j ) {
                auto& cell = m_buffer_pheronome[i * m_stride + j];
                cell[0] *= m_beta;
                cell[1] *= m_beta;
            }
        }
    }

    void mark_pheronome( const position_t& pos ) {
        // Legacy API: not thread-safe. Prefer mark_pheromone_thread when running in parallel.
        mark_pheromone_thread(pos, m_buffer_pheronome);
    }

    void mark_pheromone_thread( const position_t& pos, std::vector<pheronome_t>& out_buffer ) const {
        std::size_t i = pos.x;
        std::size_t j = pos.y;
        assert( i < m_dim );
        assert( j < m_dim );
        const pheronome_t& left_cell   = get( static_cast<int>(i) - 1, static_cast<int>(j) );
        const pheronome_t& right_cell  = get( static_cast<int>(i) + 1, static_cast<int>(j) );
        const pheronome_t& upper_cell  = get( static_cast<int>(i), static_cast<int>(j) - 1 );
        const pheronome_t& bottom_cell = get( static_cast<int>(i), static_cast<int>(j) + 1 );
        double v1_left   = std::max( left_cell[0], 0. );
        double v2_left   = std::max( left_cell[1], 0. );
        double v1_right  = std::max( right_cell[0], 0. );
        double v2_right  = std::max( right_cell[1], 0. );
        double v1_upper  = std::max( upper_cell[0], 0. );
        double v2_upper  = std::max( upper_cell[1], 0. );
        double v1_bottom = std::max( bottom_cell[0], 0. );
        double v2_bottom = std::max( bottom_cell[1], 0. );

        double new_v1 =
            m_alpha * std::max( {v1_left, v1_right, v1_upper, v1_bottom} ) +
            ( 1 - m_alpha ) * 0.25 * ( v1_left + v1_right + v1_upper + v1_bottom );
        double new_v2 =
            m_alpha * std::max( {v2_left, v2_right, v2_upper, v2_bottom} ) +
            ( 1 - m_alpha ) * 0.25 * ( v2_left + v2_right + v2_upper + v2_bottom );

        auto& slot = out_buffer[index(pos)];
        slot[0] = std::max( slot[0], new_v1 );
        slot[1] = std::max( slot[1], new_v2 );
    }

    size_t size() const { return m_map_of_pheronome.size(); }
    size_t stride() const { return m_stride; }

    void reset_buffer_from_map() {
        m_buffer_pheronome = m_map_of_pheronome;
    }

    void init_thread_buffer( std::vector<pheronome_t>& buf ) const {
        buf.assign( size(), {{0., 0.}} );
        // Avoid raising boundary cells from -1 to 0 during reduction
        for ( unsigned long j = 0; j < m_stride; ++j ) {
            buf[j]                            = {{-1., -1.}};
            buf[j + m_stride * ( m_dim + 1 )] = {{-1., -1.}};
            buf[j * m_stride]                 = {{-1., -1.}};
            buf[j * m_stride + m_dim + 1]     = {{-1., -1.}};
        }
    }

    void merge_thread_buffers( const std::vector<std::vector<pheronome_t>>& bufs ) {
        // Merge per-thread buffers into m_buffer_pheronome using max.
        const size_t N = size();
        if (bufs.empty()) return;
        #pragma omp parallel for schedule(static)
        for ( size_t idx = 0; idx < N; ++idx ) {
            auto& dst = m_buffer_pheronome[idx];
            for ( const auto& b : bufs ) {
                dst[0] = std::max(dst[0], b[idx][0]);
                dst[1] = std::max(dst[1], b[idx][1]);
            }
        }
    }

    void update( ) {
        m_map_of_pheronome.swap( m_buffer_pheronome );
        cl_update( );
        m_map_of_pheronome[( m_pos_food.x + 1 ) * m_stride + m_pos_food.y + 1][0] = 1;
        m_map_of_pheronome[( m_pos_nest.x + 1 ) * m_stride + m_pos_nest.y + 1][1] = 1;
    }

private:
    size_t index( const position_t& pos ) const
    {
      return (pos.x+1)*m_stride + pos.y + 1;
    }
    /**
     * @brief Mets à jour les conditions limites sur les cellules fantômes
     * @details Mets à jour les conditions limites sur les cellules fantômes :
     *     pour l'instant, on se contente simplement de mettre ces cellules avec
     *     des valeurs à -1 pour être sûr que les fourmis évitent ces cellules
     */
    void cl_update( ) {
        // On mets tous les bords à -1 pour les marquer comme indésirables :
        for ( unsigned long j = 0; j < m_stride; ++j ) {
            m_map_of_pheronome[j]                            = {{-1., -1.}};
            m_map_of_pheronome[j + m_stride * ( m_dim + 1 )] = {{-1., -1.}};
            m_map_of_pheronome[j * m_stride]                 = {{-1., -1.}};
            m_map_of_pheronome[j * m_stride + m_dim + 1]     = {{-1., -1.}};
        }
    }
    unsigned long              m_dim, m_stride;
    double                     m_alpha, m_beta;
    std::vector< pheronome_t > m_map_of_pheronome, m_buffer_pheronome;
    position_t m_pos_nest, m_pos_food;
};

#endif