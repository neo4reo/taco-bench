#include "taco/tensor.h"

using namespace taco;
using namespace std;

#ifdef OSKI
extern "C" {
#include <oski/oski.h>
}

  void exprToOSKI(BenchExpr Expr, map<string,Tensor<double>> exprOperands,int repeat, taco::util::TimeResults timevalue) {
    switch(Expr) {
      case SpMV: {
        int rows=exprOperands.at("A").getDimension(0);
        int cols=exprOperands.at("A").getDimension(1);
        double *a_CSC;
        int* ia_CSC;
        int* ja_CSC;
        getCSCArrays(exprOperands.at("A"),&ia_CSC,&ja_CSC,&a_CSC);
        oski_matrix_t Aoski;
        oski_vecview_t xoski, yoski;
        oski_Init();
        Aoski = oski_CreateMatCSC(ia_CSC,ja_CSC,a_CSC,
                                  rows, cols, SHARE_INPUTMAT, 1, INDEX_ZERO_BASED);
        xoski = oski_CreateVecView((double*)(exprOperands.at("x").getStorage().getValues().getData()),
                                   cols, STRIDE_UNIT);
        Tensor<double> y_oski({rows}, Dense);
        y_oski.pack();
        yoski = oski_CreateVecView((double*)(y_oski.getStorage().getValues().getData()),
                                   rows, STRIDE_UNIT);

        TACO_BENCH( oski_MatMult(Aoski, OP_NORMAL, 1, xoski, 0, yoski);,"OSKI",repeat,timevalue,true );

        validate("OSKI", y_oski, exprOperands.at("yRef"));

        // Tuned version
        oski_SetHintMatMult(Aoski, OP_NORMAL, 1.0, SYMBOLIC_VEC, 0.0, SYMBOLIC_VEC, ALWAYS_TUNE_AGGRESSIVELY);
        oski_TuneMat(Aoski);
        char* xform = oski_GetMatTransforms (Aoski);
        int blockSize=0;
        if (xform) {
          fprintf (stdout, "\tDid tune: '%s'\n", xform);
          std::string oskiTune(xform);
          std::string oskiBegin=oskiTune.substr(oskiTune.find(",")+2);
          std::string oskiXSize=oskiBegin.substr(0,oskiBegin.find(","));
          int XOski=atoi(oskiXSize.c_str());
          if (XOski!=0)
            blockSize = XOski;
          oski_Free (xform);
        }

        TACO_BENCH(oski_MatMult(Aoski, OP_NORMAL, 1, xoski, 0, yoski);,"OSKI Tuned",repeat,timevalue,true);

        validate("OSKI Tuned", y_oski, exprOperands.at("yRef"));

        // commented to avoid some crashes with poski
  //      oski_DestroyMat(Aoski);
  //      oski_DestroyVecView(xoski);
  //      oski_DestroyVecView(yoski);
  //      oski_Close();

        // Taco block version with oski tuned number
        if (blockSize>0) {
          cout << "y(i,ib) = A(i,j,ib,jb)*x(j,jb) -- DSDD " <<endl;

          IndexVar i, j, ib,jb;
          Tensor<double> yb({rows/blockSize,blockSize}, Format({Dense,Dense}));
          Tensor<double> xb({cols/blockSize,blockSize}, Format({Dense,Dense}));
          Tensor<double> Ab({rows/blockSize,cols/blockSize,blockSize,blockSize},
                            Format({Dense,Sparse,Dense,Dense}));

          int i_b=0;
          for (auto& value : iterate<double>(exprOperands.at("x"))) {
            xb.insert({value.first.at(0)/blockSize,value.first.at(0)%blockSize},
                      value.second);
            i_b++;
          }
          xb.pack();
          for (auto& value : iterate<double>(exprOperands.at("A"))) {
            Ab.insert({value.first.at(0)/blockSize,value.first.at(1)/blockSize,
                       value.first.at(0)%blockSize,value.first.at(1)%blockSize},
                      value.second);
          }
          Ab.pack();

          yb(i,ib) = Ab(i,j,ib,jb) * xb(j,jb);

          TACO_BENCH(yb.compile();, "Compile",1,timevalue,false)
          TACO_BENCH(yb.assemble();,"Assemble",1,timevalue,false)
          TACO_BENCH(yb.compute();, "Compute",repeat, timevalue, true)
        }
        break;
      }
      case MATTRANSMUL:
      case RESIDUAL: {
        int rows=exprOperands.at("A").getDimension(0);
        int cols=exprOperands.at("A").getDimension(1);
        double *a_CSC;
        int* ia_CSC;
        int* ja_CSC;
        getCSCArrays(exprOperands.at("A"),&ia_CSC,&ja_CSC,&a_CSC);
        oski_matrix_t Aoski;
        oski_vecview_t xoski, yoski, zoski;
        oski_Init();
        Aoski = oski_CreateMatCSC(ia_CSC,ja_CSC,a_CSC,
                                  rows, cols, SHARE_INPUTMAT, 1, INDEX_ZERO_BASED);
        xoski = oski_CreateVecView((double*)(exprOperands.at("x").getStorage().getValues().getData()),
                                   cols, STRIDE_UNIT);
        zoski = oski_CreateVecView((double*)(exprOperands.at("z").getStorage().getValues().getData()),
                                   rows, STRIDE_UNIT);
        Tensor<double> y_oski({rows}, Dense);
        y_oski.pack();
        yoski = oski_CreateVecView((double*)(y_oski.getStorage().getValues().getData()),
                                   rows, STRIDE_UNIT);
        double alpha = ((double*)(exprOperands.at("alpha").getStorage().getValues().getData()))[0];
        double beta = ((double*)(exprOperands.at("beta").getStorage().getValues().getData()))[0];

        double* yvals=((double*)(y_oski.getStorage().getValues().getData()));
        double* zvals=((double*)(exprOperands.at("z").getStorage().getValues().getData()));

        if (Expr==MATTRANSMUL) {
          TACO_BENCH(for (auto k=0; k<rows; k++) {yvals[k]=zvals[k];} ;
                     oski_MatMult(Aoski, OP_TRANS, alpha, xoski, beta, yoski);,"OSKI",repeat,timevalue,true); }
        else {
          TACO_BENCH(for (auto k=0; k<rows; k++) {yvals[k]=zvals[k];} ;
                     oski_MatMult(Aoski, OP_NORMAL, -1.0, xoski, 1.0, yoski);,"OSKI",repeat,timevalue,true); }

        validate("OSKI", y_oski, exprOperands.at("yRef"));

        // Tuned version
        if (Expr==MATTRANSMUL) {
          oski_SetHintMatMult(Aoski, OP_TRANS, alpha, SYMBOLIC_VEC, beta, SYMBOLIC_VEC, ALWAYS_TUNE_AGGRESSIVELY); }
        else {
          oski_SetHintMatMult(Aoski, OP_NORMAL, -1.0, SYMBOLIC_VEC, 1.0, SYMBOLIC_VEC, ALWAYS_TUNE_AGGRESSIVELY); }
        oski_TuneMat(Aoski);

        if (Expr==MATTRANSMUL) {
          TACO_BENCH(for (auto k=0; k<rows; k++) {yvals[k]=zvals[k];} ;
                     oski_MatMult(Aoski, OP_TRANS, alpha, xoski, beta, yoski);,"OSKI Tuned",repeat,timevalue,true); }
        else {
          TACO_BENCH(for (auto k=0; k<rows; k++) {yvals[k]=zvals[k];} ;
                     oski_MatMult(Aoski, OP_NORMAL, -1.0, xoski, 1.0, yoski);,"OSKI Tuned",repeat,timevalue,true); }

        validate("OSKI Tuned", y_oski, exprOperands.at("yRef"));

        break;
      }
      default:
        cout << " !! Expression not implemented for OSKI" << endl;
        break;
    }
  }

#endif
