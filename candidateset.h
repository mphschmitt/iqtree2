/*
 * candidateset.h
 *
 *  Created on: Jun 1, 2014
 *      Author: Tung Nguyen
 */

#ifndef CANDIDATESET_H_
#define CANDIDATESET_H_
#include "tools.h"
#include "alignment.h"
#include <stack>

struct CandidateTree {
	string tree; // with branch length
	string topology; // tree topology WITHOUT branch lengths and WITH TAXON ID (instead of taxon names) for sorting purpose
	double score; // log-likelihood under ML or parsimony score
};


/**
 * Candidate tree set
 */
class CandidateSet : public multimap<double, CandidateTree> {

public:
    /**
     * constructor
     */
	CandidateSet(int limit, int max_candidates, Alignment *aln);

	CandidateSet();

    /**
     * return randomly one candidate tree from max_candidate
     */
    string getRandCandTree();

    /**
     * return the next parent tree for reproduction.
     * Here we always maintain a list of candidate trees which have not
     * been used for reproduction. If all candidate trees have been used, we select the
     * current best trees as the new parent trees
     */
    string getNextCandTree();

    /**
     *  Replace an existing tree in the candidate set
     *  @param tree the new tree string that will replace the existing tree
     *  @param score the score of the new tree
     *  @return true if the topology of \a tree exist in the candidate set
     */
    bool replaceTree(string tree, double score);

    /**
     *  create the parent tree set containing top trees
     */
    void initParentTrees();

    /**
     * update / insert tree into set of score is higher than lowest-scoring tree
     * @return true if the candidate set is updated, otherwise false
     *
     */
    bool update(string tree, double score);

    /**
     *  print score of max_candidates best trees
     *
     *  @param numScore
     *  	Number of best scores to print out starting from the highest
     */
    vector<double> getBestScores(int numScore);

    /**
     *  Return \a numTree best tree strings
     *  @param numTree number of best trees
     *  @return a list of tree
     */
    vector<string> getBestTrees(int numTree = 0);

    /**
     * get tree(s) with highest score. More than one tree is
     * returned if there are multiple optima.
     * @return a vector containing optimal trees
     */
    vector<string> getBestTree();

    /**
     * destructor
     */
    virtual ~CandidateSet();

    /**
     * hard limit for number of trees (typically superset of candidate set)
     */
    int limit;

    /**
     *  best score in the set
     */
    double bestScore;

    /**
     *  maximum number of candidate trees
     */
    int max_candidates;

    /** index of tree topologies in set
     *
     */
    StringDoubleHashMap topologies;

    /**
     *  Trees used for reproduction
     */
    stack<string> parentTrees;

    /**
     * pointer to alignment, just to assign correct IDs for taxa
     */
    Alignment *aln;

    /**
     * check if tree topology WITHOUT branch length exist in the candidate set?
     */
    bool treeTopologyExist(string topo);

    /**
     * check if tree topology WITH branch length exist in the candidate set?
     */
    bool treeExist(string tree);

    /**
     * return a unique topology (sorted by taxon names, rooted at taxon with alphabetically smallest name) without branch lengths
     */
    string getTopology(string tree);

};

#endif /* CANDIDATESET_H_ */
