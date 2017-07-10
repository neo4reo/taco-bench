
#include "taco.h"

using namespace taco;
using namespace std;

// MACRO to benchmark some CODE with REPEAT times and COLD/WARM cache
#define TACO_BENCH(CODE, NAME, REPEAT, TIMER, COLD) {           \
    TACO_TIME_REPEAT(CODE, REPEAT, TIMER, COLD);                \
    cout << NAME << " time (ms)" << endl << TIMER << endl;      \
}

// Enum of possible expressions to Benchmark
enum BenchExpr {SpMV,plus3};

// Compare two tensors
bool compare(const Tensor<double>&Dst, const Tensor<double>&Ref) {
  if (Dst.getDimensions() != Ref.getDimensions()) {
    return false;
  }

  {
    std::set<std::vector<int>> coords;
    for (const auto& val : Dst) {
      if (!coords.insert(val.first).second) {
        return false;
      }
    }
  }

  vector<std::pair<std::vector<int>,double>> vals;
  for (const auto& val : Dst) {
    if (val.second != 0) {
      vals.push_back(val);
    }
  }

  vector<std::pair<std::vector<int>,double>> expected;
  for (const auto& val : Ref) {
    if (val.second != 0) {
      expected.push_back(val);
    }
  }
  std::sort(expected.begin(), expected.end());
  std::sort(vals.begin(), vals.end());
  return vals == expected;
}

void validate (string name, const Tensor<double>& Dst, const Tensor<double>& Ref) {
  if (Dst.getFormat()==Ref.getFormat()) {
    if (!equals (Dst, Ref)) {
      cout << "\033[1;31m  Validation Error with " << name << " \033[0m" << endl;
    }
  }
  else {
    if (!compare(Dst,Ref)) {
      cout << "\033[1;31m  Validation Error with " << name << " \033[0m" << endl;
    }
  }
}