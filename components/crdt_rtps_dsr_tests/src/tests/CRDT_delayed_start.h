//
// Created by juancarlos on 7/5/20.
//

#ifndef CRDT_RTPS_DSR_delayed_start_H
#define CRDT_RTPS_DSR_delayed_start_H

#include "../../../../graph-related-classes/CRDT.h"
#include "DSR_test.h"


class CRDT_delayed_start : DSR_test {
public:
    CRDT_delayed_start () {};
    CRDT_delayed_start(RoboCompDSRGetID::DSRGetIDPrxPtr id_prx, shared_ptr<CRDT::CRDTGraph> G_, const std::string& output_, int num_ops_, int agent_id_)
        : DSR_test(id_prx, G_, output_), num_ops(num_ops_), agent_id(agent_id_) {};

    CRDT_delayed_start& operator=(CRDT_delayed_start&& t) {
        dsrgetid_proxy = std::move(t.dsrgetid_proxy);
        G = t.G;
        agent_id = t.agent_id;
        output = std::move(t.output);
        num_ops = t.num_ops;
        return *this;
    }

    ~CRDT_delayed_start () {};

    void save_json_result();
    void run_test();

private:

    std::chrono::steady_clock::time_point start, end;
    int num_ops;
    int agent_id;

    void create_or_remove_nodes(int i, const shared_ptr<CRDT::CRDTGraph>& G);
    int delay = 5; //ms

};


#endif //CRDT_RTPS_DSR_delayed_start_H