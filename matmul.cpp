#include <hpx/config.hpp>
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/include/util.hpp>

#include "sched_futures.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

std::mt19937 mt{};

// (mxn) * (nxk)
// matrix can be sub matrix
// la is width of actual matrix a
// ptr + la = ptr to cell vertically below it
void matmul(int m, int n, int k, double *a, int la, double *b, int lb,
            double *c, int lc) {
  int ii, jj, kk;
  for (ii = 0; ii < m; ii++) {
    for (jj = 0; jj < n; jj++) {
      double acc = 0.0;
      for (kk = 0; kk < k; kk++) {
        acc += a[ii + la * kk] * b[kk + lb * jj];
      }
      c[ii + lc * jj] = acc;
    }
  }
}

// generates matrix of (m*32)x(n*32)
double *generate_matrix(int m, int n) {
  double *matrix;
  int num_rows = m * 32;
  int num_cols = n * 32;

  matrix = (double *)malloc(sizeof(double) * num_rows * num_cols);

  for (int i = 0; i < num_rows * num_cols; i++) {
    matrix[i] = mt();
  }

  return matrix;
}

void multiply_matrix(double *A, double *B, double *C, int m, int n, int k) {
  // number of mutexes = number of tiles in resultant matrix
  Scheduler sched;

  for (int ii = 0; ii < m; ii++)
    for (int kk = 0; kk < k; kk++) {
      ResourceRef rr = sched.add_resource();
      for (int jj = 0; jj < n; jj++) {
        TaskRef tr =
            sched.add_task([=]() { matmul(32, 32, 32, A, m, B, n, C, m); });
        sched.add_required_resource(tr, rr);
      }
    }

  auto f = sched.run();
  f.wait();
}

int hpx_main(hpx::program_options::variables_map &vm) {
  double *A; // m tiles x n tiles
  double *B; // n tiles x k tiles
  double *C; // m tiles x k tiles

  int m = vm["m"].as<int>();
  int n = vm["n"].as<int>();
  int k = vm["k"].as<int>();

  A = (double *)malloc(sizeof(double) * m * 32 * n * 32); // m tiles X n tiles
  B = (double *)malloc(sizeof(double) * n * 32 * k * 32); // n tiles X k tiles
  C = (double *)malloc(sizeof(double) * m * 32 * k * 32); // m tiles X k tiles

  if (!(A || B || C)) {
    std::cerr << "COULD NOT MALLOC" << std::endl;
    exit(1);
  }

  for (int i = 0; i < 20; i++) {

    std::for_each(A, A + m * 32 * n * 32, [](double &i) { i = rand(); });
    std::for_each(B, B + n * 32 * k * 32, [](double &i) { i = rand(); });
    std::for_each(C, C + m * 32 * k * 32, [](double &i) { i = rand(); });

    auto start = std::chrono::high_resolution_clock::now();
    multiply_matrix(A, B, C, m, n, k);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration = end - start;
    std::cout << m << "," << duration.count() << std::endl;
  }

  return hpx::finalize();
}

int main(int argc, char *argv[]) {
  using namespace hpx::program_options;

  options_description desc_commandline;
  // clang-format off
  desc_commandline.add_options()("results",
                                 "print generated results (default: false)")
      ("m", value<int>()->default_value(10), "m")
      ("n", value<int>()->default_value(10), "n")
      ("k", value<int>()->default_value(10), "k");
  // clang-format on

  hpx::init_params init_args;
  init_args.desc_cmdline = desc_commandline;

  return hpx::init(argc, argv, init_args);
}
