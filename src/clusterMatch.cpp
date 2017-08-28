#include "pagoda2.h"

using namespace similarity;

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

#include <forward_list>
#include <set>

using namespace std;
using namespace Rcpp;

// Data structures
struct queryResult {
  int32_t s;
  int32_t e;
  float d;
};

// Function Prototypes
void readNumericMatrixIntoObjectVector(NumericMatrix &m,
                                       ObjectVector &dataSet,
                                       SpaceCosineSimilarity<float> &space);

Index<float>* makeIndex(SpaceCosineSimilarity<float> &space,
                        ObjectVector &dataSet,
                        bool verbose,
                        int nThreads) ;


forward_list<queryResult>* queryIndex(Index<float> *index,
                                      ObjectVector &dataset, SpaceCosineSimilarity<float> &space,
                                      int k, bool verbose );



// A functor that compares two query results
// only on the nodes contained
struct queryResultCompareNodes {
  bool operator() (const queryResult& a, const queryResult& b) const {
    return a.s < b.s || ! (b.s < a.s) && a.e < b.e;
  }
};


forward_list<queryResult>* findMNN(forward_list<queryResult>* qrA, forward_list<queryResult>* qrB, bool verbose = true) {
  std::chrono::time_point<std::chrono::high_resolution_clock> t1, t2;

  if (verbose) {
    t1 = high_resolution_clock::now();
    cout << "finding MNN..." << flush;
  }


  // Put results of A into a set that ignores the distances
  // Sets are the fastest for lookup
  set<queryResult, queryResultCompareNodes> setA;
  for (queryResult &qr: *qrA) {
    setA.insert(qr);
  }

  int nhits = 0;

  forward_list<queryResult> *MNN = new forward_list<queryResult>();

  for (queryResult &qr: *qrB) {
    queryResult qrTmp;
    qrTmp.s = qr.e;
    qrTmp.e = qr.s;
    qrTmp.d = qr.d;  // don't care about d here

    auto search = setA.find(qrTmp);
    if (search != setA.end()) {
      MNN->push_front(qr);
    }
  }


  if (verbose) {
    t2 = high_resolution_clock::now();
    auto elapsed_time = duration_cast<duration<double>>(t2 - t1).count();
    cout << endl << "done (" << elapsed_time << "s)" << endl;
  }

  return MNN;
}


// [[Rcpp::export]]
DataFrame mutualNN(NumericMatrix mA, NumericMatrix mB, NumericVector kA, NumericVector kB) {
  bool verbose = true;
  int nThreads = 30;
  int kvalA = kA[0];
  int kvalB = kB[0];

  if(verbose) cout << "Starting..." << endl << flush;
  initLibrary(LIB_LOGNONE, NULL);
  // initLibrary(LIB_LOGSTDERR, NULL);

  AnyParams empty;
  SpaceCosineSimilarity<float> space;

  // Converting format for A
  if (verbose) cout << "reading points from mA..." << flush;
  ObjectVector datasetA;
  readNumericMatrixIntoObjectVector(mA, datasetA, space);
  if (verbose) cout << "done (" << datasetA.size() << " points)" << endl;

  // Converting format for B
  if (verbose) cout << "reading points from mB ..." << flush;
  ObjectVector datasetB;
  readNumericMatrixIntoObjectVector(mB, datasetB, space);
  if (verbose) cout << "done (" << datasetB.size() << " points)" << endl;

  // Make the index for A
  Index<float> *indexA;
  indexA = makeIndex(space, datasetA, verbose, nThreads);

  // Make the index for B
  Index<float> *indexB;
  indexB = makeIndex(space, datasetB, verbose, nThreads);

  // query the index of B with objects from A
  forward_list<queryResult>* qrA;
  qrA = queryIndex(indexB, datasetA, space, kvalA, true);

  // query the index of A with object from B
  forward_list<queryResult>* qrB;
  qrB = queryIndex(indexA, datasetB, space, kvalB, true);

  forward_list<queryResult> *mnn= findMNN(qrA, qrB);

  delete indexA;
  delete indexB;

  delete qrA;
  delete qrB;

  vector<int> startV, endV;
  vector<float> distV;
  for (queryResult &qr : *mnn) {
    // Add 1 here to get the indexes to be 1 indexed as in R
    startV.push_back(qr.s + 1);
    endV.push_back(qr.e + 1);
    distV.push_back(qr.d);
  }

  for (const Object* obj: datasetA) delete obj;
  for (const Object* obj: datasetB) delete obj;

  return DataFrame::create( _["mA.id"] = endV, _["mB.id"] = startV, _["dist"] = distV);
}

// Make an index
Index<float>* makeIndex(SpaceCosineSimilarity<float> &space, ObjectVector &dataSet, bool verbose, int nThreads) {
  std::chrono::time_point<std::chrono::high_resolution_clock> t1, t2;

  if (verbose) {
    t1 = high_resolution_clock::now();
    cout << "building index..." << flush;
  }

  AnyParams IndexParams({
    "M=20",
    "efConstruction=100",
    "indexThreadQty=" + std::to_string(nThreads), /* number of indexing threads */
    "searchMethod=4",
  });
  AnyParams QueryTimeParams({
    "efSearch=100",
  });

  Index<float> *index = MethodFactoryRegistry<float>::Instance().CreateMethod(verbose,"hnsw",
                                                               "consinesimil",space, dataSet);
  index->CreateIndex(IndexParams);
  index->SetQueryTimeParams(QueryTimeParams);


  if (verbose) {
    t2 = high_resolution_clock::now();
    auto elapsed_time = duration_cast<duration<double>>(t2 - t1).count();
    cout << endl << "done (" << elapsed_time << "s)" << endl;
  }

  return index;

}


// Convert a numeric matrix into object vector
void readNumericMatrixIntoObjectVector(NumericMatrix &m, ObjectVector &dataSet,
                                       SpaceCosineSimilarity<float> &space) {

  auto nrow = m.nrow();

  for (int i = 0; i < nrow; i++) {
    NumericVector nv = m.row(i);
    vector<float> v = Rcpp::as<vector<float>>(nv);

    Object *o = space.CreateObjFromVect(i, NULL, v);
    dataSet.push_back(o);
  }
}



forward_list<queryResult>* queryIndex(Index<float> *index,
                                      ObjectVector &dataset, SpaceCosineSimilarity<float> &space,
                                      int k, bool verbose ) {

  forward_list<queryResult> *queryResults = new forward_list<queryResult>();
  std::chrono::time_point<std::chrono::high_resolution_clock> t1, t2;

  int nqueries = dataset.size(); // ObjectVector is just a std::vector

  if (verbose) {
    cout << "running queries with k=" << k << " ..." << flush;
    t1 = high_resolution_clock::now();
  }

  unique_ptr<boost::progress_display> query_bar(verbose ? new boost::progress_display(nqueries) : NULL);


  for (int i = 0; i < nqueries; i++) {
    const Object *queryObj = dataset[i];

    KNNQuery<float> knnQ(space, queryObj, k);
    index->Search(&knnQ);

    KNNQueue<float> *res = knnQ.Result()->Clone();

    while(!res->Empty()) {
      queryResult qr;
      qr.s = i;
      qr.e = res->TopObject()->id();
      qr.d = res->TopDistance();
      queryResults->push_front(qr);

      res->Pop();
    }

    if(query_bar) { ++(*query_bar); }
  }

  if (verbose) {
    t2 = high_resolution_clock::now();
    auto elapsed_time = duration_cast<duration<double>>(t2 - t1).count();
    cout << endl << "done (" << elapsed_time << "s)" << endl;
  }

  return queryResults;
}

