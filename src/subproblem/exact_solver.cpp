//  Created by Alberto Santini on 18/09/13.
//  Copyright (c) 2013 Alberto Santini. All rights reserved.
//

#include <limits>
#include <list>

#include <boost/graph/r_c_shortest_paths.hpp>

#include <subproblem/exact_solver.h>
#include <subproblem/labelling.h>

std::vector<Solution> ExactSolver::solve(double& time_spent) const {
    std::vector<Solution> sols;
    
    std::vector<Path> optimal_paths;
    std::vector<Label> optimal_labels;
    
    NodeIdFunctor nf(g);
    ArcIdFunctor af(g);
    
    auto vc = g->vessel_class;
    
    auto cl_start = clock();
    
    r_c_shortest_paths(
        g->graph,
        make_property_map<Vertex>(nf),
        make_property_map<Edge>(af),
        g->h1().second,
        g->h2().second,
        optimal_paths,
        optimal_labels,
        Label(g, vc->capacity, vc->capacity, 0),
        LabelExtender(),
        Dominance(),
        std::allocator<r_c_shortest_paths_label<BGraph, Label>>(),
        default_r_c_shortest_paths_visitor()
    );
        
    auto cl_end = clock();
    auto time_s = (double(cl_end - cl_start) / CLOCKS_PER_SEC);
    if(time_s - time_spent > std::numeric_limits<double>::epsilon()) {
        time_spent = time_s;
    }
        
    for(auto i = 0u; i < optimal_paths.size(); i++) {
        sols.push_back(Solution(optimal_paths[i], g->calculate_cost(optimal_paths[i]), optimal_labels[i].cost, vc, g));
    }
    
    return sols;
}