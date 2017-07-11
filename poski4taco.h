#include "taco/tensor.h"

using namespace taco;
using namespace std;

#ifdef POSKI
extern "C" {
#include <poski/poski.h>
}

  void exprToPOSKI(BenchExpr Expr, map<string,Tensor<double>> exprOperands,int repeat, taco::util::TimeResults timevalue) {
    switch(Expr) {
      case SpMV: {
        int rows=exprOperands.at("A").getDimension(0);
        int cols=exprOperands.at("A").getDimension(1);
        // convert to CSR
        Tensor<double> ACSR({rows,cols}, CSR);
        for (auto& value : iterate<double>(exprOperands.at("A"))) {
          ACSR.insert({value.first.at(0),value.first.at(1)},value.second);
        }
        ACSR.pack();
        double *a_CSR;
        int* ia_CSR;
        int* ja_CSR;
        getCSRArrays(ACSR,&ia_CSR,&ja_CSR,&a_CSR);

        int extra = 0;
        if (rows%8)
          extra = 8-rows%8;
        Tensor<double> xposki({cols+extra}, Dense);
//        int u=0;
        for (auto& value : iterate<double>(exprOperands.at("x"))) {
          xposki.insert({value.first[0]}, value.second);
        }
        xposki.pack();

        poski_Init();

        // default thread object
        poski_threadarg_t *poski_thread = poski_InitThreads();
        poski_ThreadHints(poski_thread, NULL, POSKI_OPENMP, 12);
        poski_partitionarg_t *mat_partition = NULL;

        // create CSR matrix
        poski_mat_t A_tunable = poski_CreateMatCSR(ia_CSR, ja_CSR, a_CSR,
             rows, cols, ACSR.getStorage().getValues().getSize(),
             COPY_INPUTMAT,  // greatest flexibility in tuning
             poski_thread, mat_partition, 2, INDEX_ZERO_BASED, MAT_GENERAL);

        Tensor<double> y_poski({rows}, Dense);
        y_poski.pack();

        poski_vec_t xposki_view = poski_CreateVec((double*)(xposki.getStorage().getValues().getData()), cols, STRIDE_UNIT, NULL);
        poski_vec_t yposki_view = poski_CreateVec((double*)(y_poski.getStorage().getValues().getData()), rows, STRIDE_UNIT, NULL);

        TACO_BENCH(poski_MatMult(A_tunable, OP_NORMAL, 1, xposki_view, 0, yposki_view);,"POSKI",repeat,timevalue,true)

        validate("POSKI", y_poski, exprOperands.at("yRef"));

        // tune
        poski_TuneHint_MatMult(A_tunable, OP_NORMAL, 1, xposki_view, 0, yposki_view, ALWAYS_TUNE_AGGRESSIVELY);
        poski_TuneMat(A_tunable);

        TACO_BENCH(poski_MatMult(A_tunable, OP_NORMAL, 1, xposki_view, 0, yposki_view);,"POSKI Tuned",repeat,timevalue,true);

        validate("POSKI Tuned", y_poski, exprOperands.at("yRef"));

        // deallocate everything -- commented because of some crashes
    //    poski_DestroyMat(A_tunable);
    //    poski_DestroyVec(xoski_view);
    //    poski_DestroyVec(yoski_view);
    //    poski_DestroyThreads(poski_thread);
    //    poski_Close();
        break;
      }
      default:
        cout << " !! Expression not implemented for MKL" << endl;
        break;
    }
  }

#endif
