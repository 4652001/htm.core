/* ---------------------------------------------------------------------
 * Copyright (C) 2018-2019, David McDougall, @breznak
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 * ----------------------------------------------------------------------
 */

/**
 * Solving the MNIST dataset with Spatial Pooler. Parallel demonstartion using c++17 TS Parallel (TBB)
 * Requirements:
 *   - c++17 codebase
 *   - compiler: MSVC 2017+, g++-9
 *   - link with TBB (The Building Blocks)
 *
 *
 * Note 1: the example is more ugly, because we parallelize for-loop, compared to std::algorithms `sort(execution::policy::par, a.begin(), a.end());`
 * Note 2: Running SP.compute() in parallel is useless for sequences, but works for MNIST and the likes. 
 *
 * This consists of a simple black & white image encoder, a spatial pool, and an
 * SDR classifier.  The task is to recognise images of hand written numbers 0-9.
 * This should score at least 95%.
 */

#include <cstdint> //uint8_t
#include <iostream>
#include <fstream>      // std::ofstream
#include <vector>

#include <htm/algorithms/SpatialPooler.hpp>
#include <htm/algorithms/SDRClassifier.hpp>
#include <htm/utils/SdrMetrics.hpp>
#include <htm/os/Timer.hpp>

#include <htm/types/Parallelizable.hpp>

#include <mnist/mnist_reader.hpp> // MNIST data itself + read methods, namespace mnist::
#include <mnist/mnist_utils.hpp>  // mnist::binarize_dataset

using namespace std;
using namespace htm;

class MNIST {
/**
 * RESULTS: Store results in the MNIST_SP.cpp file only, this parallel is just for experimenting with parallelization.
 */

  private:
    SpatialPooler sp;
    SDR input;
    SDR columns;
    Classifier clsr;
    mnist::MNIST_dataset<std::vector, std::vector<uint8_t>, uint8_t> dataset;

