#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include "taco.h"
#include "taco/util/strings.h"
#include "taco/util/timers.h"
#include "taco/util/collections.h"
#include "taco/util/fill.h"

#include "tacoBench.h"
// Includes for all the products
#include "eigen4taco.h"
#include "ublas4taco.h"
#include "gmm4taco.h"
#include "mkl4taco.h"
#include "poski4taco.h"
#include "oski4taco.h"
#include "yours4taco.h"

using namespace taco;
using namespace std;

static void printFlag(string flag, string text) {
  const size_t descriptionStart = 30;
  const size_t columnEnd        = 80;
  string flagString = "  -" + flag +
                      util::repeat(" ",descriptionStart-(flag.size()+3));
  cout << flagString;
  size_t column = flagString.size();
  vector<string> words = util::split(text, " ");
  for (auto& word : words) {
    if (word=="\n") {
      cout << endl << util::repeat(" ", descriptionStart);
      column = descriptionStart;
    }
    else {
      if (column + word.size()+1 >= columnEnd) {
        cout << endl << util::repeat(" ", descriptionStart);
        column = descriptionStart;
      }
      column += word.size()+1;
      cout << word << " ";
    }
  }
  cout << endl;
}

static void printUsageInfo() {
  cout << "Usage: tacoBench [options]" << endl;
  cout << endl;
  cout << "Examples:" << endl;
  cout << "  tacoBench -E=1 -r=10 -p=eigen,ublas  -i=A:/tmp/consph.mtx" << endl;
  cout << "Options:" << endl;
  printFlag("E=<expressionId>",
            "Specify the expression Id to benchmark from: \n"
            "   1: SpMV         y(i) = A(i,j)*x(j) \n"
            "   2: PLUS3       A(i,j) = B(i,j) + C(i,j) + D(i,j) \n"
            "   3: MATTRANSMUL y = alpha*A^Tx + beta*z \n"
            "   4: RESIDUAL    y(i) = b(i) - A(i,j)*x(j) \n"
            "   5: SDDMM       A = B o (CxD) ");
  cout << endl;
  printFlag("r=<repeat>",
            "Time compilation, assembly and <repeat> times computation "
            "(defaults to 1).");
  cout << endl;
  printFlag("i=<tensor>:<filename>",
            "Read a tensor from a .mtx file.");
  cout << endl;
  printFlag("p=<product>,<products>",
            "Specify a list of products to use from: \n "
            "eigen, gmm, ublas, oski, poski, mkl and eventually yours. \n "
            "(not specified launches all products)");
  cout << endl;
}

static int reportError(string errorMessage, int errorCode) {
  cerr << "Error: " << errorMessage << endl << endl;
  printUsageInfo();
  return errorCode;
}

// Get MatrixSize from a .mtx file
static void readMatrixSize(string filename, int& rows, int& cols)
{
  fstream file;
  string line;
  file.open(filename);
  std::string comment("%");
  int size;

  while (getline(file,line))
  {
    if (line.compare(0, comment.size(), comment)) {
      std::istringstream iss(line);
      if (!(iss >> rows >> cols >> size)) { break; }
      break;
    }
  }
  file.close();
}


int main(int argc, char* argv[]) {

  int Expression=1;
  BenchExpr Expr;
  map<string,Tensor<double>> exprOperands;
  int repeat=1;
  map<string,string> inputFilenames;
  taco::util::TimeResults timevalue;
  map<string,bool> products;
  products.insert({"EIGEN",true});
  products.insert({"GMM",true});
  products.insert({"UBLAS",true});
  products.insert({"OSKI",true});
  products.insert({"POSKI",true});
  products.insert({"MKL",true});
  products.insert({"YOURS",true});

  if (argc < 2)
    return reportError("no arguments", 3);

  // Read Parameters
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    vector<string> argparts = util::split(arg, "=");
    if (argparts.size() > 2) {
      return reportError("Too many '\"' signs in argument", 5);
    }
    string argName = argparts[0];
    string argValue;
    if (argparts.size() == 2)
      argValue = argparts[1];

    if ("-E" == argName) {
      try {
        Expression=stoi(argValue);
        if (Expression==1)
          Expr=SpMV;
        else if(Expression==2)
          Expr=PLUS3;
        else if(Expression==3)
          Expr=MATTRANSMUL;
        else if(Expression==4)
          Expr=RESIDUAL;
        else if(Expression==5)
          Expr=SDDMM;
        else
          return reportError("Incorrect Expression descriptor", 3);
      }
      catch (...) {
        return reportError("Incorrect Expression descriptor", 3);
      }
    }
    else if ("-i" == argName) {
      vector<string> descriptor = util::split(argValue, ":");
      if (descriptor.size() != 2) {
        return reportError("Incorrect -i usage", 3);
      }
      string tensorName = descriptor[0];
      string fileName  = descriptor[1];
      inputFilenames.insert({tensorName,fileName});
    }
    else if ("-p" == argName) {
      vector<string> descriptor = util::split(argValue, ",");
      if (descriptor.size() > products.size()) {
        return reportError("Incorrect -p usage", 3);
      }
      for (auto &product : products ) {
        product.second=false;
      }
      for(int i=0; i<descriptor.size();i++) {
        for (auto & c: descriptor[i]) c = toupper(c);
        products.at(descriptor[i])=true;
      }
    }
    if ("-r" == argName) {
      try {
        repeat=stoi(argValue);
      }
      catch (...) {
        return reportError("Incorrect repeat descriptor", 3);
      }
    }
  }

  // Check products
