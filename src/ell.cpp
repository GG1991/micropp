#include "ell.h"
#include <iostream>
#include <iomanip> // print with format
#include <cmath>

using namespace std;

void ell_free (ell_matrix &m)
{
  if (m.cols != NULL) free(m.cols);
  if (m.vals != NULL) free(m.vals);
}

int ell_set_zero_mat (ell_matrix * m)
{
  for (int i=0; i<m->nrow; i++)
    for (int j=0; j<m->nnz; j++)
      m->vals[i*m->nnz + j] = 0.0;
  return 0;
}

void ell_mvp_2D (ell_matrix * m, double *x, double *y)
{
  //  y = m * x
  for (int i=0; i<m->nrow; i++) {
    y[i] = 0;
    for (int j=0; j<m->nnz; j++)
      y[i] += m->vals[(i*m->nnz) + j] * x[m->cols[(i*m->nnz) + j]];
  }
}

int ell_solve_jacobi_2D (ell_solver *solver, ell_matrix * m, int nFields, int nx, int ny, double *b, double *x)
{
  /* A = K - N  
   * K = diag(A)
   * N_ij = -a_ij for i!=j  and =0 if i=j
   * x_(i) = K^-1 * ( N * x_(i-1) + b )
   */
  if (m == NULL || b == NULL || x == NULL) return 1;
  
  double *k = (double*)malloc (m->nrow * sizeof(double)); // K = diag(A)
  double *e_i = (double*)malloc(m->nrow * sizeof(double));
  
  int nn = nx*ny;
  for (int i=0; i<nn; i++) {
    for (int d=0; d<nFields; d++) {
      k[i*nFields + d] = 1 / m->vals[i*nFields*m->nnz + 4*nFields + d*m->nnz + d];
    }
  }

  int its = 0;
  int max_its = solver->max_its;
  double err;
  double min_tol = solver->min_tol;

  while (its < max_its) {

    err = 0;
    int i = 0;
    while (i < m->nrow) {
      double aux = 0.0; // sum_(j!=i) a_ij * x_j
      int j = 0;
      while (j < m->nnz) {
        if (m->cols[i*m->nnz + j] == -1) break;
        if (m->cols[i*m->nnz + j] != i)
	  aux += m->vals[i*m->nnz + j] * x[m->cols[i*m->nnz + j]];
	j++;
      }
      x[i] = k[i] * (-1*aux + b[i]);
      i++;
    }

    err = 0;
    ell_mvp_2D (m, x, e_i);
    for (int i = 0 ; i < m->nrow ; i++){
      e_i[i] -= b[i];
      err += e_i[i] * e_i[i];
    }
    err = sqrt(err); if (err < min_tol) break;
    its ++;
  }
  solver->err = err;
  solver->its = its;
  return 0;
}

int ell_solve_cgpd_2D (ell_solver *solver, ell_matrix * m, int nFields, int nx, int ny, double *b, double *x)
{
  /* cg with jacobi preconditioner
   * r_1 residue in actual iteration
   * z_1 = K^-1 * r_0 actual auxiliar vector
   * rho_0 rho_1 = r_0^t * z_1 previous and actual iner products <r_i, K^-1, r_i>
   * p_1 actual search direction
   * q_1 = A*p_1 auxiliar vector
   * d_1 = rho_0 / (p_1^t * q_1) actual step
   * x_1 = x_0 - d_1 * p_1
   * r_1 = r_0 - d_1 * q_1
  */
  if (m == NULL || b == NULL || x == NULL) return 1;
  
  int its = 0;
  double *k = (double*)malloc(m->nrow * sizeof(double)); // K = diag(A)
  double *r = (double*)malloc(m->nrow * sizeof(double));
  double *z = (double*)malloc(m->nrow * sizeof(double));
  double *p = (double*)malloc(m->nrow * sizeof(double));
  double *q = (double*)malloc(m->nrow * sizeof(double));
  double rho_0, rho_1, d;
  double err;

  int nn = nx * ny;

  for (int i=0; i<nn; i++) {
    for (int d=0; d<nFields; d++) {
      k[i*nFields + d] = 1 / m->vals[i*nFields*m->nnz + 4*nFields + d*m->nnz + d];
    }
  }

  ell_mvp_2D (m, x, r);

  for (int i=0; i<m->nrow; i++)
    r[i] -= b[i];

  do {

    err = 0;
    for (int i=0; i<m->nrow; i++)
      err += r[i] * r[i];
//    cout << "cg_err = " << err << endl;
    err = sqrt(err); if (err < solver->min_tol) break;

    for (int i=0 ; i<m->nrow; i++)
      z[i] = k[i] * r[i];

    rho_1 = 0.0;
    for (int i=0; i<m->nrow; i++)
      rho_1 += r[i] * z[i];

    if (its == 0) {
      for (int i=0 ; i<m->nrow; i++)
	p[i] = z[i];
    } else {
      double beta = rho_1 / rho_0;
      for (int i=0; i<m->nrow; i++)
	p[i] = z[i] + beta * p[i];
    }

    ell_mvp_2D (m, p, q);
    double aux = 0;
    for (int i=0 ; i<m->nrow; i++)
      aux += p[i] * q[i];
    d = rho_1 / aux;

    for (int i=0; i<m->nrow; i++) {
      x[i] -= d * p[i];
      r[i] -= d * q[i];
    }

    rho_0 = rho_1;
    its ++;

  } while (its < solver->max_its);

  solver->err = err;
  solver->its = its;

  free(k);
  free(r);
  free(z);
  free(p);
  free(q);

  return 0;
}

