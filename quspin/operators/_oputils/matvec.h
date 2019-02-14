#ifndef __MATVEC_H__
#define __MATVEC_H__


#include <algorithm>
#include <iostream>

// y += a*x
template <typename I, typename T>
void axpy(const I n, const T a, const T * x, T * y){
	for(I i = 0; i < n; i++){
		y[i] += a * x[i];
	}
}

#if defined(_OPENMP)
#include "csrmv_merge.h"

template<typename I, typename T1,typename T2>
void inline csr_matvec(const bool overwrite_y,
						const I n,
						const I Ap[],
						const I Aj[],
						const T1 Ax[],
						const T1 a,
						const T2 x[],
							  I rco[],
							  T2 vco[],
							  T2 y[])
{
	csrmv_merge(overwrite_y,n,Ap,Aj,Ax,a,x,rco,vco,y);
}

template <typename I, typename T1, typename T2>
void dia_matvec(const bool overwrite_y,
				const I n_row,
				const I n_col,
				const I n_diags,
				const I L,
				const I offsets[], 
				const T1 diags[], 
				const T1 a,
				const T2 x[],
					  T2 y[])
{

	const int nthread = omp_get_num_threads();

	if(overwrite_y){
		const I chunk = n_row/nthread;
		#pragma omp for schedule(static,chunk)
		for(I n=0;n<n_row;n++){
			y[n] = 0; 
		}
	}

	for(I i = 0; i < n_diags; i++){
		const I k = offsets[i];  //diagonal offset

		const I i_start = std::max<I>(0,-k);
		const I j_start = std::max<I>(0, k);
		const I j_end   = std::min<I>(std::min<I>(n_row + k, n_col),L);

		const I N = j_end - j_start;  //number of elements to process
        const I chunk = N/nthread;
		const T1 * diag = diags + i*L + j_start;
		const T2 * x_row = x + j_start;
			  T2 * y_row = y + i_start;

		#pragma omp for schedule(static,chunk)
		for(I n=0;n<N;n++){
			y_row[n] += (T2)(a * diag[n]) * x_row[n]; 
		}
	}
}


#include <complex>

template<class T>
void inline atomic_add(T &y,const T &aa){
	#pragma omp atomic
	y += aa;
}

template<class T>
void inline atomic_add(std::complex<T> &y,const std::complex<T> &aa){
	T * y_v = reinterpret_cast<T*>(&y);
	const T * aa_v = reinterpret_cast<const T*>(&aa);

	#pragma omp atomic
	y_v[0] += aa_v[0];
	#pragma omp atomic
	y_v[1] += aa_v[1];	
}

template<typename I, typename T1,typename T2>
void csc_matvec(const bool overwrite_y,
						const I n_row,
						const I n_col,
						const I Ap[],
						const I Ai[],
						const T1 Ax[],
						const T1 a,
						const T2 x[],
							  T2 y[])
{
	const int nthread = omp_get_num_threads();
	const I chunk = std::max((I)1,n_row/(100*nthread));
	if(overwrite_y){
		#pragma omp for schedule(static,chunk)
		for(I j = 0; j < n_row; j++){
			y[j] = 0;
		}
	}
	
	#pragma omp for schedule(dynamic,chunk)
	for(I j = 0; j < n_col; j++){
		I col_start = Ap[j];
		I col_end   = Ap[j+1];

		for(I ii = col_start; ii < col_end; ii++){
			const I i = Ai[ii];
			const T2 aa = (T2)(a * Ax[ii]) * x[j];
			atomic_add(y[i],aa);
		}
	}
}



#else

template<typename I, typename T1,typename T2>
void csr_matvec(const bool overwrite_y,
				const I n,
				const I Ap[],
				const I Aj[],
				const T1 Ax[],
				const T1 a,
				const T2 x[],
					  I rco[],
					  T2 vco[],
					  T2 y[])
{
	const T2 a_cast = a;
	if(overwrite_y){
		for(I k = 0; k<n; k++){
			T2 sum = 0;
			for(I jj = Ap[k]; jj < Ap[k+1]; jj++){
				sum += (T2)Ax[jj] * x[Aj[jj]];
			}
			y[k] = a_cast * sum;
		}
	}else{
		for(I k = 0; k<n; k++){
			T2 sum = 0;
			for(I jj = Ap[k]; jj < Ap[k+1]; jj++){
				sum += (T2)Ax[jj] * x[Aj[jj]];
			}
			y[k] += a_cast * sum;
		}
	}
}

