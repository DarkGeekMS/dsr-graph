/*
 *    Copyright (C)2018 by YOUR NAME HERE
 *
 *    This file is part of RoboComp
 *
 *    RoboComp is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RoboComp is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with RoboComp.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "specificworker.h"

/**
* \brief Default constructor
*/
SpecificWorker::SpecificWorker(TuplePrx tprx) : GenericWorker(tprx) {
}

/**
* \brief Default destructor
*/
SpecificWorker::~SpecificWorker() {
    std::cout << "Destroying SpecificWorker" << std::endl;
    G.reset();

}

bool SpecificWorker::setParams(RoboCompCommonBehavior::ParameterList params) 
{
    
    agent_id = stoi(params["agent_id"].value);
    dsr_output_file = params["dsr_output_file"].value;
    dsr_input_file = params["dsr_input_file"].value;
    test_output_file = params["test_output_file"].value;
    dsr_input_file = params["dsr_input_file"].value;    
    return true;
}

void SpecificWorker::initialize(int period) 
{
    std::cout << "Initialize worker" << std::endl;

    // create graph
    G = std::make_shared<CRDT::CRDTGraph>(0, agent_name, agent_id, ""); // Init nodes
    G->print();

    // GraphViewer creation
    graph_viewer = std::make_unique<DSR::GraphViewer>(std::shared_ptr<SpecificWorker>(this));
    
    // Random initialization
    // mt = std::mt19937(rd());
    // unif_float = std::uniform_real_distribution((float)-40.0, (float)40.0);
    // unif_int = std::uniform_int_distribution((int)100, (int)140.0);
    timer.setSingleShot(true);
    timer.start(100);
}

void SpecificWorker::compute()
{
    auto vertex = G->get_vertex("world");
    vertex->print();
    auto edge = vertex->get_edge(131, "RT");
    edge->print();
    auto edges = vertex->get_edges();
    for(auto e: edges)
        e->print();


}


