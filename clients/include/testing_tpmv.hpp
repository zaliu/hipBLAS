/* ************************************************************************
 * Copyright 2016-2020 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_tpmv(const Arguments& argus)
{
    bool FORTRAN       = argus.fortran;
    auto hipblasTpmvFn = FORTRAN ? hipblasTpmv<T, true> : hipblasTpmv<T, false>;

    int M    = argus.M;
    int incx = argus.incx;

    int A_size = M * (M + 1) / 2;

    hipblasFillMode_t  uplo   = char2hipblas_fill(argus.uplo_option);
    hipblasOperation_t transA = char2hipblas_operation(argus.transA_option);
    hipblasDiagType_t  diag   = char2hipblas_diagonal(argus.diag_option);
    hipblasStatus_t    status = HIPBLAS_STATUS_SUCCESS;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(M < 0 || incx == 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(A_size);
    host_vector<T> hx(M * incx);
    host_vector<T> hres(M * incx);

    device_vector<T> dA(A_size);
    device_vector<T> dx(M * incx);

    double gpu_time_used, cpu_time_used;
    double hipblasGflops, cblas_gflops, hipblasBandwidth;
    double rocblas_error;

    hipblasHandle_t handle;
    hipblasCreate(&handle);

    // Initial Data on CPU
    srand(1);
    hipblas_init<T>(hA, 1, A_size, 1);
    hipblas_init<T>(hx, 1, M, incx);

    // copy vector is easy in STL; hz = hy: save a copy in hz which will be output of CPU BLAS
    hres = hx;

    // copy data from CPU to device
    hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice);
    hipMemcpy(dx, hx.data(), sizeof(T) * M * incx, hipMemcpyHostToDevice);

    /* =====================================================================
           ROCBLAS
    =================================================================== */
    for(int iter = 0; iter < 1; iter++)
    {

        status = hipblasTpmvFn(handle, uplo, transA, diag, M, dA, dx, incx);

        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            hipblasDestroy(handle);
            return status;
        }
    }

    // copy output from device to CPU
    hipMemcpy(hres.data(), dx, sizeof(T) * M * incx, hipMemcpyDeviceToHost);

    if(argus.unit_check)
    {
        /* =====================================================================
           CPU BLAS
        =================================================================== */

        cblas_tpmv<T>(uplo, transA, diag, M, hA.data(), hx.data(), incx);

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(argus.unit_check)
        {
            unit_check_general<T>(1, M, incx, hx, hres);
        }
    }

    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
