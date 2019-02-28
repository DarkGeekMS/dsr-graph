#ifndef SPECIFICWORKER_H
#define SPECIFICWORKER_H

#include <genericworker.h>
#include <innermodel/innermodel.h>
#include "../../../graph-related-classes/CRDT.h"
#include "../../../graph-related-classes/CRDT_graphviewer.h"
#include "../../../graph-related-classes/libs/DSRGraph.h"
#include <random>

#define LAPS 150
#define NODES 25
#define TIMEOUT 5

class SpecificWorker : public GenericWorker
{
	Q_OBJECT
	public:
		SpecificWorker(TuplePrx tprx);
		~SpecificWorker();
		bool setParams(RoboCompCommonBehavior::ParameterList params);
		std::shared_ptr<CRDT::CRDTGraph> getGCRDT() const {return gcrdt;};

	public slots:
		void compute();
		void initialize(int period);

	signals:
		void addNodeSIGNAL(std::int32_t id, const std::string &name, const std::string &type, float posx, float posy, const std::string &color);
		void addEdgeSIGNAL(std::int32_t from, std::int32_t to, const std::string &ege_tag);

	private:
		InnerModel *innerModel;
		std::string agent_name;
		std::shared_ptr<CRDT::CRDTGraph> gcrdt;
		std::unique_ptr<DSR::GraphViewer> graph_viewer;

		void tester();

		// Random
		std::random_device rd;
		std::mt19937 mt;
		std::uniform_real_distribution<float> dist;
};

#endif