#ifndef EIGEN
  CHECK_PRODUCT("EIGEN");
#endif
#ifndef UBLAS
  CHECK_PRODUCT("UBLAS");
#endif
#ifndef GMM
  CHECK_PRODUCT("GMM");
#endif
#ifndef MKL
  CHECK_PRODUCT("MKL");
#endif
#ifndef POSKI
  CHECK_PRODUCT("POSKI");
#endif
#ifndef OSKI
  CHECK_PRODUCT("OSKI");
#endif
#ifndef YOURS
  CHECK_PRODUCT("YOURS");
#endif

  // taco Formats
  map<string,Format> TacoFormats;
  TacoFormats.insert({"CSR",CSR});
  TacoFormats.insert({"Sparse,Sparse",Format({Sparse,Sparse})});
  TacoFormats.insert({"CSC",CSC});

  switch(Expr) {
    case SpMV: {
      int rows,cols;
      readMatrixSize(inputFilenames.at("A"),rows,cols);
      Tensor<double> x({cols}, Dense);
      util::fillTensor(x,util::FillMethod::Dense);
      Tensor<double> yRef({rows}, Dense);
      Tensor<double> A=read(inputFilenames.at("A"),CSC,true);
      IndexVar i, j;
      yRef(i) = A(i,j) * x(j);
      yRef.compile();
      yRef.assemble();
      yRef.compute();

      for (auto& formats:TacoFormats) {
        cout << "y(i) = A(i,j)*x(j) -- " << formats.first <<endl;
        A=read(inputFilenames.at("A"),formats.second,true);
        Tensor<double> y({rows}, Dense);

        y(i) = A(i,j) * x(j);

        TACO_BENCH(y.compile();, "Compile",1,timevalue,false)
        TACO_BENCH(y.assemble();,"Assemble",1,timevalue,false)
        TACO_BENCH(y.compute();, "Compute",repeat, timevalue, true)

        validate("taco", y, yRef);
      }
      // get CSC arrays for other products
      A=read(inputFilenames.at("A"),CSC,true);
      exprOperands.insert({"yRef",yRef});
      exprOperands.insert({"A",A});
      exprOperands.insert({"x",x});
      break;
    }
    case PLUS3: {
      int rows,cols;
      readMatrixSize(inputFilenames.at("B"),rows,cols);
      Tensor<double> B=read(inputFilenames.at("B"),CSC,true);
      Tensor<double> C=read(inputFilenames.at("C"),CSC,true);
      Tensor<double> D=read(inputFilenames.at("D"),CSC,true);
      Tensor<double> ARef("ARef",{rows,cols},CSC);
      IndexVar i, j;
      ARef(i,j) = B(i,j) + C(i,j) + D(i,j);
      ARef.compile();
      ARef.assemble();
      ARef.compute();

      map<string,Format> TacoFormats;
      TacoFormats.insert({"CSR",CSR});
      TacoFormats.insert({"Sparse,Sparse",Format({Sparse,Sparse})});
      TacoFormats.insert({"CSC",CSC});

      for (auto& formats:TacoFormats) {
        cout << "A(i,j) = B(i,j) + C(i,j) + D(i,j) -- " << formats.first <<endl;
        B=read(inputFilenames.at("B"),formats.second,true);
        C=read(inputFilenames.at("C"),formats.second,true);
        D=read(inputFilenames.at("D"),formats.second,true);
        Tensor<double> A({rows,cols},formats.second);

        A(i,j) = B(i,j) + C(i,j) + D(i,j);

        TACO_BENCH(A.compile();, "Compile",1,timevalue,false)
        TACO_BENCH(A.assemble();,"Assemble",1,timevalue,false)
        TACO_BENCH(A.compute();, "Compute",repeat, timevalue, true)

        validate("taco", A, ARef);
      }
      // get CSC arrays for other products
      B=read(inputFilenames.at("B"),CSC,true);
      C=read(inputFilenames.at("C"),CSC,true);
      D=read(inputFilenames.at("D"),CSC,true);

      exprOperands.insert({"ARef",ARef});
      exprOperands.insert({"B",B});
      exprOperands.insert({"C",C});
      exprOperands.insert({"D",D});
      break;
    }
    case MATTRANSMUL:
    case RESIDUAL: {
      int rows,cols;
      readMatrixSize(inputFilenames.at("A"),rows,cols);
      Tensor<double> x({cols}, Dense);
      util::fillTensor(x,util::FillMethod::Dense);
      Tensor<double> z({rows}, Dense);
      util::fillTensor(z,util::FillMethod::Dense);
      Tensor<double> Talpha("alpha");
      Tensor<double> Tbeta("beta");
      Tensor<double> yRef({rows}, Dense);
      Tensor<double> A=read(inputFilenames.at("A"),CSC,true);
      IndexVar i, j;
      Talpha.insert({}, 42.0);
      Tbeta.insert({}, 24.0);
      Talpha.pack();
      Tbeta.pack();
      if (Expr==RESIDUAL) {
        ((double*)(Talpha.getStorage().getValues().getData()))[0] = -1.0;
        ((double*)(Tbeta.getStorage().getValues().getData()))[0] = 1.0;
        Tensor<double> A=read(inputFilenames.at("A"),CSR,true);
        yRef(i) = z(i) -(A(i,j) * x(j)) ;
        cout << "y= b - Ax -- " << endl;
      }
      else {
        Tensor<double> A=read(inputFilenames.at("A"),CSC,true);
        yRef(i) = Talpha() * (A(j,i) * x(j)) + Tbeta() * z(i);
        cout << "y=alpha*A^Tx + beta*z -- " << endl;
      }
      TACO_BENCH(yRef.compile();, "Compile",1,timevalue,false)
      TACO_BENCH(yRef.assemble();, "Assemble",1,timevalue,false)
      TACO_BENCH(yRef.compute();, "Compute",repeat,timevalue,true)

      A=read(inputFilenames.at("A"),CSC,true);
      exprOperands.insert({"yRef",yRef});
      exprOperands.insert({"A",A});
      exprOperands.insert({"x",x});
      exprOperands.insert({"z",z});
      exprOperands.insert({"alpha",Talpha});
      exprOperands.insert({"beta",Tbeta});
      break;
    }
    case SDDMM: {
      int rows,cols;
      readMatrixSize(inputFilenames.at("B"),rows,cols);
      Tensor<double> B=read(inputFilenames.at("B"),CSC,true);
      Tensor<double> ARef("ARef",{rows,cols},CSC);

      int Ksize=100;
      Tensor<double> C("C",{rows,Ksize},Dense);
      util::fillTensor(C,util::FillMethod::Dense);
      Format densedenseColMajorMatrixFormat({Dense, Dense},{1,0});
      Tensor<double> D("D",{Ksize,cols},densedenseColMajorMatrixFormat);
      util::fillTensor(D,util::FillMethod::Dense);

      IndexVar i, j, k;
      ARef(i,k) = C(i,j)*D(j,k)*B(i,k);
      std::cout << "A=B o (CxD) -- " << endl;

      TACO_BENCH(ARef.compile();, "Compile",1,timevalue,false)
      TACO_BENCH(ARef.assemble();,"Assemble",1,timevalue,false)
      TACO_BENCH(ARef.compute();, "Compute",repeat, timevalue, true)

      B=read(inputFilenames.at("B"),CSC,true);
      exprOperands.insert({"ARef",ARef});
      exprOperands.insert({"B",B});
      exprOperands.insert({"C",C});
      exprOperands.insert({"D",D});
      break;
    }
    default: {
      return reportError("Unknown Expression", 3);
    }
  }
#ifdef EIGEN
  if (products.at("EIGEN")) {
    exprToEIGEN(Expr,exprOperands,repeat,timevalue);
  }
#endif
#ifdef UBLAS
  if (products.at("UBLAS")) {
    exprToUBLAS(Expr,exprOperands,repeat,timevalue);
  }
#endif
#ifdef GMM
  if (products.at("GMM")) {
    exprToGMM(Expr,exprOperands,repeat,timevalue);
  }
#endif
#ifdef MKL
  if (products.at("MKL")) {
    exprToMKL(Expr,exprOperands,repeat,timevalue);
  }
#endif
#ifdef POSKI
  if (products.at("POSKI")) {
    exprToPOSKI(Expr,exprOperands,repeat,timevalue);
  }
#endif
#ifdef OSKI
  if (products.at("OSKI")) {
    exprToOSKI(Expr,exprOperands,repeat,timevalue);
  }
#endif
#ifdef YOURS
  if (products.at("YOURS")) {
    exprToYOURS(Expr,exprOperands,repeat,timevalue);
  }
#endif
}

