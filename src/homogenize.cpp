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

#include <cmath>
#include <cassert>

#include "instrument.hpp"
#include "micro.hpp"


//double micropp_t::get_inv_2(const double *tensor)
//{
//	if (dim == 3)
//		return tensor[0] * tensor[1] +
//			tensor[0] * tensor[2] +
//			tensor[1] * tensor[2] +
//			tensor[3] * tensor[3] +
//			tensor[4] * tensor[4] +
//			tensor[5] * tensor[5];
//
//	return NAN;
//}

template <int tdim>
void micropp<tdim>::set_macro_strain(const int gp_id, const double *macro_strain)
{
	assert(gp_id < ngp);
	assert(ngp > 0);
	for (int i = 0; i < nvoi; ++i)
		gp_list[gp_id].macro_strain[i] = macro_strain[i];
}


template <int tdim>
void micropp<tdim>::get_macro_stress(const int gp_id, double *macro_stress)
{
	assert(gp_id < ngp);
	assert(ngp > 0);
	for (int i = 0; i < nvoi; ++i)
		macro_stress[i] = gp_list[gp_id].macro_stress[i];
}


template <int tdim>
void micropp<tdim>::get_macro_ctan(const int gp_id, double *macro_ctan)
{
	assert(gp_id < ngp);
	assert(ngp > 0);
	for (int i = 0; i < (nvoi * nvoi); ++i)
		macro_ctan[i] = gp_list[gp_id].macro_ctan[i];
}

template <int tdim>
void micropp<tdim>::homogenize()
{
	INST_START;

	for (int igp = 0; igp < ngp; ++igp) {
		gp_t * const gp_ptr = &gp_list[igp];

		inv_max = -1.0e10;

		if (is_linear(gp_ptr->macro_strain) && (!gp_ptr->allocated)) {

			// S = CL : E
			for (int i = 0; i < nvoi; ++i) {
				gp_ptr->macro_stress[i] = 0.0;
				for (int j = 0; j < nvoi; ++j)
					gp_ptr->macro_stress[i] += ctan_lin[i * nvoi + j]
						* gp_ptr->macro_strain[j];
			}
			// C = CL
			for (int i = 0; i < nvoi; ++i)
				for (int j = 0; j < nvoi; ++j)
					gp_ptr->macro_ctan[i * nvoi + j] = ctan_lin[i * nvoi + j];

			for (int i = 0; i < (nvoi + 1); ++i) {
				gp_ptr->nr_its[i] = 0;
				gp_ptr->nr_err[i] = 0.0;
			}

		} else {

			if (!gp_ptr->allocated) {
				vars_old = vars_old_aux;
				vars_new = vars_new_aux;
				memset(vars_old, 0, num_int_vars * sizeof(double));
				memset(u, 0.0, nn * dim * sizeof(double));
			} else {
				for (int i = 0; i < (nn * dim); ++i)
					u[i] = gp_ptr->u_n[i];
				vars_old = gp_ptr->int_vars_n;
				vars_new = gp_ptr->int_vars_k;
			}

			// SIGMA
			int nr_its;
			bool nl_flag;
			double nr_err;

			set_displ(gp_ptr->macro_strain);  // Acts on u
			newton_raphson(&nl_flag, &nr_its, &nr_err);
			calc_ave_stress(gp_ptr->macro_stress);

			if (nl_flag) {
				if (!gp_ptr->allocated) {
					gp_ptr->allocate(num_int_vars, nn, dim);
					memcpy(gp_ptr->int_vars_k, vars_new, num_int_vars * sizeof(double));
				}

				for (int i = 0; i < nn * dim; ++i)
					gp_ptr->u_k[i] = u[i];
			}

			gp_ptr->nr_its[0] = nr_its;
			gp_ptr->nr_err[0] = nr_err;

			// CTAN
			double eps_1[6], sig_0[6], sig_1[6];
			const double dEps = 1.0e-8;
			for (int v = 0; v < nvoi; ++v)
				sig_0[v] = gp_ptr->macro_stress[v];

            vars_new = NULL;

			for (int i = 0; i < nvoi; ++i) {
				for (int v = 0; v < nvoi; ++v)
					eps_1[v] = gp_ptr->macro_strain[v];
				eps_1[i] += dEps;

				set_displ(eps_1);   // Acts on u
				newton_raphson(&nl_flag, &nr_its, &nr_err);
				calc_ave_stress(sig_1);
				for (int v = 0; v < nvoi; ++v)
					gp_ptr->macro_ctan[v * nvoi + i] = (sig_1[v] - sig_0[v]) / dEps;

				gp_ptr->nr_its[i + 1] = nr_its;
				gp_ptr->nr_err[i + 1] = nr_err;
			}
		}
		gp_ptr->inv_max = inv_max;
	}
}

template <int tdim>
void micropp<tdim>::update_vars()
{
	for (int igp = 0; igp < ngp; ++igp)
		gp_list[igp].update_vars();
}

// Explicit instantiation
template class micropp<2>;
template class micropp<3>;