template <typename I, typename T1, typename T2>
void dia_matvec(const bool overwrite_y,
				const I n_row,
                const I n_col,
                const I n_diags,
                const I L,
	            const I offsets[], 
	            const T1 diags[], 
	            const T1 a,
	            const T2 x[],
	                  T2 y[])
{

	if(overwrite_y){
		for(I i = 0; i < n_row; i++){
			y[i] = 0;
		}
	}

    for(I i = 0; i < n_diags; i++){
        const I k = offsets[i];  //diagonal offset

        const I i_start = std::max<I>(0,-k);
        const I j_start = std::max<I>(0, k);
        const I j_end   = std::min<I>(std::min<I>(n_row + k, n_col),L);

        const I N = j_end - j_start;  //number of elements to process

        const T1 * diag = diags + (npy_intp)i*L + j_start;
        const T2 * x_row = x + j_start;
              T2 * y_row = y + i_start;

        for(I n = 0; n < N; n++){
            y_row[n] += (T2)(a * diag[n]) * x_row[n]; 
        }
    }
}


template<typename I, typename T1,typename T2>
void csc_matvec(const bool overwrite_y,
						const I n_row,
						const I n_col,
						const I Ap[],
						const I Ai[],
						const T1 Ax[],
						const T1 a,
						const T2 x[],
							  T2 y[])
{
	if(overwrite_y){
		for(I j = 0; j < n_row; j++){
			y[j] = 0;
		}
	}

	

	for(I j = 0; j < n_col; j++){
		I col_start = Ap[j];
		I col_end   = Ap[j+1];

		for(I ii = col_start; ii < col_end; ii++){
			const I i = Ai[ii];
			y[i] += (T2)(a * Ax[ii]) * x[j];
		}
	}
}



#endif



template <typename I, typename T1, typename T2>
void dia_matvecs(const bool overwrite_y,
				const I n_row,
                const I n_col,
                const I n_vecs,
                const I n_diags,
                const I L,
	            const I offsets[], 
	            const T1 diags[], 
	            const T1 a,
	            const T2 x[],
	                  T2 y[])
{
	if(overwrite_y){
		const npy_intp n = (npy_intp)n_row * n_vecs;

		for(npy_intp i = 0; i < n; i++){
			y[i] = 0;
		}
	}

    for(I i = 0; i < n_diags; i++){
        const I k = offsets[i];  //diagonal offset

        const I i_start = std::max<I>(0,-k);
        const I j_start = std::max<I>(0, k);
        const I j_end   = std::min<I>(std::min<I>(n_row + k, n_col),L);

        const I N = j_end - j_start;  //number of elements to process

        const T1 * diag = diags + (npy_intp)i*L + j_start;
        const T2 * x_row = x + (npy_intp)j_start * n_vecs;
              T2 * y_row = y + (npy_intp)i_start * n_vecs;

        for(I n = 0; n < N; n++){
            axpy(n_vecs,(T2)(a * diag[n]), x_row + (npy_intp)n_vecs * n, y_row + (npy_intp)n_vecs * n);
        }
    }
}



template<typename I, typename T1,typename T2>
void csr_matvecs(const bool overwrite_y,
				const I n,
				const I n_vecs,
				const I Ap[],
				const I Aj[],
				const T1 Ax[],
				const T1 a,
				const T2 x[],
					  T2 y[])
{
	
	if(overwrite_y){
		for(I k = 0; k<n; k++){
			T2 * y_row = y + (npy_intp)n_vecs * k;
			
			for(I jj=0;jj<n_vecs;jj++){y_row[jj] = 0;}

			for(I jj = Ap[k]; jj < Ap[k+1]; jj++){
				const I j = Aj[jj];
				const T2 ax = a * Ax[jj];
				const T2 * x_row = x + (npy_intp)n_vecs * j;
				axpy(n_vecs, ax, x_row, y_row);

			}
		}
	}else{
		for(I k = 0; k<n; k++){
			T2 * y_row = y + (npy_intp)n_vecs * k;

			for(I jj = Ap[k]; jj < Ap[k+1]; jj++){
				const I j = Aj[jj];
				const T2 ax = a * Ax[jj];
				const T2 * x_row = x + (npy_intp)n_vecs * j;
				axpy(n_vecs, ax, x_row, y_row);

			}
		}
	}

}









template<typename I, typename T1,typename T2>
void csc_matvecs(const bool overwrite_y,
						const I n_row,
						const I n_col,
						const I n_vecs,
						const I Ap[],
						const I Ai[],
						const T1 Ax[],
						const T1 a,
						const T2 x[],
							  T2 y[])
{
	if(overwrite_y){
		for(I i = 0; i < n_row*n_vecs; i++){
			y[i] = 0;
		}
	}

	

	for(I j = 0; j < n_col; j++){
		I col_start = Ap[j];
		I col_end   = Ap[j+1];

		for(I ii = col_start; ii < col_end; ii++){
			I i	= Ai[ii];
			const T2 ax = a * Ax[ii];

			 axpy(n_vecs, ax, x + (npy_intp)n_vecs * j, y + (npy_intp)n_vecs * i);
		}
	}
}




#endif