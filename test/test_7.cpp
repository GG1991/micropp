/*
 *  This is a test example for MicroPP: a finite element library
 *  to solve microstructural problems for composite materials.
 *
 *  Copyright (C) - 2018 - Guido Giuntoli <gagiuntoli@gmail.com>
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

#include <iostream>
#include <iomanip>

#include <ctime>

#include "micro.hpp"

using namespace std;

int main (int argc, char *argv[])
{
	int dim = 3;
	int nx = 10;
	int ny = 10;
	int nz = 10;

	int size[3];
	size[0] = nx;
	size[1] = ny;
	size[2] = nz;

	int micro_type = 1; // 2 materiales matriz y fibra (3D esfera en matriz)
	double micro_params[5];
	micro_params[0] = 1.0; // lx
	micro_params[1] = 1.0; // ly
	micro_params[2] = 1.0; // lz
	micro_params[3] = 0.1; // grosor capa de abajo
	micro_params[4] = 0; // INV_MAX

	int mat_types[2]; // dos materiales lineales (type = 0)
	mat_types[0] = 1;
	mat_types[1] = 0;

	double mat_params[2*MAX_MAT_PARAM];
	mat_params[0*MAX_MAT_PARAM + 0] = 1.0e6; // E
	mat_params[0*MAX_MAT_PARAM + 1] = 0.3;   // nu
	mat_params[0*MAX_MAT_PARAM + 2] = 5.0e4; // Sy
	mat_params[0*MAX_MAT_PARAM + 3] = 5.0e4; // Ka

	mat_params[1*MAX_MAT_PARAM + 0] = 1.0e6;
	mat_params[1*MAX_MAT_PARAM + 1] = 0.3;
	mat_params[1*MAX_MAT_PARAM + 2] = 1.0e4;
	mat_params[1*MAX_MAT_PARAM + 3] = 0.0e-1;

	micropp_t micro (dim, size, micro_type, micro_params, mat_types, mat_params);

	int time_steps = 80;
	double MacroStress[6], MacroCtan[36];
	double d_eps = 0.01;
	int dir = 2;

	double eps[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	for (int t = 0; t < time_steps; ++t)
	{
		cout << "time step = " << t << endl;
		if (t<30)
			eps[dir] += d_eps;
		else if (t<80)
			eps[dir] -= d_eps;
		else if (t<130)
			eps[dir] += d_eps;
		else if (t<250)
			eps[dir] -= d_eps;
		else
			eps[dir] += d_eps;

		micro.set_macro_strain(1, eps);
		micro.homogenize();

		micro.get_macro_stress(1, MacroStress);

		micro.update_vars();
		micro.write_info_files ();

		cout << "eps = " << eps[dir] << endl;
		cout
			<< "MacroStress = "
			<< MacroStress[0] << " " << MacroStress[1] << " " << MacroStress[2] << " "
			<< MacroStress[3] << " " << MacroStress[4] << " " << MacroStress[5]
			<< endl;

		cout << endl;
		micro.output (t, 1);
	}
	return 0;
}