int ell_solve_cgpd_struct (ell_solver *solver, ell_matrix * m, int nFields, int dim, int nn, double *b, double *x)
{
  /* cg with jacobi preconditioner
   * r_1 residue in actual iteration
   * z_1 = K^-1 * r_0 actual auxiliar vector
   * rho_0 rho_1 = r_0^t * z_1 previous and actual iner products <r_i, K^-1, r_i>
   * p_1 actual search direction
   * q_1 = A*p_1 auxiliar vector
   * d_1 = rho_0 / (p_1^t * q_1) actual step
   * x_1 = x_0 - d_1 * p_1
   * r_1 = r_0 - d_1 * q_1
  */
  if (m == NULL || b == NULL || x == NULL) return 1;
  
  int its = 0;
  double *k = (double*)malloc(m->nrow * sizeof(double)); // K = diag(A)
  double *r = (double*)malloc(m->nrow * sizeof(double));
  double *z = (double*)malloc(m->nrow * sizeof(double));
  double *p = (double*)malloc(m->nrow * sizeof(double));
  double *q = (double*)malloc(m->nrow * sizeof(double));
  double rho_0, rho_1, d;
  double err;

  if (dim == 2) {
    for (int i=0; i<nn; i++) {
      for (int d=0; d<nFields; d++) {
	k[i*nFields + d] = 1 / m->vals[i*nFields*m->nnz + 4*nFields + d*m->nnz + d];
      }
    }
  } else if (dim == 3) {
    for (int i=0; i<nn; i++) {
      for (int d=0; d<nFields; d++) {
	k[i*nFields + d] = 1 / m->vals[i*nFields*m->nnz + 13*nFields + d*m->nnz + d];
      }
    }
  }

  ell_mvp_2D (m, x, r);

  for (int i=0; i<m->nrow; i++)
    r[i] -= b[i];

  do {

    err = 0;
    for (int i=0; i<m->nrow; i++)
      err += r[i] * r[i];
    err = sqrt(err); if (err < solver->min_tol) break;
    cout << "cg_err = " << err << endl;

    for (int i=0 ; i<m->nrow; i++)
      z[i] = k[i] * r[i];

    rho_1 = 0.0;
    for (int i=0; i<m->nrow; i++)
      rho_1 += r[i] * z[i];

    if (its == 0) {
      for (int i=0 ; i<m->nrow; i++)
	p[i] = z[i];
    } else {
      double beta = rho_1 / rho_0;
      for (int i=0; i<m->nrow; i++)
	p[i] = z[i] + beta * p[i];
    }

    ell_mvp_2D (m, p, q);
    double aux = 0;
    for (int i=0 ; i<m->nrow; i++)
      aux += p[i] * q[i];
    d = rho_1 / aux;

    for (int i=0; i<m->nrow; i++) {
      x[i] -= d * p[i];
      r[i] -= d * q[i];
    }

    rho_0 = rho_1;
    its ++;

  } while (its < solver->max_its);

  solver->err = err;
  solver->its = its;

  free(k);
  free(r);
  free(z);
  free(p);
  free(q);

  return 0;
}

int ell_print_full (ell_matrix * m)
{
  if (m == NULL) return 1;
  if (m->vals == NULL || m->cols == NULL) return 2;
  double val;
  for (int i = 0 ; i < m->nrow ; i++) {
    for (int j = 0 ; j < m->ncol ; j++) {
      if (ell_get_val(m, i, j, &val) == 0) {
	cout << setw(4) << val << " ";
      } else return 1;
    }
    cout << endl;
  }
  return 0;
}

