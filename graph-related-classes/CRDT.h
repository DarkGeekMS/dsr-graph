//
// Created by crivac on 17/01/19.
//

#ifndef CRDT_GRAPH
#define CRDT_GRAPH

#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <any>
#include <memory>
#include <vector>
#include <variant>
#include <qmat/QMatAll>
#include <typeinfo>

#include <optional>

#include "libs/delta-crdts.cc"
#include "fast_rtps/dsrparticipant.h"
#include "fast_rtps/dsrpublisher.h"
#include "fast_rtps/dsrsubscriber.h"
#include "topics/DSRGraphPubSubTypes.h"
#include "vertex.h"
#include "inner_api.h"

#define NO_PARENT -1
#define TIMEOUT 5000

// Overload pattern used inprintVisitor
template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

namespace CRDT
{

    using N = Node;
    using Nodes = ormap<int, aworset<N,  int >, int>;
    using MTypes = std::variant<std::string, std::int32_t, float , std::vector<float>, RMat::RTMat>;
    using IDType = std::int32_t;
    using AttribsMap = std::unordered_map<std::string, MTypes>;
    using VertexPtr = std::shared_ptr<Vertex>;
    struct pair_hash
    {
        template <class T1, class T2>
        std::size_t operator() (const std::pair<T1, T2> &pair) const
        {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
        }
    };

    /////////////////////////////////////////////////////////////////
    /// DSR Exceptions
    /////////////////////////////////////////////////////////////////
    class DSRException : std::runtime_error
    {
        public:
            explicit DSRException(const std::string &message): std::runtime_error(buildMsg(message)){};
        private:
        std::string buildMsg(const std::string &message)
        {
            std::ostringstream buffer;
            buffer << "DSRException: " << message;
            return buffer.str();
        };
    };


    /////////////////////////////////////////////////////////////////
    /// CRDT API
    /////////////////////////////////////////////////////////////////

    class CRDTGraph : public QObject
    {
        Q_OBJECT
    public:
        size_t size();
        CRDTGraph(int root, std::string name, int id, std::string dsr_input_file = std::string());
        ~CRDTGraph();

        // threads
        bool start_fullgraph_request_thread();
        void start_fullgraph_server_thread();
        void start_subscription_thread(bool showReceived);

        //////////////////////////////////////////////////////
        ///  Graph API
        //////////////////////////////////////////////////////

        // Utils
        //void read_from_file(const std::string &xml_file_path);
        void read_from_json_file(const std::string &json_file_path);
        void write_to_json_file(const std::string &json_file_path);
        bool empty(const int &id);
        void print();
        std::tuple<std::string, std::string, int> nativetype_to_string(const MTypes &t); //Used by viewer
        std::map<long, Node> getCopy() const;   
        std::vector<long> getKeys() const ;   
          
        // not working yet
        typename std::map<int, aworset<N,int>>::const_iterator begin() const { return nodes.getMap().begin(); };
        typename std::map<int, aworset<N,int>>::const_iterator end() const { return nodes.getMap().end(); };

        // Innermodel API
        std::unique_ptr<InnerAPI> get_inner_api() { return std::make_unique<InnerAPI>(this); };

        // Nodes
        std::optional<Node> get_node(const std::string& name);
        std::optional<Node> get_node(int id);
        std::optional<VertexPtr> get_vertex(const std::string& name);
        std::optional<VertexPtr> get_vertex(int id);
        bool insert_or_assign_node(const N &node);
        bool delete_node(const std::string &name);
        bool delete_node(int id);
        std::vector<Node> get_nodes_by_type(const std::string& type);
        std::optional<std::string> get_name_from_id(std::int32_t id);  // caché
        std::optional<int> get_id_from_name(const std::string &name);  // caché
        
        // to be moved to Vertex //////////////////////////////////
        //std::int32_t get_node_level(Node& n);
        //std::string> get_node_type(Node& n);

        void add_attrib(std::map<string, Attrib> &v, std::string att_name, CRDT::MTypes att_value);
        template <typename T, typename = std::enable_if_t<std::is_same<Node,  T>::value || std::is_same<Edge, T>::value ,T >  >
        std::optional<Attrib> get_attrib_by_name_(const T& n, const std::string &key)
        {
            auto attrs = n.attrs();
            auto value  = attrs.find(key);
            if (value != attrs.end())
                return value->second;
            else{
                if constexpr (std::is_same<Node,  T>::value)
                    std::cout << "ERROR: " << __FILE__ << " " << __FUNCTION__ << ":" << __LINE__ << " "
                            << "-> " << n.id() << std::endl;
                if constexpr (std::is_same<Attrib,  T>::value)
                    std::cout << "ERROR: " << __FILE__ << " " << __FUNCTION__ << ":" << __LINE__ << " "
                            << "-> " << n.to() << std::endl;
            }
            return {};
        }
        template <typename Ta, typename Type, typename =  std::enable_if_t<std::is_same<Node,  Type>::value || std::is_same<Edge, Type>::value, Type>>
        std::optional<Ta> get_attrib_by_name(Type& n, const std::string &key) {
            std::optional<Attrib> av = get_attrib_by_name_(n, key);
            if (!av.has_value()) return {};
            if constexpr (std::is_same<Ta, std::string>::value) {
                return av.value().value().str();
            }
            if constexpr (std::is_same<Ta, std::int32_t>::value){
                return av.value().value().dec();
            }
            if constexpr (std::is_same<Ta, float>::value) {
                return av.value().value().fl();
            }
            if constexpr (std::is_same<Ta, std::vector<float>>::value)
            {
                return av.value().value().float_vec();
            }
            if constexpr (std::is_same<Ta, RMat::RTMat>::value) {
                return RTMat {  n.attrs()["rot"].value().float_vec()[0],  n.attrs()["rot"].value().float_vec()[1],  n.attrs()["rot"].value().float_vec()[2],
                                n.attrs()["trans"].value().float_vec()[0], n.attrs()["trans"].value().float_vec()[1], n.attrs()["trans"].value().float_vec()[2]      } ;
            }
        }

