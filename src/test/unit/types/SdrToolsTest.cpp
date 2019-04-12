/* ---------------------------------------------------------------------
 * Copyright (C) 2019, David McDougall.
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

#include <vector>
#include <gtest/gtest.h>
#include <nupic/types/Sdr.hpp>
#include <nupic/types/SdrTools.hpp>

namespace testing {
    
using namespace std;
using namespace nupic;
using namespace nupic::sdr;

TEST(SdrReshapeTest, TestReshapeExamples) {
    SDR     A(    { 4, 4 });
    Reshape B( A, { 8, 2 });
    A.setCoordinates(SDR_coordinate_t({{1, 1, 2}, {0, 1, 2}}));
    auto coords = B.getCoordinates();
    ASSERT_EQ(coords, SDR_coordinate_t({{2, 2, 5}, {0, 1, 0}}));
}

TEST(SdrReshapeTest, TestReshapeConstructor) {
    SDR       A({ 11 });
    Reshape   B( A );
    ASSERT_EQ( A.dimensions, B.dimensions );
    Reshape   C( A, { 11 });
    SDR       D({ 5, 4, 3, 2, 1 });
    Reshape   E( D, {1, 1, 1, 120, 1});
    Reshape   F( D, { 20, 6 });
    Reshape   X( (SDR&) F );

    // Test that SDR Reshapes can be safely made and destroyed.
    Reshape *G = new Reshape( A );
    Reshape *H = new Reshape( A );
    Reshape *I = new Reshape( A );
    A.zero();
    H->getDense();
    delete H;
    I->getDense();
    A.zero();
    Reshape *J = new Reshape( A );
    J->getDense();
    Reshape *K = new Reshape( A );
    delete K;
    Reshape *L = new Reshape( A );
    L->getCoordinates();
    delete L;
    delete G;
    I->getCoordinates();
    delete I;
    delete J;
    A.getDense();

    // Test invalid dimensions
    ASSERT_ANY_THROW( new Reshape( A, {2, 5}) );
    ASSERT_ANY_THROW( new Reshape( A, {11, 0}) );
}

TEST(SdrReshapeTest, TestReshapeDeconstructor) {
    SDR     *A = new SDR({12});
    Reshape *B = new Reshape( *A );
    Reshape *C = new Reshape( *A, {3, 4} );
    Reshape *D = new Reshape( *C, {4, 3} );
    Reshape *E = new Reshape( *C, {2, 6} );
    D->getDense();
    E->getCoordinates();
    // Test subtree deletion
    delete C;
    ASSERT_ANY_THROW( D->getDense() );
    ASSERT_ANY_THROW( E->getCoordinates() );
    ASSERT_ANY_THROW( new Reshape( *E ) );
    delete D;
    // Test rest of tree is OK.
    B->getSparse();
    A->zero();
    B->getSparse();
    // Test delete root.
    delete A;
    ASSERT_ANY_THROW( B->getDense() );
    ASSERT_ANY_THROW( E->getCoordinates() );
    // Cleanup remaining Reshapes.
    delete B;
    delete E;
}

TEST(SdrReshapeTest, TestReshapeThrows) {
    SDR A({10});
    Reshape B(A, {2, 5});
    SDR *C = &B;

    ASSERT_ANY_THROW( C->setDense( SDR_dense_t( 10, 1 ) ));
    ASSERT_ANY_THROW( C->setCoordinates( SDR_coordinate_t({ {0}, {0} }) ));
    ASSERT_ANY_THROW( C->setSparse( SDR_sparse_t({ 0, 1, 2 }) ));
    SDR X({10});
    ASSERT_ANY_THROW( C->setSDR( X ));
    ASSERT_ANY_THROW( C->randomize(0.10f) );
    ASSERT_ANY_THROW( C->addNoise(0.10f) );
}

TEST(SdrReshapeTest, TestReshapeGetters) {
    SDR A({ 2, 3 });
    Reshape B( A, { 3, 2 });
    SDR *C = &B;
    // Test getting dense
    A.setDense( SDR_dense_t({ 0, 1, 0, 0, 1, 0 }) );
    ASSERT_EQ( C->getDense(), SDR_dense_t({ 0, 1, 0, 0, 1, 0 }) );

    // Test getting coordinates
    A.setCoordinates( SDR_coordinate_t({ {0, 1}, {0, 1} }));
    ASSERT_EQ( C->getCoordinates(), SDR_coordinate_t({ {0, 2}, {0, 0} }) );

    // Test getting sparse
    A.setSparse( SDR_sparse_t({ 2, 3 }));
    ASSERT_EQ( C->getSparse(), SDR_sparse_t({ 2, 3 }) );

    // Test getting coordinates, a second time.
    A.setSparse( SDR_sparse_t({ 2, 3 }));
    ASSERT_EQ( C->getCoordinates(), SDR_coordinate_t({ {1, 1}, {0, 1} }) );

    // Test getting coordinates, when the parent SDR already has coordinates
    // computed and the dimensions are the same.
    A.zero();
    Reshape D( A );
    SDR *E = &D;
    A.setCoordinates( SDR_coordinate_t({ {0, 1}, {0, 1} }));
    ASSERT_EQ( E->getCoordinates(), SDR_coordinate_t({ {0, 1}, {0, 1} }) );
}

TEST(SdrReshapeTest, TestSaveLoad) {
    const char *filename = "SdrReshapeSerialization.tmp";
    ofstream outfile;
    outfile.open(filename);

    // Test zero value
    SDR zero({ 3, 3 });
    Reshape z( zero );
    z.save( outfile );

    // Test dense data
    SDR dense({ 3, 3 });
    Reshape d( dense );
    dense.setDense(SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }));
    Serializable &ser = d;
    ser.save( outfile );

    // Test sparse data
    SDR sparse({ 3, 3 });
    Reshape f( sparse );
    sparse.setSparse(SDR_sparse_t({ 1, 4, 8 }));
    f.save( outfile );

    // Test coordinate data
    SDR coord({ 3, 3 });
    Reshape x( coord );
    coord.setCoordinates(SDR_coordinate_t({
            { 0, 1, 2 },
            { 1, 1, 2 }}));
    x.save( outfile );

    // Now load all of the data back into SDRs.
    outfile.close();
    ifstream infile( filename );

    SDR zero_2;
    zero_2.load( infile );
    SDR dense_2;
    dense_2.load( infile );
    SDR sparse_2;
    sparse_2.load( infile );
    SDR coord_2;
    coord_2.load( infile );

    infile.close();
    int ret = ::remove( filename );
    EXPECT_TRUE(ret == 0) << "Failed to delete " << filename;

    // Check that all of the data is OK
    ASSERT_TRUE( zero    == zero_2 );
    ASSERT_TRUE( dense   == dense_2 );
    ASSERT_TRUE( sparse  == sparse_2 );
    ASSERT_TRUE( coord   == coord_2 );
}
}