  public:
    UInt verbosity = 1;
    const UInt train_dataset_iterations = 20u; //epochs somewhat help, at linear time


void setup() {

  input.initialize({28, 28, 1}); 
  columns.initialize({28, 28, 8}); //1D vs 2D no big difference, 2D seems more natural for the problem. Speed-----, Results+++++++++; #columns HIGHEST impact. 
  sp.initialize(
    /* inputDimensions */             input.dimensions,
    /* columnDimensions */            columns.dimensions,
    /* potentialRadius */             7, // with 2D, 7 results in 15x15 area, which is cca 25% for the input area. Slightly improves than 99999 aka "no topology, all to all connections"
    /* potentialPct */                0.1f, //we have only 10 classes, and << #columns. So we want to force each col to specialize. Cca 0.3 w "7" above, or very small (0.1) for "no topology". Cannot be too small due to internal checks. Speed++
    /* globalInhibition */            true, //Speed+++++++; SDR quality-- (global does have active nearby cols, which we want to avoid (local)); Results+-0
    /* localAreaDensity */            0.1f,  // % active bits
    /* stimulusThreshold */           6u,
    /* synPermInactiveDec */          0.002f, //very low values better for MNIST
    /* synPermActiveInc */            0.14f, //takes upto 5x steps to get dis/connected
    /* synPermConnected */            0.5f, //no difference, let's leave at 0.5 in the middle
    /* minPctOverlapDutyCycles */     0.2f, //speed of re-learning?
    /* dutyCyclePeriod */             1402,
    /* boostStrength */               12.0f, // Boosting does help, but entropy is high, on MNIST it does not matter, for learning with TM prefer boosting off (BOOSTING_DISABLED), or "neutral"=1.0
    /* seed */                        4u,
    /* spVerbosity */                 1u,
    /* wrapAround */                  true); // does not matter (helps slightly)

  // Save the connections to file for postmortem analysis.
  ofstream dump("mnist_sp_initial.connections", ofstream::binary | ofstream::trunc | ofstream::out);
  sp.connections.save( dump );
  dump.close();

  clsr.initialize( /* alpha */ 0.001f);

  dataset = mnist::read_dataset<std::vector, std::vector, uint8_t, uint8_t>(string("../ThirdParty/mnist_data/mnist-src/")); //from CMake
  mnist::binarize_dataset(dataset);
}


/**
 *  train the SP on the training set. 
 *  @param skipSP bool (default false) if set, output directly the input to the classifier.
 *  This is used for a baseline benchmark (Classifier directly learns on input images)
 */
void train(const bool skipSP=false) {
  // Train

  if(verbosity)
    cout << "Training for " << (train_dataset_iterations * dataset.training_labels.size())
         << " cycles ..." << endl;
  size_t i = 0;

  Metrics inputStats(input,    1402);
  Metrics columnStats(columns, 1402);

  Timer tTrain(true);

  for(auto epoch = 0u; epoch < train_dataset_iterations; epoch++) {
    NTA_INFO << "epoch " << epoch;
    // Shuffle the training data.
    vector<UInt> index( dataset.training_labels.size() );
    for (UInt i=0; i<dataset.training_labels.size(); i++) {
      index.push_back(i);
    }
    Random().shuffle( index.begin(), index.end() );



    //parallel loop with TBB
    std::mutex m;
    tbb::parallel_for( tbb::blocked_range<size_t>(0, index.size()),
                       [&](tbb::blocked_range<size_t> r) {
//    for(size_t i=0; i< index.size(); i++) { // index = order of label (shuffeled)
      for(auto i = r.begin(); i < r.end(); ++i) {

      const auto idx = index[i];
      // Get the input & label
      const auto image = dataset.training_images.at(idx);
      const UInt label  = dataset.training_labels.at(idx);

      // Compute & Train
      SDR Pinput(input.dimensions);
      Pinput.setDense( image );

      SDR Pcolumns({28,28,8});
      if(not skipSP)
        sp.compute(Pinput, true, Pcolumns); //TODO change to return output?
      //TODO make compute() const for parallelization? 
      
      // sync this 
      {
        m.lock(); //TODO use better locks than just mutex, unique_lock etc
        clsr.learn( Pcolumns, {label} );
        clsr.learn( skipSP ? Pinput : Pcolumns, {label} );
        m.unlock(); 
      }
      if( verbosity && (++i % 1000 == 0) ) cout << "." << flush;
    }
    }); // !end of lambda


    if( verbosity ) cout << endl;
  
  cout << "epoch ended" << endl;
  cout << "inputStats "  << inputStats << endl;
  cout << "columnStats " << columnStats << endl;
  cout << sp << endl;
  }
  
  tTrain.stop();
  cout << "MNIST train time: " << tTrain.getElapsed() << endl; 

  // Save the connections to file for postmortem analysis.
  ofstream dump("mnist_sp_learned.connections", ofstream::binary | ofstream::trunc | ofstream::out);
  sp.connections.save( dump );
  dump.close();
}

void test(const bool skipSP=false) {
  // Test
  Real score = 0;
  UInt n_samples = 0;
  if(verbosity)
    cout << "Testing for " << dataset.test_labels.size() << " cycles ..." << endl;
  for(UInt i = 0; i < dataset.test_labels.size(); i++) {
    // Get the input & label
    const auto image  = dataset.test_images.at(i);
    const UInt label  = dataset.test_labels.at(i);

    // Compute
    input.setDense( image );
    if(not skipSP) 
      sp.compute(input, false, columns);

    // Check results
    if( argmax( clsr.infer( skipSP ? input : columns ) ) == label)
        score += 1;
    n_samples += 1;
    if( verbosity && i % 1000 == 0 ) cout << "." << flush;
  }
  if( verbosity ) cout << endl;
  cout << "===========RESULTs=================" << endl;
  cout << "Score: " << 100.0 * score / n_samples << "% ("<< (n_samples - score) << " / " << n_samples << " wrong). "   << endl;
  cout << "SDR example: " << columns << endl;
}

};  // End class MNIST

int main(int argc, char **argv) {
  MNIST m;
  cout << "=========== Spatial Pooler (parallel) =====================" << endl;
  m.setup();
  m.train();
  m.test();

  return 0;
}