int ell_print (ell_matrix * m)
{
  if (m == NULL) return 1;
  if (m->vals == NULL || m->cols == NULL) return 2;

  cout << "Cols = " << endl;
  for (int i=0; i<m->nrow; i++) {
    for (int j=0; j<m->nnz; j++) {
      cout << setw(7) << setprecision (4) << m->cols[i*m->nnz + j] << " ";
    }
    cout << endl;
  }

  cout << "Vals = " << endl;
  for (int i=0; i<m->nrow; i++) {
    for (int j=0; j<m->nnz; j++) {
      cout << setw(7) << setprecision (4) << m->vals[i*m->nnz + j] << " ";
    }
    cout << endl;
  }
  return 0;
}

void ell_add_2D (ell_matrix &m, int e, double *Ae, int nFields, int nx, int ny)
{
  // assembly Ae in 2D structured grid representation
  // nFields : number of scalar components on each node

  const int npe = 4;
  int nnz = m.nnz;
  int cols_row_0[4] = {4,5,8,7};
  int cols_row_1[4] = {3,4,7,6};
  int cols_row_2[4] = {0,1,4,3};
  int cols_row_3[4] = {1,2,5,4};

  int xfactor = e%(nx-1);
  int yfactor = e/(ny-1);

  int n0 = yfactor     * nx + xfactor     ;
  int n1 = yfactor     * nx + xfactor + 1 ;
  int n2 = (yfactor+1) * nx + xfactor + 1 ;
  int n3 = (yfactor+1) * nx + xfactor     ;

  for (int i=0; i<nFields; i++) {
    for (int n=0; n<npe; n++) {
      for (int j=0; j<nFields; j++) {
	m.vals[n0*nFields*nnz + i*nnz + cols_row_0[n]*nFields + j] += Ae[0*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n1*nFields*nnz + i*nnz + cols_row_1[n]*nFields + j] += Ae[1*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n2*nFields*nnz + i*nnz + cols_row_2[n]*nFields + j] += Ae[2*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n3*nFields*nnz + i*nnz + cols_row_3[n]*nFields + j] += Ae[3*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
      }
    }
  }

}

void ell_add_3D (ell_matrix &m, int ex, int ey, int ez, double *Ae, int nFields, int nx, int ny, int nz)
{
  // assembly Ae in 3D structured grid representation
  // nFields : number of scalar components on each node

  int npe = 8;

  int nnz = m.nnz;
  int cols_row_0[8] = {13,14,16,17,22,23,25,26};
  int cols_row_1[8] = {12,13,15,16,21,22,24,25};
  int cols_row_2[8] = {9 ,10,12,13,18,19,21,22};
  int cols_row_3[8] = {10,11,13,14,19,20,22,23};
  int cols_row_4[8] = { 0, 1, 3, 4, 9,10,12,13};
  int cols_row_5[8] = { 1, 2, 4, 5,10,11,13,14};
  int cols_row_6[8] = { 4, 5, 7, 8,13,14,16,17};
  int cols_row_7[8] = { 3, 4, 6, 7,12,13,15,16};

  int n0 = ez*(nx*ny) + ey*nx + ex;
  int n1 = ez*(nx*ny) + ey*nx + ex + 1;
  int n2 = ez*(nx*ny) + ey*nx + ex + 1;
  int n3 = ez*(nx*ny) + ey*nx + ex;
  int n4 = n0 + nx*ny;
  int n5 = n1 + nx*ny;
  int n6 = n2 + nx*ny;
  int n7 = n3 + nx*ny;

  for (int i=0; i<nFields; i++) {
    for (int n=0; n<npe; n++) {
      for (int j=0; j<nFields; j++) {
	m.vals[n0*nFields*nnz + i*nnz + cols_row_0[n]*nFields + j] += Ae[0*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n1*nFields*nnz + i*nnz + cols_row_1[n]*nFields + j] += Ae[1*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n2*nFields*nnz + i*nnz + cols_row_2[n]*nFields + j] += Ae[2*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n3*nFields*nnz + i*nnz + cols_row_3[n]*nFields + j] += Ae[3*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n4*nFields*nnz + i*nnz + cols_row_4[n]*nFields + j] += Ae[4*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n5*nFields*nnz + i*nnz + cols_row_5[n]*nFields + j] += Ae[5*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n6*nFields*nnz + i*nnz + cols_row_6[n]*nFields + j] += Ae[6*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
	m.vals[n7*nFields*nnz + i*nnz + cols_row_7[n]*nFields + j] += Ae[7*(npe*nFields)*nFields + i*(npe*nFields) + n*nFields + j];
      }
    }
  }

}

void ell_set_bc_2D (ell_matrix &m, int nFields, int nx, int ny)
{
  // Sets 1 on the diagonal of the boundaries and does 0 on the columns corresponding to that values

  int nnz = m.nnz;

  // y=0
  for (int d=0; d<nFields; d++){
    for (int n=0; n<nx; n++){
      for (int j=0; j<nnz; j++) {
	m.vals[n*nFields*nnz + d*nnz + j] = 0;
      } 
      m.vals[n*nFields*nnz + d*nnz + 4*nFields + d] = 1;
    }
  }
  // y=ly
  for (int d=0; d<nFields; d++){
    for (int n=0; n<nx; n++){
      for (int j=0; j<nnz; j++) {
	m.vals[(n+(ny-1)*nx)*nFields*nnz + d*nnz + j] = 0;
      } 
      m.vals[(n+(ny-1)*nx)*nFields*nnz + d*nnz + 4*nFields + d] = 1;
    }
  }
  // x=0
  for (int d=0; d<nFields; d++){
    for (int n=0; n<ny-2; n++){
      for (int j=0; j<nnz; j++) {
	m.vals[(n+1)*nx*nFields*nnz + d*nnz + j] = 0;
      } 
      m.vals[(n+1)*nx*nFields*nnz + d*nnz + 4*nFields + d] = 1;
    }
  }
  // x=lx
  for (int d=0; d<nFields; d++){
    for (int n=0; n<ny-2; n++){
      for (int j=0; j<nnz; j++) {
	m.vals[((n+2)*nx-1)*nFields*nnz + d*nnz + j] = 0;
      } 
      m.vals[((n+2)*nx-1)*nFields*nnz + d*nnz + 4*nFields + d] = 1;
    }
  }

  // internal nodes next to the boundary

  // y = hy
  for (int d1=0; d1<nFields; d1++){
    int n = nx + 2;
    for (int i=0; i<nx-4; i++){
      for (int d2=0; d2<nFields; d2++){
	m.vals[n*nFields*nnz + d1*nnz + 0*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 1*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 2*nFields + d2] = 0;
      }
      n += 1;
    }
  }

  // y = ly - hy
  for (int d1=0; d1<nFields; d1++){
    int n = (ny-2)*nx + 2;
    for (int i=0; i<nx-4; i++){
      for (int d2=0; d2<nFields; d2++){
	m.vals[n*nFields*nnz + d1*nnz + 6*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 7*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 8*nFields + d2] = 0;
      }
      n += 1;
    }
  }

  // x = hx
  for (int d1=0; d1<nFields; d1++){
    int n = 2*nx + 1;
    for (int i=0; i<ny-4; i++){
      for (int d2=0; d2<nFields; d2++){
	m.vals[n*nFields*nnz + d1*nnz + 0*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 3*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 6*nFields + d2] = 0;
      }
      n += nx;
    }
  }

  // x = lx - hx
  for (int d1=0; d1<nFields; d1++){
    int n = 4*nx - 1; 
    for (int i=0; i<ny-4; i++){
      for (int d2=0; d2<nFields; d2++){
	m.vals[n*nFields*nnz + d1*nnz + 2*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 5*nFields + d2] = 0;
	m.vals[n*nFields*nnz + d1*nnz + 8*nFields + d2] = 0;
      }
      n += nx;
    }
  }

  // x = hx , y = hy
  for (int d1=0; d1<nFields; d1++){
    int n = nx + 1;
    for (int d2=0; d2<nFields; d2++){
      m.vals[n*nFields*nnz + d1*nnz + 0*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 1*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 2*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 3*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 6*nFields + d2] = 0;
    }
  }

  // x = lx - hx , y = hy
  for (int d1=0; d1<nFields; d1++){
    int n = 2*nx - 2;
    for (int d2=0; d2<nFields; d2++){
      m.vals[n*nFields*nnz + d1*nnz + 0*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 1*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 2*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 5*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 8*nFields + d2] = 0;
    }
  }

  // x = lx - hx , y = ly - hy
  for (int d1=0; d1<nFields; d1++){
    int n = (ny-1)*nx - 1;
    for (int d2=0; d2<nFields; d2++){
      m.vals[n*nFields*nnz + d1*nnz + 2*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 5*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 6*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 7*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 8*nFields + d2] = 0;
    }
  }

  // x = hx , y = ly - hy
  for (int d1=0; d1<nFields; d1++){
    int n = (ny-2)*nx + 1;
    for (int d2=0; d2<nFields; d2++){
      m.vals[n*nFields*nnz + d1*nnz + 0*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 3*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 6*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 7*nFields + d2] = 0;
      m.vals[n*nFields*nnz + d1*nnz + 8*nFields + d2] = 0;
    }
  }

}

void ell_init_2D (ell_matrix &m, int nFields, int nx, int ny)
{
  int nn = nx*ny;
  m.nnz   = 9 * nFields;
  m.nrow  = nn * nFields;
  m.ncol = nn * nFields;
  m.cols = (int*)malloc(m.nnz * m.nrow * sizeof(int));
  m.vals = (double*)malloc(m.nnz * m.nrow * sizeof(double));

  int nnz = m.nnz;

  for (int i=0; i<nn; i++){
    // the coorners
    if (i == 0) {       
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = (i + 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = (i + nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = (i + nx + 1)*nFields + d2;
	}
      }
    }
    else if (i == nx-1) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = (i - 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = (i + nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = (i + nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
    else if (i == nx*ny-1) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = (i - nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = (i - nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = (i - 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
    else if (i == (ny-1)*nx) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = (i - nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = (i - nx + 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = (i + 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
    // y=0
    else if (i < nx) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = (i - 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = (i + 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = (i + nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = (i + nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = (i + nx + 1)*nFields + d2;
	}
      }
    }
    // y=ly
    else if (i > (ny-1)*nx) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = (i - nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = (i - nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = (i - nx + 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = (i - 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = (i + 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
    // x=0
    else if ((i%nx) == 0) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = (i - nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = (i - nx + 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = (i + 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = (i + nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = (i + nx + 1)*nFields + d2;
	}
      }
    }
    // x=ly
    else if ((i+1)%nx == 0) {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = (i - nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = (i - nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = 0; 
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = (i - 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = 0;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = (i + nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
    // internal node
    else {
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[i*nFields*nnz + 0*nFields + nnz*d1 + d2] = (i - nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 1*nFields + nnz*d1 + d2] = (i - nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 2*nFields + nnz*d1 + d2] = (i - nx + 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 3*nFields + nnz*d1 + d2] = (i - 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 4*nFields + nnz*d1 + d2] = (i         )*nFields + d2;
	  m.cols[i*nFields*nnz + 5*nFields + nnz*d1 + d2] = (i + 1     )*nFields + d2;
	  m.cols[i*nFields*nnz + 6*nFields + nnz*d1 + d2] = (i + nx - 1)*nFields + d2;
	  m.cols[i*nFields*nnz + 7*nFields + nnz*d1 + d2] = (i + nx    )*nFields + d2;
	  m.cols[i*nFields*nnz + 8*nFields + nnz*d1 + d2] = (i + nx + 1)*nFields + d2;
	}
      }
    }
  }

}

void ell_init_3D (ell_matrix &m, int nFields, int nx, int ny, int nz)
{
  int nn = nx * ny * nz;
  m.nnz = 27 * nFields;
  m.nrow = nn * nFields;
  m.ncol = nn * nFields;
  m.cols = (int*)malloc(m.nnz * m.nrow * sizeof(int));
  m.vals = (double*)malloc(m.nnz * m.nrow * sizeof(double));

  int nnz = m.nnz;

  // x=0 y=0 z=0
  nn = 0;
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
    }
  }

  // x=lx y=0 z=0
  nn = nx-1;
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0; 
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }

  // x=lx y=ly z=0
  nn = nx*ny - 1;
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }

  // x=0 y=ly z=0
  nn = nx*(ny-1);
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }
  // x=0 y=0 z=lz 
  nn = nx*ny*(nz-1);
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }

  // x=lx y=0 z=lz 
  nn = nx*ny*(nz-1) + nx - 1;
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }

  // x=lx y=ly z=lz 
  nn = nx*ny*(nz-1) + nx*ny - 1;
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }

  // x=0 y=ly z=lz 
  nn = nx*ny*(nz-1) + nx*ny - 1;
  for (int d1=0; d1<nFields; d1++){
    for (int d2=0; d2<nFields; d2++){
      m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
      m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
      m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
      m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
      m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
      m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
    }
  }

  // x=0 y=0 (linea)
  for (int k=1; k<nz-1; k++){
    nn = k * (nx*ny) + 0*nx + 0;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
      }
    }
  }

  // x=lx y=0 (linea)
  for (int k=1; k<nz-1; k++){
    nn = k * (nx*ny) + 0*nx + nx - 1;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // x=lx y=ly (linea)
  for (int k=1; k<nz-1; k++){
    nn = k * (nx*ny) + (ny-1)*nx + nx - 1;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // x=0 y=ly (linea)
  for (int k=1; k<nz-1; k++){
    nn = k * (nx*ny) + (ny-1)*nx + 0;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // y=0 z=0 (linea)
  for (int i=1; i<nx-1; i++){
    nn = 0*(nx*ny) + 0*nx + i;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
      }
    }
  }

  // y=ly z=0 (linea)
  for (int i=1; i<nx-1; i++){
    nn = 0*(nx*ny) + (ny-1)*nx + i;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // y=ly z=lz (linea)
  for (int i=1; i<nx-1; i++){
    nn = (nz-1)*(nx*ny) + (ny-1)*nx + i;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // y=0 z=lz (linea)
  for (int i=1; i<nx-1; i++){
    nn = (nz-1)*(nx*ny) + (ny-1)*nx + i;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // x=0 z=0 (linea)
  for (int j=1; j<ny-1; j++){
    nn = 0*(nx*ny) + j*nx + 0;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
      }
    }
  }

  // x=lx z=0 (linea)
  for (int j=1; j<ny-1; j++){
    nn = 0*(nx*ny) + j*nx + nx - 1;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // x=lx z=lz (linea)
  for (int j=1; j<ny-1; j++){
    nn = (nz-1)*(nx*ny) + j*nx + nx - 1;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // x=0 z=lz (linea)
  for (int j=1; j<ny-1; j++){
    nn = (nz-1)*(nx*ny) + j*nx + 0;
    for (int d1=0; d1<nFields; d1++){
      for (int d2=0; d2<nFields; d2++){
	m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
      }
    }
  }

  // z=0 
  for (int i=1; i<nx-1; i++){
    for (int j=1; j<ny-1; j++){
      nn = 0*(nx*ny) + j*nx + i;
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	  m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
	}
      }
    }
  }

  // z=lz 
  for (int i=1; i<nx-1; i++){
    for (int j=1; j<ny-1; j++){
      nn = (nz-1)*(nx*ny) + j*nx + i;
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	  m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
  }

  // y=0 
  for (int i=1; i<nx-1; i++){
    for (int k=1; k<nz-1; k++){
      nn = k*(nx*ny) + 0*nx + i;
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	  m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
	}
      }
    }
  }

  // y=ly 
  for (int i=1; i<nx-1; i++){
    for (int k=1; k<nz-1; k++){
      nn = k*(nx*ny) + (ny-1)*nx + i;
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	  m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
  }

  // x=0
  for (int j=1; j<ny-1; j++){
    for (int k=1; k<nz-1; k++){
      nn = k*(nx*ny) + j*nx + 0;
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	  m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
	}
      }
    }
  }

  // x=lx
  for (int j=1; j<ny-1; j++){
    for (int k=1; k<nz-1; k++){
      nn = k*(nx*ny) + j*nx + nx - 1;
      for (int d1=0; d1<nFields; d1++){
	for (int d2=0; d2<nFields; d2++){
	  m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	  m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	  m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = 0;
	  m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	  m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	  m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = 0;
	}
      }
    }
  }

  // Internal nodes
  for (int i=1; i<nx-1; i++){
    for (int j=1; j<ny-1; j++){
      for (int k=1; k<nz-1; k++){
	nn = k * (nx*ny) + j*nx + i;
	for (int d1=0; d1<nFields; d1++){
	  for (int d2=0; d2<nFields; d2++){
	    m.cols[nn*nFields*nnz + 0*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 1*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx    )*nFields + d2;
	    m.cols[nn*nFields*nnz + 2*nFields  + nnz*d1 + d2] = (nn - (nx*ny) - nx + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 3*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 4*nFields  + nnz*d1 + d2] = (nn - (nx*ny)         )*nFields + d2;
	    m.cols[nn*nFields*nnz + 5*nFields  + nnz*d1 + d2] = (nn - (nx*ny)      + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 6*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 7*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx    )*nFields + d2;
	    m.cols[nn*nFields*nnz + 8*nFields  + nnz*d1 + d2] = (nn - (nx*ny) + nx + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 9*nFields  + nnz*d1 + d2] = (nn           - nx - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 10*nFields + nnz*d1 + d2] = (nn           - nx    )*nFields + d2;
	    m.cols[nn*nFields*nnz + 11*nFields + nnz*d1 + d2] = (nn           - nx + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 12*nFields + nnz*d1 + d2] = (nn                - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 13*nFields + nnz*d1 + d2] = (nn                   )*nFields + d2;
	    m.cols[nn*nFields*nnz + 14*nFields + nnz*d1 + d2] = (nn                + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 15*nFields + nnz*d1 + d2] = (nn           + nx - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 16*nFields + nnz*d1 + d2] = (nn           + nx    )*nFields + d2;
	    m.cols[nn*nFields*nnz + 17*nFields + nnz*d1 + d2] = (nn           + nx + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 18*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 19*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx    )*nFields + d2;
	    m.cols[nn*nFields*nnz + 20*nFields + nnz*d1 + d2] = (nn + (nx*ny) - nx + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 21*nFields + nnz*d1 + d2] = (nn + (nx*ny)      - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 22*nFields + nnz*d1 + d2] = (nn + (nx*ny)         )*nFields + d2;
	    m.cols[nn*nFields*nnz + 23*nFields + nnz*d1 + d2] = (nn + (nx*ny)      + 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 24*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx - 1)*nFields + d2;
	    m.cols[nn*nFields*nnz + 25*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx    )*nFields + d2;
	    m.cols[nn*nFields*nnz + 26*nFields + nnz*d1 + d2] = (nn + (nx*ny) + nx + 1)*nFields + d2;
	  }
	}
      }
    }
  }

}

int ell_set_val (ell_matrix * m, int row, int col, double val)
{
  if (row >= m->nrow) { 
    cout << "ell error: row " << row << " is greater than the dimension of the matrix" << endl;
    return 1;
  } else if(col >= m->ncol) {
    cout << "ell error: col " << col << " is greater than the dimension of the matrix" << endl;
    return 2;
  } else if (row < 0) {
    cout << "ell error: negative values in row " << row << endl;
    return 3;
  } else if (col < 0) {
    cout << "ell error: negative values in col " << col << endl;
    return 4;
  }
  int j = 0;
  while (j < m->nnz) {
    if (m->cols[(row*m->nnz) + j] == -1) {
      m->cols[(row*m->nnz) + j] = col;
      m->vals[(row*m->nnz) + j] = val;
      return 0;
    } else if (m->cols[(row*m->nnz) + j] == col) {
      m->vals[(row*m->nnz) + j] = val;
      return 0;
    }
    j++;
  }
  if (j == m->nnz) {
    cout << "ell error: not enought space to store value in row " << row << "and col " << col << endl;
    return 5;
  }
  return 6;
}

int ell_add_val (ell_matrix * m, int row, int col, double val)
{
  if (row >= m->nrow || col >= m->ncol) {
    cout << "ell error: row " << row << " or col " << col << " greater than the dimension of the matrix" << endl;
    return 1;
  }
  if (row < 0 || col < 0) {
    cout << "ell error: negative values in row or col" << endl;
    return 2;
  }
  int j = 0, row_start = row*m->nnz;
  while (j < m->nnz) {
    if (m->cols[row_start + j] == -1) {
      m->cols[row_start + j] = col;
      m->vals[row_start + j] = val;
      return 0;
    } else if (m->cols[row_start + j] == col) {
      m->vals[row_start + j] += val;
      return 0;
    }
    j++;
  }
  if (j == m->nnz) {
    cout << "ell error: not enought space to add value in row and col" << endl;
    return 3;
  }
  return 4;
}

int ell_add_vals (ell_matrix *m, int *ix, int nx, int *iy, int ny, double *vals)
{
  if (m == NULL || ix == NULL || iy == NULL || vals == NULL) return 1;
  for (int i = 0 ; i < nx ; i++) {
    for (int j = 0 ; j < ny ; j++) {
      ell_add_val (m, ix[i], iy[j], vals[i*nx + j]);
    }
  }
  return 0;
}

int ell_set_zero_row (ell_matrix *m, int row, double diag_val)
{
  if (m == NULL) return 1;
  int j = 0;
  while (j < m->nnz) {
    if (m->cols[(row*m->nnz) + j] == -1) {
      return 0;
    } else if (m->cols[(row*m->nnz) + j] == row) {
      m->vals[(row*m->nnz) + j] = diag_val;
    } else {
      m->vals[(row*m->nnz) + j] = 0.0;
    }
    j++;
  }
  return 0;
}

int ell_set_zero_col (ell_matrix *m, int col, double diag_val)
{
  if (m == NULL) return 1;
  for (int i = 0 ; i < m->nrow ; i++) {
    int j = 0;
    while (j < m->nnz) {
      if (m->cols[(i*m->nnz) + j] == -1) {
	break;
      } else if (m->cols[(i*m->nnz) + j] == col) {
	m->vals[(i*m->nnz) + j] = (i == col) ? diag_val : 0.0;
      }
      j++;
    }
  }
  return 0;
}

int ell_mvp (ell_matrix * m, double *x, double *y)
{
  //  y = m * x
  if (m == NULL || x == NULL || y == NULL) return 1;

  for (int i = 0 ; i < m->nrow ; i++) {
    y[i] = 0;
    int j = 0;
    while (j < m->nnz) {
      if (m->cols[(i*m->nnz) + j] == -1) break;
      y[i] += m->vals[(i*m->nnz) + j] * x[m->cols[(i*m->nnz) + j]];
      j++;
    }
  }
  return 0;
}

int ell_solve_jacobi (ell_solver *solver, ell_matrix * m, double *b, double *x)
{
  /* A = K - N  
   * K = diag(A)
   * N_ij = -a_ij for i!=j  and =0 if i=j
   * x_(i) = K^-1 * ( N * x_(i-1) + b )
   */
  if (m == NULL || b == NULL || x == NULL) return 1;
  
  double *k = (double*)malloc (m->nrow * sizeof(double)); // K = diag(A)
  double *e_i = (double*)malloc(m->nrow * sizeof(double));

  for (int i=0; i<m->nrow; i++) {
    ell_get_val (m, i, i, &k[i]);
    k[i] = 1 / k[i];
  }

  int its = 0;
  int max_its = solver->max_its;
  double err;
  double min_tol = solver->min_tol;

  while (its < max_its) {

    err = 0;
    int i = 0;
    while (i < m->nrow) {
      double aux = 0.0; // sum_(j!=i) a_ij * x_j
      int j = 0;
      while (j < m->nnz) {
        if (m->cols[i*m->nnz + j] == -1) break;
        if (m->cols[i*m->nnz + j] != i)
	  aux += m->vals[i*m->nnz + j] * x[m->cols[i*m->nnz + j]];
	j++;
      }
      x[i] = k[i] * (-1*aux + b[i]);
      i++;
    }

    err = 0;
    ell_mvp (m, x, e_i);
    for (int i = 0 ; i < m->nrow ; i++){
      e_i[i] -= b[i];
      err += e_i[i] * e_i[i];
    }
    err = sqrt(err); if (err < min_tol) break;
    its ++;
  }
  solver->err = err;
  solver->its = its;
  return 0;
}

int ell_solve_cg (ell_solver *solver, ell_matrix * m, double *b, double *x)
{
  /* cg with jacobi preconditioner
   * r_1 residue in actual iteration
   * z_1 = K^-1 * r_0 actual auxiliar vector
   * rho_0 rho_1 = r_0^t * z_1 previous and actual iner products <r_i, K^-1, r_i>
   * p_1 actual search direction
   * q_1 = A*p_1 auxiliar vector
   * d_1 = rho_0 / (p_1^t * q_1) actual step
   * x_1 = x_0 - d_1 * p_1
   * r_1 = r_0 - d_1 * q_1
  */
  if (m == NULL || b == NULL || x == NULL) return 1;
  
  int its = 0;
  double *k = (double*)malloc(m->nrow * sizeof(double)); // K = diag(A)
  double *r = (double*)malloc(m->nrow * sizeof(double));
  double *z = (double*)malloc(m->nrow * sizeof(double));
  double *p = (double*)malloc(m->nrow * sizeof(double));
  double *q = (double*)malloc(m->nrow * sizeof(double));
  double rho_0, rho_1, d;
  double err;

  for (int i = 0 ; i < m->nrow ; i++) {
    ell_get_val (m, i, i, &k[i]);
    k[i] = 1 / k[i];
  }

  ell_mvp (m, x, r);
  for (int i = 0 ; i < m->nrow ; i++)
    r[i] -= b[i];

  do {

    err = 0;
    for (int i = 0 ; i < m->nrow ; i++)
      err += r[i] * r[i];
    err = sqrt(err); if (err < solver->min_tol) break;

    for (int i = 0 ; i < m->nrow ; i++)
      z[i] = k[i] * r[i];

    rho_1 = 0.0;
    for (int i = 0 ; i < m->nrow ; i++)
      rho_1 += r[i] * z[i];

    if (its == 0) {
      for (int i = 0 ; i < m->nrow ; i++)
	p[i] = z[i];
    } else {
      double beta = rho_1 / rho_0;
      for (int i = 0 ; i < m->nrow ; i++)
	p[i] = z[i] + beta * p[i];
    }

    ell_mvp (m, p, q);
    double aux = 0;
    for (int i = 0 ; i < m->nrow ; i++)
      aux += p[i] * q[i];
    d = rho_1 / aux;

    for (int i = 0 ; i < m->nrow ; i++) {
      x[i] -= d * p[i];
      r[i] -= d * q[i];
    }

    rho_0 = rho_1;
    its ++;

  } while (its < solver->max_its);

  solver->err = err;
  solver->its = its;
  return 0;
}

int ell_get_val (ell_matrix * m, int row, int col, double *val)
{
  if (row >= m->nrow || col >= m->ncol) {
    cout << "ell_get_val: row or col greater than the dimension of the matrix" << endl;
    return 1;
  }
  if (row < 0 || col < 0) {
    cout << "ell_get_val: negative values in row or col" << endl;
    return 2;
  }
  int j = 0;
  while (j < m->nnz) {
    if (m->cols[(row * m->nnz) + j] == -1) {
      *val = 0.0;
      return 0;
    } else if (m->cols[(row * m->nnz) + j] == col) {
      *val = m->vals[(row * m->nnz) + j];
      return 0;
    }
    j++;
  }
  return 3;
}
