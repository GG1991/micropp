/*
 *  This source code is part of MicroPP: a finite element library
 *  to solve microstructural problems for composite materials.
 *
 *  Copyright (C) - 2018 - Jimmy Aguilar Mena <kratsbinovish@gmail.com>
 *                         Guido Giuntoli <gagiuntoli@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "micro.hpp"


using namespace std;


template <int tdim>
newton_t micropp<tdim>::newton_raphson(ell_matrix *A, double *b, double *u, double *du,
				       const double strain[nvoi], const double *vars_old,
				       const int max_its, const double max_tol, const double rel_tol)
{

	INST_START;

	newton_t newton;

	set_displ_bc(strain, u);

	int its = 0;

	double norm = assembly_rhs(u, vars_old, b);
	const double norm_0 = norm;

	while (its < max_its) {

		newton.norms[its] = norm;

		if (norm < max_tol || norm < norm_0 * rel_tol)
			break;

		assembly_mat(A, u, vars_old);

		double cg_err;
		int cg_its = ell_solve_cgpd(A, b, du, &cg_err);

		newton.solver_its[its] = cg_its;
		newton.solver_norms[its] = cg_err;

		for (int i = 0; i < nn * dim; ++i)
			u[i] += du[i];

		norm = assembly_rhs(u, vars_old, b);

		its++;
	}

	newton.its = its;

	return newton;
}


template class micropp<2>;
template class micropp<3>;
