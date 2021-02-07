//
//  upgma.h
//  iqtree
//
//  Created by James Barbetti on 31/10/20.
//

#ifndef upgma_h
#define upgma_h

#include "distancematrix.h"          //for Matrix template class
#include "clustertree.h"             //for ClusterTree template class
                                     //rows of the S and I matrices
                                     //See [SMP2011], section 2.5.
#include <vector>                    //for std::vector
#include <string>                    //sequence names stored as std::string
#include "progress.h"                //for progress_display
#include "my_assert.h"               //for ASSERT macro

typedef float    NJFloat;
const   NJFloat  infiniteDistance = (NJFloat)(1e+36);
const   intptr_t notMappedToRow = -1;

#ifdef   USE_VECTORCLASS_LIBRARY
#include <vectorclass/vectorclass.h> //for Vec4d and Vec4db vector classes
typedef  Vec8f    FloatVector;
typedef  Vec8fb   FloatBoolVector;
#endif

namespace StartTree
{
template <class T=NJFloat> struct Position
{
    //A position (row, column) in an NJ matrix
    //Note that column is always less than row.
    //(Because that's the convention in RapidNJ).
public:
    intptr_t row;
    intptr_t column;
    T        value;
    size_t   imbalance;
    Position() : row(0), column(0), value(0), imbalance(0) {}
    Position(size_t r, size_t c, T v, size_t imbalance)
        : row(r), column(c), value(v) {}
    Position& operator = (const Position &rhs) {
        row       = rhs.row;
        column    = rhs.column;
        value     = rhs.value;
        imbalance = rhs.imbalance;
        return *this;
    }
    bool operator< ( const Position& rhs ) const {
        return value < rhs.value
        || (value == rhs.value && imbalance < rhs.imbalance);
    }
    bool operator<= ( const Position& rhs ) const {
        return value < rhs.value
        || (value == rhs.value && imbalance <= rhs.imbalance);
    }
};

template <class T> class Positions : public std::vector<Position<T>>
{
};


template <class T=NJFloat> class UPGMA_Matrix: public SquareMatrix<T> {
    //UPGMA_Matrix is a D matrix (a matrix of distances).
public:
    typedef SquareMatrix<T> super;
    using super::rows;
    using super::setSize;
    using super::loadDistancesFromFlatArray;
    using super::calculateRowTotals;
    using super::removeRowAndColumn;
    using super::row_count;
    using super::column_count;
protected:
    std::vector<size_t>  rowToCluster; //*not* initialized by setSize
    ClusterTree<T>       clusters;     //*not* touched by setSize
    bool                 isOutputToBeZipped;
    mutable Positions<T> rowMinima;    //*not* touched by setSize
    bool                 silent;
public:
    UPGMA_Matrix():super(), isOutputToBeZipped(false), silent(false) {
    }
    virtual std::string getAlgorithmName() const {
        return "UPGMA";
    }
    virtual void setSize(size_t rank) {
        super::setSize(rank);
        rowToCluster.clear();
        for (int r=0; r<row_count; ++r) {
            rowToCluster.emplace_back(r);
        }
    }
    virtual void addCluster(const std::string &name) {
        clusters.addCluster(name);
    }
    bool loadMatrixFromFile(const std::string &distanceMatrixFilePath) {
        bool rc = loadDistanceMatrixInto(distanceMatrixFilePath, true, *this);
        calculateRowTotals();
        return rc;
    }
    virtual bool loadMatrix(const std::vector<std::string>& names, const double* matrix) {
        //Assumptions: 2 < names.size(), all names distinct
        //  matrix is symmetric, with matrix[row*names.size()+col]
        //  containing the distance between taxon row and taxon col.
        setSize(names.size());
        clusters.clear();
        for (auto it = names.begin(); it != names.end(); ++it) {
            clusters.addCluster(*it);
        }
        loadDistancesFromFlatArray(matrix);
        calculateRowTotals();
        return true;
    }
    virtual bool constructTree() {
        Position<T> best;
        std::string taskName = "Constructing " + getAlgorithmName() + " tree";
        if (silent) {
            taskName="";
        }
        #if USES_PROGRESS_DISPLAY
        double triangle = row_count * (row_count + 1.0) * 0.5;
        progress_display show_progress(triangle, taskName.c_str(), "", "");
        #else
        double show_progress = 0;
        #endif
        while ( 3 < row_count ) {
            getMinimumEntry(best);
            cluster(best.column, best.row);
            show_progress += row_count;
        }
        finishClustering();
        #if USES_PROGRESS_DISPLAY
        show_progress.done();
        #endif
        return true;
    }
    virtual void setZippedOutput(bool zipIt) {
        isOutputToBeZipped = zipIt;
    }
    virtual void beSilent() {
        silent = true;
    }
    bool writeTreeFile(int precision, const std::string &treeFilePath) const {
        return clusters.writeTreeFile(isOutputToBeZipped, precision, treeFilePath);
    }
protected:
    void getMinimumEntry(Position<T> &best) {
        getRowMinima();
        best.value = infiniteDistance;
        for (intptr_t r=0; r<row_count; ++r) {
            Position<T> & here = rowMinima[r];
            if (here.value < best.value && here.row != here.column) {
                best = here;
            }
        }
    }
    virtual void getRowMinima() const
    {
        rowMinima.resize(row_count);
        rowMinima[0].value = infiniteDistance;
        #ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic)
        #endif
        for (intptr_t row=1; row<row_count; ++row) {
            T      bestVrc    = (T)infiniteDistance;
            size_t bestColumn = 0;
            const  T* rowData = rows[row];
            for (intptr_t col=0; col<row; ++col) {
                T    v      = rowData[col];
                bool better = ( v < bestVrc );
                if (better) {
                    bestColumn = col;
                    bestVrc = v;
                }
            }
            rowMinima[row] = Position<T>(row, bestColumn, bestVrc, getImbalance(row, bestColumn));
        }
    }
    virtual void finishClustering() {
        //But:  The formula is probably wrong. Felsenstein [2004] chapter 11 only
        //      covers UPGMA for rooted trees, and I don't know what
        //      the right formula is.
        //
        ASSERT( row_count == 3);
        T weights[3];
        T denominator = (T)0.0;
        for (size_t i=0; i<3; ++i) {
            weights[i]   = (T)clusters[rowToCluster[i]].countOfExteriorNodes;
            denominator += weights[i];
        }
        for (size_t i=0; i<3; ++i) {
            weights[i] /= ((T)2.0 * denominator);
        }
        clusters.addCluster
            ( rowToCluster[0], weights[1]*rows[0][1] + weights[2]*rows[0][2]
            , rowToCluster[1], weights[0]*rows[0][1] + weights[2]*rows[1][2]
            , rowToCluster[2], weights[0]*rows[0][2] + weights[1]*rows[1][2]);
        row_count = 0;
    }
    virtual void cluster(intptr_t a, intptr_t b) {
        T      aLength = rows[b][a] * (T)0.5;
        T      bLength = aLength;
        size_t aCount  = clusters[rowToCluster[a]].countOfExteriorNodes;
        size_t bCount  = clusters[rowToCluster[b]].countOfExteriorNodes;
        size_t tCount  = aCount + bCount;
        T      lambda  = (T)aCount / (T)tCount;
        T      mu      = (T)1.0 - lambda;
        auto rowA = rows[a];
        auto rowB = rows[b];
        for (intptr_t i=0; i<row_count; ++i) {
            if (i!=a && i!=b) {
                T Dai      = rowA[i];
                T Dbi      = rowB[i];
                T Dci      = lambda * Dai + mu * Dbi;
                rowA[i]    = Dci;
                rows[i][a] = Dci;
            }
        }
        clusters.addCluster ( rowToCluster[a], aLength,
                              rowToCluster[b], bLength);
        rowToCluster[a] = clusters.size()-1;
        rowToCluster[b] = rowToCluster[row_count-1];
        removeRowAndColumn(b);
    }
    size_t getImbalance(size_t rowA, size_t rowB) const {
        size_t clusterA = rowToCluster[rowA];
        size_t clusterB = rowToCluster[rowB];
        size_t sizeA    = clusters[clusterA].countOfExteriorNodes;
        size_t sizeB    = clusters[clusterB].countOfExteriorNodes;
        return (sizeA<sizeB) ? (sizeB-sizeA) : (sizeA-sizeB);
    }
};

#ifdef USE_VECTORCLASS_LIBRARY
template <class T=NJFloat, class V=FloatVector, class VB=FloatBoolVector>
class VectorizedUPGMA_Matrix: public UPGMA_Matrix<T>
{
protected:
    typedef UPGMA_Matrix<T> super;
    using super::rowMinima;
    using super::rows;
    using super::row_count;
    using super::calculateRowTotals;
    using super::getImbalance;
    const intptr_t blockSize;
    mutable std::vector<T> scratchColumnNumbers;
public:
    VectorizedUPGMA_Matrix() : super(), blockSize(VB().size()) {
    }
    virtual std::string getAlgorithmName() const {
        return "Vectorized-" + super::getAlgorithmName();
    }
    virtual void calculateRowTotals() const {
        size_t fluff = MATRIX_ALIGNMENT / sizeof(T);
        scratchColumnNumbers.resize(row_count + fluff, 0.0);
    }
    virtual void getRowMinima() const
    {
        T* nums = matrixAlign ( scratchColumnNumbers.data() );
        rowMinima.resize(row_count);
        rowMinima[0].value = infiniteDistance;
        #ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic)
        #endif
        for (intptr_t row=1; row<row_count; ++row) {
            Position<T> pos(row, 0, infiniteDistance, 0);
            const T* rowData = rows[row];
            intptr_t col;
            V minVector  = infiniteDistance;
            V ixVector   = -1 ;

            for (col=0; col+blockSize<row; col+=blockSize) {
                V  rowVector; rowVector.load_a(rowData+col);
                VB less      = rowVector < minVector;
                V  numVector; numVector.load_a(nums+col);
                ixVector     = select(less, numVector, ixVector);
                minVector    = select(less, rowVector, minVector);
            }
            //Extract minimum and column number
            for (int c=0; c<blockSize; ++c) {
                if (minVector[c] < pos.value) {
                    pos.value  = minVector[c];
                    pos.column = (size_t)ixVector[c];
                }
            }
            for (; col<row; ++col) {
                T dist = rowData[col];
                if (dist < pos.value) {
                    pos.column = col;
                    pos.value  = dist;
                }
            }
            pos.imbalance  = getImbalance(pos.row, pos.column);
            rowMinima[row] = pos;
        }
    }
};
#endif //USE_VECTORCLASS_LIBRARY

}

#endif /* upgma_h */
