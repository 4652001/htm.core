/* ---------------------------------------------------------------------
 * Copyright (C) 2018, David McDougall.
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
 * Solving the MNIST dataset with Spatial Pooler.
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

#include <mnist/mnist_reader.hpp> // MNIST data itself + read methods, namespace mnist::
#include <mnist/mnist_utils.hpp>  // mnist::binarize_dataset

namespace examples {

using namespace std;
using namespace htm;

class MNIST {

  private:
    SpatialPooler sp;
    SDR input;
    SDR columns;
    Classifier clsr;
    mnist::MNIST_dataset<std::vector, std::vector<uint8_t>, uint8_t> dataset;

  public:
    UInt verbosity = 1;
    const UInt train_dataset_iterations = 1u;


void setup() {

  input.initialize({28, 28, 1});
  columns.initialize({28, 28, 8});
  sp.initialize(
    /* inputDimensions */             input.dimensions,
    /* columnDimensions */            columns.dimensions,
    /* potentialRadius */             7u, // No topology, all to all connections.
    /* potentialPct */                0.1f,
    /* globalInhibition */            true,
    /* localAreaDensity */            0.05f,  // % active bits
    /* numActiveColumnsPerInhArea */  -1,
    /* stimulusThreshold */           6u,
    /* synPermInactiveDec */          0.005f,
    /* synPermActiveInc */            0.014f,
    /* synPermConnected */            0.1f,
    /* minPctOverlapDutyCycles */     0.001f,
    /* dutyCyclePeriod */             1402,
    /* boostStrength */               7.8f, // Boosting does help
    /* seed */                        93u,
    /* spVerbosity */                 1u,
    /* wrapAround */                  false); // No topology, turn off wrapping

  // Save the connections to file for postmortem analysis.
  ofstream dump("mnist_sp_initial.connections", ofstream::binary | ofstream::trunc | ofstream::out);
  sp.connections.save( dump );
  dump.close();

  clsr.initialize( /* alpha */ 0.001f);

  dataset = mnist::read_dataset<std::vector, std::vector, uint8_t, uint8_t>(string("../ThirdParty/mnist_data/mnist-src/")); //from CMake
  mnist::binarize_dataset(dataset);
}

void train() {
  // Train

  if(verbosity)
    cout << "Training for " << (train_dataset_iterations * dataset.training_labels.size())
         << " cycles ..." << endl;
  size_t i = 0;

  Metrics inputStats(input,    1402);
  Metrics columnStats(columns, 1402);

  for(auto epoch = 0u; epoch < train_dataset_iterations; epoch++) {
    NTA_INFO << "epoch " << epoch;
    // Shuffle the training data.
    vector<UInt> index( dataset.training_labels.size() );
    for (UInt i=0; i<dataset.training_labels.size(); i++) {
      index.push_back(i);
    }
    Random().shuffle( index.begin(), index.end() );

    for(const auto idx : index) { // index = order of label (shuffeled)
      // Get the input & label
      const auto image = dataset.training_images.at(idx);
      const UInt label  = dataset.training_labels.at(idx);

      // Compute & Train
      input.setDense( image );
      sp.compute(input, true, columns);
      clsr.learn( columns, {label} );
      if( verbosity && (++i % 1000 == 0) ) cout << "." << flush;
    }
    if( verbosity ) cout << endl;
  }
  cout << "epoch ended" << endl;
  cout << "inputStats "  << inputStats << endl;
  cout << "columnStats " << columnStats << endl;
  cout << sp << endl;

  // Save the connections to file for postmortem analysis.
  ofstream dump("mnist_sp_learned.connections", ofstream::binary | ofstream::trunc | ofstream::out);
  sp.connections.save( dump );
  dump.close();
}

void test() {
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
    sp.compute(input, false, columns);
    // Check results
    if( argmax( clsr.infer( columns ) ) == label)
        score += 1;
    n_samples += 1;
    if( verbosity && i % 1000 == 0 ) cout << "." << flush;
  }
  if( verbosity ) cout << endl;
  cout << "Score: " << 100.0 * score / n_samples << "% " << endl;
}

};  // End class MNIST
}   // End namespace examples

int main(int argc, char **argv) {
  examples::MNIST m;
  m.setup();
  m.train();
  m.test();

  return 0;
}

