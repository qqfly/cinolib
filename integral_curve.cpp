/****************************************************************************
* Italian National Research Council                                         *
* Institute for Applied Mathematics and Information Technologies, Genoa     *
* IMATI-GE / CNR                                                            *
*                                                                           *
* Author: Marco Livesu (marco.livesu@gmail.com)                             *
*                                                                           *
* Copyright(C) 2016                                                         *
* All rights reserved.                                                      *
*                                                                           *
* This file is part of CinoLib                                              *
*                                                                           *
* CinoLib is free software; you can redistribute it and/or modify           *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 3 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
****************************************************************************/
#include "integral_curve.h"
#include "gl/draw_cylinder.h"
#include "gl/draw_arrow.h"
#include "colors.h"
#include "intersection.h"
#include "trimesh/drawable_trimesh.h"
#include "tetmesh/drawable_tetmesh.h"
#include "trimesh/triangle.h"

namespace cinolib
{

template<class Mesh>
CINO_INLINE
IntegralCurve<Mesh>::IntegralCurve(const Mesh        & m,
                                   const VectorField & grad,
                                   const int           source_tid,
                                   const int           source_vid)
    : m_ptr(&m)
    , grad_ptr(&grad)
{
    type = INTEGRAL_CURVE;

    opt.source_tid            = source_tid;
    opt.source_vid            = source_vid;
    opt.source_pos            = m.vertex(source_vid);
    opt.convergence_criterion = STOP_AT_LOCAL_MAX;

    make_curve();
}


template<class Mesh>
CINO_INLINE
IntegralCurve<Mesh>::IntegralCurve(const Mesh        & m,
                                   const VectorField & grad,
                                   const int           source_tid,
                                   const int           source_vid,
                                   const float         stop_at_this_value)
    : m_ptr(&m)
    , grad_ptr(&grad)
{
    type = INTEGRAL_CURVE;

    opt.source_tid            = source_tid;
    opt.source_vid            = source_vid;
    opt.source_pos            = m.vertex(source_vid);
    opt.convergence_criterion = STOP_AT_GIVEN_VAL;
    opt.stop_at_this_value    = stop_at_this_value;

    make_curve();
}

template<class Mesh>
CINO_INLINE
IntegralCurve<Mesh>::IntegralCurve(const Mesh        & m,
                                   const VectorField & grad,
                                   const int           source_tid,
                                   const int           source_vid,
                                   const int           stop_at_this_vertex)
    : m_ptr(&m)
    , grad_ptr(&grad)
{
    type = INTEGRAL_CURVE;

    opt.source_tid            = source_tid;
    opt.source_vid            = source_vid;
    opt.source_pos            = m.vertex(source_vid);
    opt.convergence_criterion = STOP_AT_GIVEN_VTX;
    opt.stop_at_this_vertex   = stop_at_this_vertex; // if you ever run into it....

    make_curve();
}

template<class Mesh>
CINO_INLINE
void IntegralCurve<Mesh>::draw() const
{
    double cylind_rad = m_ptr->bbox().diag()*0.001;

    for(size_t i=1; i<curve.size(); ++i)
    {
        cylinder<vec3d>(curve[i-1].pos, curve[i].pos, cylind_rad, cylind_rad, RED);
        //arrow<vec3d>(curve_samples[i-1].pos, curve_samples[i].pos, cylind_rad, RED);
    }
}


/**********************************
 *
 * TRIMESH TEMPLATE SPECIALIZATIONS
 *
************************************/


template<>
CINO_INLINE
int IntegralCurve<Trimesh>::find_exit_gate(const CurveSample & curr_sample,
                                           const vec3d       & target_dir) const
{
    /* Split the triangle into three sub triangles meeting at <pos>,
     * then find, among the three edges incident to <pos>, the one
     * closest to the target dir. The exit_edge will be either at the
     * left or at the right of such edge. To understand on which side,
     * compare the cross between edge and target dir with the triangle
     * normal. That's it
     */

    vec3d uvw[3] =
    {
        m_ptr->triangle_vertex(curr_sample.elem_id, 0) - curr_sample.pos,
        m_ptr->triangle_vertex(curr_sample.elem_id, 1) - curr_sample.pos,
        m_ptr->triangle_vertex(curr_sample.elem_id, 2) - curr_sample.pos
    };

    std::set< std::pair<double,int> > sorted_by_angle;
    for(int i=0; i<3; ++i)
    {
        sorted_by_angle.insert(std::make_pair(target_dir.angle_rad(uvw[i]),i));
    }

    int   vert  = (*sorted_by_angle.begin()).second;
    vec3d tn    = m_ptr->triangle_normal(curr_sample.elem_id);
    vec3d cross = target_dir.cross(uvw[vert]);

    if (cross.dot(tn) >= 0) return (vert+2)%3; // see TRI_EDGES to understand why....
    return vert;
}



template<>
CINO_INLINE
bool IntegralCurve<Trimesh>::gradient_skins_into(const CurveSample & curr_sample,
                                                 const CurveSample & next_sample) const
{
    vec3d next_target_dir = grad_ptr->vec_at(next_sample.elem_id);
    next_target_dir.normalize();

    int next_exit_edge = find_exit_gate(next_sample, next_target_dir);
    int next_vidA      = m_ptr->triangle_vertex_id(next_sample.elem_id, TRI_EDGES[next_exit_edge][0]);
    int next_vidB      = m_ptr->triangle_vertex_id(next_sample.elem_id, TRI_EDGES[next_exit_edge][1]);
    int next_next_tid  = m_ptr->triangle_adjacent_along(next_sample.elem_id, next_vidA, next_vidB);

    if (next_next_tid == curr_sample.elem_id)
    {
        return true;
    }
    return false;
}


template<>
CINO_INLINE
IntegralCurve<Trimesh>::CurveSample IntegralCurve<Trimesh>::traverse_element(const CurveSample & curr_sample) const
{
    vec3d target_dir = grad_ptr->vec_at(curr_sample.elem_id);
    target_dir.normalize();

    int   exit_edge = find_exit_gate(curr_sample, target_dir);
    int   vidA      = m_ptr->triangle_vertex_id(curr_sample.elem_id, TRI_EDGES[exit_edge][0]);
    int   vidB      = m_ptr->triangle_vertex_id(curr_sample.elem_id, TRI_EDGES[exit_edge][1]);
    vec3d A         = m_ptr->vertex(vidA);
    vec3d B         = m_ptr->vertex(vidB);

    vec3d  V0       = curr_sample.pos;
    vec3d  V1       = A;
    vec3d  e0_dir   = B - A; e0_dir.normalize();
    vec3d  e1_dir   = -target_dir;
    vec3d  e2_dir   = V1 - V0; e2_dir.normalize();

    double V0_ang   = e2_dir.angle_rad(-e1_dir);
    double V2_ang   = e1_dir.angle_rad(-e0_dir);
    double e2_len   = (V1 - V0).length();
    double e0_len   = triangle_law_of_sines(V2_ang, V0_ang, e2_len);

    vec3d next_pos  = V1 + e0_len * e0_dir;
    int   next_tid  = m_ptr->triangle_adjacent_along(curr_sample.elem_id, vidA, vidB);

    CurveSample next_sample;
    next_sample.pos     = next_pos;
    next_sample.elem_id = next_tid;
    next_sample.gate_id = exit_edge;

    if (gradient_skins_into(curr_sample, next_sample))
    {
        // follow the exit edge until the endpoint best aligned
        // with the gradient dir...
        //
        next_sample.vert_id   = ((B-A).dot(target_dir)>0) ? vidB : vidA;
        next_sample.pos       = m_ptr->vertex(next_sample.vert_id);
        next_sample.gate_id   = -1;
        next_sample.elem_id   = -1;
        for(int tid : m_ptr->adj_vtx2tri(next_sample.vert_id))
        {
            if (tid != next_tid && tid != curr_sample.elem_id)
            {
                next_sample.elem_id = tid;
            }
        }
        assert(next_sample.elem_id != -1);
    }

    return next_sample;
}

template <>
CINO_INLINE
bool IntegralCurve<Trimesh>::is_converged(const int curr_tid, const int convergence_criterion) const
{
    switch (convergence_criterion)
    {
        case STOP_AT_LOCAL_MAX :
        {
            for(int i=0; i<3; ++i)
            {
                int vid = m_ptr->triangle_vertex_id(curr_tid,i);
                if (m_ptr->vertex_is_local_maxima(vid)) return true;
            }
        }
        break;

        case STOP_AT_GIVEN_VAL :
        {
            if (m_ptr->triangle_min_u_text(curr_tid) > opt.stop_at_this_value) return true;
        }
        break;

        case STOP_AT_GIVEN_VTX :
        {
            if (m_ptr->triangle_contains_vertex(curr_tid, opt.stop_at_this_vertex)) return true;
        }
        break;

        default: assert("Integral Curve Convergence Error" && false);
    }

    return false;
}


template<>
CINO_INLINE
void IntegralCurve<Trimesh>::make_curve()
{
    CurveSample cs;
    cs.pos     = opt.source_pos;
    cs.elem_id = opt.source_tid;
    cs.vert_id = opt.source_vid;
    curve.push_back(cs);

    bool border_reached, locmax_reached, target_reached;

    do
    {
        curve.push_back( traverse_element(curve.back()) );

        border_reached = (curve.back().elem_id == -1);
        locmax_reached = !border_reached && is_converged(curve.back().elem_id, STOP_AT_LOCAL_MAX);
        target_reached = !border_reached && is_converged(curve.back().elem_id, opt.convergence_criterion);
    }
    while (!border_reached && !locmax_reached && !target_reached);

    if (locmax_reached)
    {
        // append the final segment to the curve
        //
        for(int i=0; i<3; ++i)
        if (m_ptr->vertex_is_local_maxima(m_ptr->triangle_vertex_id(curve.back().elem_id,i)))
        {
            CurveSample cs;
            cs.vert_id = m_ptr->triangle_vertex_id(curve.back().elem_id, i);
            cs.pos     = m_ptr->vertex(cs.vert_id);
            cs.elem_id = curve.back().elem_id;
            curve.push_back(cs);
        }
    }
}


/**********************************
 *
 * TETMESH TEMPLATE SPECIALIZATIONS
 *
************************************/

template<>
CINO_INLINE
int IntegralCurve<Tetmesh>::find_exit_gate(const CurveSample & curr_sample,
                                           const vec3d       & target_dir,
                                                 vec3d       & exit_pos) const
{
    std::cout << curr_sample.elem_id << std::endl;

        exit_pos   = curr_sample.pos;
    int exit_facet = -1;

    for(int facet=0; facet<4; ++facet)
    {
        vec3d f[3] =
        {
            m_ptr->tet_vertex(curr_sample.elem_id, TET_FACES[facet][0]),
            m_ptr->tet_vertex(curr_sample.elem_id, TET_FACES[facet][1]),
            m_ptr->tet_vertex(curr_sample.elem_id, TET_FACES[facet][2])
        };

        std::cout.precision(20);
        std::cout << "ray:\n" << curr_sample.pos << "\n" << target_dir << std::endl;
        std::cout << "tri:\n" << f[0] << "\n" << f[1] << "\n" << f[2] << "\n" << std::endl;

        vec3d inters;
        bool b = intersection::ray_triangle_intersection(curr_sample.pos, target_dir, f[0], f[1], f[2], inters);

        std::cout << b << "\n" << inters << std::endl << std::endl;

        if (b)
        {
            //std::cout << curr_sample.elem_id << " intersection found!" << inters << "\t" << curr_sample.pos << std::endl;
            if ((inters - curr_sample.pos).length() >= (exit_pos - curr_sample.pos).length())
            {
                exit_pos   = inters;
                exit_facet = facet;
            }
        }
    }
    assert(exit_facet != -1);

    return exit_facet;
}

template<>
CINO_INLINE
bool IntegralCurve<Tetmesh>::gradient_skins_into(const CurveSample & curr_sample,
                                                 const CurveSample & next_sample) const
{
    if (next_sample.elem_id == -1) return false;

    vec3d next_target_dir = grad_ptr->vec_at(next_sample.elem_id);
    next_target_dir.normalize();

    vec3d exit_pos;
    int exit_face = find_exit_gate(next_sample, next_target_dir, exit_pos);

    if (m_ptr->adjacent_tet_through_facet(next_sample.elem_id, exit_face) == curr_sample.elem_id)
    {
        return true;
    }
    return false;
}


template<>
CINO_INLINE
IntegralCurve<Tetmesh>::CurveSample IntegralCurve<Tetmesh>::traverse_element(const CurveSample & curr_sample) const
{
    vec3d target_dir = grad_ptr->vec_at(curr_sample.elem_id);
    target_dir.normalize();

    CurveSample next_sample;
    next_sample.gate_id = find_exit_gate(curr_sample, target_dir, next_sample.pos);
    next_sample.elem_id = m_ptr->adjacent_tet_through_facet(curr_sample.elem_id, next_sample.gate_id);

    std::cout << "check if gradient skins into..." << std::endl;

    if (gradient_skins_into(curr_sample, next_sample))
    {
        std::cout << "GRADIENT SKINS INTO!" << std::endl;

        vec3d avg_target_dir = grad_ptr->vec_at(curr_sample.elem_id) + grad_ptr->vec_at(next_sample.elem_id);
        avg_target_dir.normalize();

        vec3d tri[3] = // interface between curr and next element...
        {
            m_ptr->tet_vertex(curr_sample.elem_id, TET_FACES[next_sample.gate_id][0]),
            m_ptr->tet_vertex(curr_sample.elem_id, TET_FACES[next_sample.gate_id][1]),
            m_ptr->tet_vertex(curr_sample.elem_id, TET_FACES[next_sample.gate_id][2]),
        };

        std::set< std::pair<double,int> > sorted_by_angle;
        for(int i=0; i<3; ++i)
        {
            sorted_by_angle.insert(std::make_pair(avg_target_dir.angle_rad(tri[i]-next_sample.pos),i));
        }

        int best = (*sorted_by_angle.begin()).second;
        next_sample.vert_id = m_ptr->tet_vertex_id(curr_sample.elem_id, TET_FACES[next_sample.gate_id][best]);
        next_sample.pos     = tri[best];
        next_sample.gate_id = -1;
        //next_sample.elem_id = -1;

        int new_next_elem = -1;
        for(int tid : m_ptr->adj_vtx2tet(next_sample.vert_id))
        {
            if (tid != next_sample.elem_id && tid != curr_sample.elem_id)
            {
                std::cout << "TID: " << tid << std::endl;
                vec3d exit_pos;
                CurveSample sample;
                sample.elem_id = tid;
                sample.pos = tri[best];
                vec3d dir = grad_ptr->vec_at(tid);
                dir.normalize();
                /*
                 * ci siamo quasi ma occhio a ray_intersection che si perde intersezioni se parti
                 * da uno dei vetici del tet! succede quando gate=-1 .... verifica perchè
                */
                int gate = find_exit_gate(sample, dir, exit_pos);
                if (gate != -1)
                {
                    new_next_elem = tid;
                }
            }
        }
        assert(new_next_elem!=-1);

        next_sample.elem_id = new_next_elem;

        assert(m_ptr->tet_contains_vertex(next_sample.elem_id, next_sample.vert_id));
    }

    std::cout << "check if gradient skins into...done\n" << std::endl;

    return next_sample;
}


template <>
CINO_INLINE
bool IntegralCurve<Tetmesh>::is_converged(const int curr_tid, const int convergence_criterion) const
{
    switch (convergence_criterion)
    {
        case STOP_AT_LOCAL_MAX :
        {
            for(int i=0; i<3; ++i)
            {
                int vid = m_ptr->tet_vertex_id(curr_tid,i);
                if (m_ptr->vertex_is_local_maxima(vid)) return true;
            }
        }
        break;

        case STOP_AT_GIVEN_VAL :
        {
            if (m_ptr->tet_min_u_text(curr_tid) > opt.stop_at_this_value) return true;
        }
        break;

        case STOP_AT_GIVEN_VTX :
        {
            if (m_ptr->tet_contains_vertex(curr_tid, opt.stop_at_this_vertex)) return true;
        }
        break;

        default: assert("Integral Curve Convergence Error" && false);
    }

    return false;
}


template<>
CINO_INLINE
void IntegralCurve<Tetmesh>::make_curve()
{
    CurveSample cs;
    cs.pos = opt.source_pos;
    cs.elem_id = opt.source_tid;
    cs.vert_id = opt.source_vid;
    curve.push_back(cs);

    bool border_reached, locmax_reached, target_reached;

    do
    {
        curve.push_back( traverse_element(curve.back()) );

        border_reached = (curve.back().elem_id == -1);
        locmax_reached = !border_reached && is_converged(curve.back().elem_id, STOP_AT_LOCAL_MAX);
        target_reached = !border_reached && is_converged(curve.back().elem_id, opt.convergence_criterion);
    }
    while (!border_reached && !locmax_reached && !target_reached);

    // append the final segment to the curve
    //
    if (locmax_reached)
    {
        for(int i=0; i<4; ++i)
        if (m_ptr->vertex_is_local_maxima(m_ptr->tet_vertex_id(curve.back().elem_id,i)))
        {
            CurveSample cs;
            cs.pos = m_ptr->tet_vertex(curve.back().elem_id,i);
            cs.elem_id = curve.back().elem_id;
            cs.gate_id = -1;
            curve.push_back(cs);
        }
    }
}

}