        //Edges
        std::optional<Edge> get_edge(const std::string& from, const std::string& to, const std::string& key);
        std::optional<Edge> get_edge(int from, int to, const std::string& key);
        bool insert_or_assign_edge(const Edge& attrs);
        bool delete_edge(const std::string& from, const std::string& t, const std::string& key);
        bool delete_edge(int from, int t, const std::string& key);
        std::vector<Edge> get_edges_by_type(const std::string& type);
        std::vector<Edge> get_edges_to_id(int id);
        std::optional<std::map<EdgeKey, Edge>> get_edges(int id) 
        { 
            std::optional<Node> n = get_node(id);
            return n.has_value() ?  std::optional<std::map<EdgeKey, Edge>>(n.value().fano()) : std::nullopt;
        };
        bool insert_or_assign_edge(const Node& n);   // TO IMPLEMENT

        //////////////////////////////////////////////////////
        ///  Viewer
        //////////////////////////////////////////////////////
        //Nodes get();

        /*
        template <typename Ta>
        Ta string_to_nativetype(const std::string &name, const std::string &val) {
            return std::get<Ta>(string_to_mtypes(name, val));
        }
        */
       
        //void add_edge_attribs(vector<EdgeAttribs> &v, EdgeAttribs& ea);

        //////////////////////////////////////////////////////


        //For testing

        void reset() {
            nodes.reset();
            deleted.clear();
            name_map.clear();
            id_map.clear();
            edges.clear();
            edgeType.clear();
            nodeType.clear();
        }

        //For debug
        int count = 0;

    private:
        Nodes nodes;
        int graph_root;
        bool work;
        mutable std::shared_mutex _mutex;
        std::string filter;
        std::string agent_name;
        const int agent_id;

        //////////////////////////////////////////////////////////////////////////
        // Cache maps
        ///////////////////////////////////////////////////////////////////////////
        std::unordered_set<int> deleted;     // deleted nodes, used to avoid insertion after remove.
        std::unordered_map<string, int> name_map;     // mapping between name and id of nodes.
        std::unordered_map<int, string> id_map;       // mapping between id and name of nodes.
        std::unordered_map<pair<int, int>, std::unordered_set<std::string>, pair_hash> edges;      // collection with all graph edges. ((from, to), key)
        std::unordered_map<std::string, std::unordered_set<pair<int, int>, pair_hash>> edgeType;  // collection with all edge types.
        std::unordered_map<std::string, std::unordered_set<int>> nodeType;  // collection with all node types.
        void update_maps_node_delete(int id, const Node& n);
        void update_maps_node_insert(int id, const Node& n);
        void update_maps_edge_delete(int from, int to, const std::string& key);

        //////////////////////////////////////////////////////////////////////////
        // Non-blocking graph operations
        //////////////////////////////////////////////////////////////////////////
        std::optional<N> get(int id);
        bool in(const int &id);
        std::optional<N> get_(int id);
        bool insert_or_assign_node_(const N &node);
        std::pair<bool, vector<tuple<int, int, std::string>>> delete_node_(int id);
        bool delete_edge_(int from, int t, const std::string& key);
        std::optional<Edge> get_edge_(int from, int to, const std::string& key);

        int id();
        DotContext context();
        std::map<int, AworSet> Map();

        void join_delta_node(AworSet aworSet);
        void join_full_graph(OrMap full_graph);

        class NewMessageFunctor
        {
            public:
                CRDTGraph *graph;
                bool *work;
                std::function<void(eprosima::fastrtps::Subscriber *sub, bool *work, CRDT::CRDTGraph *graph)> f;

                NewMessageFunctor(CRDTGraph *graph_, bool *work_,
                                std::function<void(eprosima::fastrtps::Subscriber *sub, bool *work, CRDT::CRDTGraph *graph)> f_)
                        : graph(graph_), work(work_), f(std::move(f_)){}

                NewMessageFunctor() = default;


                void operator()(eprosima::fastrtps::Subscriber *sub) { f(sub, work, graph); };
        };

        // Threads handlers
        void subscription_thread(bool showReceived);
        void fullgraph_server_thread();
        bool fullgraph_request_thread();

        // Translators
        AworSet translateAwCRDTtoIDL(int id, aworset<N, int> &data);
        aworset<N, int> translateAwIDLtoCRDT(AworSet &data);

        // RTSP participant
        DSRParticipant dsrparticipant;
        DSRPublisher dsrpub;
        DSRSubscriber dsrsub;
        NewMessageFunctor dsrpub_call;

        DSRSubscriber dsrsub_graph_request;
        DSRPublisher dsrpub_graph_request;
        NewMessageFunctor dsrpub_graph_request_call;

        DSRSubscriber dsrsub_request_answer;
        DSRPublisher dsrpub_request_answer;
        NewMessageFunctor dsrpub_request_answer_call;

    signals:                                                                  // for graphics update
        void update_node_signal(const std::int32_t, const std::string &type); // Signal to update CRDT

        void update_attrs_signal(const std::int32_t &id, const std::map<string, Attrib> &attribs); //Signal to show node attribs.
        void update_edge_signal(const std::int32_t from, const std::int32_t to, const std::string& type);                   // Signal to show edge attribs.

        void del_edge_signal(const std::int32_t from, const std::int32_t to, const std::string &edge_tag); // Signal to del edge.
        void del_node_signal(const std::int32_t from);                                                     // Signal to del node.
    };
} // namespace CRDT

#endif
