/*
 *    Copyright (C) 2020 by Mohamed Shawky
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
SpecificWorker::SpecificWorker(TuplePrx tprx, bool startup_check) : GenericWorker(tprx)
{
    this->startup_check_flag = startup_check;
    QLoggingCategory::setFilterRules("*.debug=false\n");
}

/**
* \brief Default destructor
*/
SpecificWorker::~SpecificWorker()
{
    std::cout << "Destroying SpecificWorker" << std::endl;
    G->write_to_json_file("./"+agent_name+".json");
    G.reset();
}

bool SpecificWorker::setParams(RoboCompCommonBehavior::ParameterList params)
{
    agent_name = params["agent_name"].value;
    agent_id = stoi(params["agent_id"].value);
    tree_view = params["tree_view"].value == "true";
    graph_view = params["graph_view"].value == "true";
    qscene_2d_view = params["2d_view"].value == "true";
    osg_3d_view = params["3d_view"].value == "true";

    return true;
}

void SpecificWorker::initialize(int period)
{
    std::cout << "Initialize worker" << std::endl;
    this->Period = period;
    if(this->startup_check_flag)
    {
        this->startup_check();
    }
    else
    {
        timer.start(Period);
        // create graph
        G = std::make_shared<DSR::DSRGraph>(0, agent_name, agent_id, "", dsrgetid_proxy); // Init nodes
        std::cout<< __FUNCTION__ << "Graph loaded" << std::endl;  

        // Graph viewer
        using opts = DSR::DSRViewer::view;
        int current_opts = 0;
        opts main = opts::none;
        if(tree_view)
        {
            current_opts = current_opts | opts::tree;
        }
        if(graph_view)
        {
            current_opts = current_opts | opts::graph;
            main = opts::graph;
        }
        if(qscene_2d_view)
        {
            current_opts = current_opts | opts::scene;
        }
        if(osg_3d_view)
        {
            current_opts = current_opts | opts::osg;
        }
        graph_viewer = std::make_unique<DSR::DSRViewer>(this, G, current_opts, main);
        graph_viewer->add_custom_widget_to_dock("Grasping", &custom_widget); // custom_widget

        setWindowTitle(QString::fromStdString(agent_name + "-") + QString::number(agent_id));

        objects_pcl = this->read_pcl_from_file();

        this->Period = period;
        timer.start(Period);
    }
}

void SpecificWorker::compute()
{
    // read RGBD image from graph
    RoboCompCameraRGBDSimple::TImage rgb = get_rgb_from_G();
    RoboCompCameraRGBDSimple::TDepth depth = get_depth_from_G();

    // cast RGB image to OpenCV Mat
    cv::Mat img = cv::Mat(rgb.height, rgb.width, CV_8UC3, &rgb.image[0]);

    // get grasp object from QT widget
    QAbstractButton* sel_button = custom_widget.object_sel_group->checkedButton();
    if (sel_button)
    {
        QString button_text = sel_button->text();
        grasp_object = button_text.toStdString();
    }

    // call pose estimation on RGBD and receive estimated poses
    RoboCompObjectPoseEstimationRGBD::PoseType poses;
    try
    {
        poses = this->objectposeestimationrgbd_proxy->getObjectPose(rgb, depth);
    }
    catch (const Ice::Exception &e)
    {
        std::cout << e << " No RoboCompPoseEstimation component found" << std::endl;
    }

    if (poses.size() != 0)
    {
        // display RGB image on QT widget
        show_image(img, poses);

        // inject estimated poses into graph
        this->inject_estimated_poses(poses);

        // get arm target and required object poses
        auto world_node = G->get_node("world");
        auto arm_id = G->get_id_from_name("viriato_arm_target");
        auto object_id = G->get_id_from_name(grasp_object);
        
        auto world_arm_edge = G->get_edge_RT(world_node.value(), arm_id.value());
        auto world_object_edge = G->get_edge_RT(world_node.value(), object_id.value());
        
        std::map<std::string, DSR::Attribute> arm_attribs = world_arm_edge.value().attrs();
        vector<float> arm_trans = arm_attribs.at("translation").float_vec();
        
        std::map<std::string, DSR::Attribute> object_attribs = world_object_edge.value().attrs();
        vector<float> object_trans = object_attribs.at("translation").float_vec();

        // get euclidean distance between arm and required object (ignoring distance along z-axis)
        float arm_object_dist = sqrt(pow(object_trans.at(0)-arm_trans.at(0), 2.0) + pow(object_trans.at(1)-arm_trans.at(1), 2.0));

        // check whether required object is within arm's reach
        if (arm_object_dist >= 0.5)
        {
            // plan a dummy target closer to the object (planning is done on multiple stages | factor = 0.2)
            vector<float> dummy_trans = this->interpolate_trans(arm_trans, object_trans, 0.2); // interpolate dummy target position
            vector<float> dummy_rot = object_attribs.at("rotation_euler_xyz").float_vec(); // set dummy target rotation with object rotation
            G->insert_or_assign_edge_RT(world_node.value(), arm_id.value(), dummy_trans, dummy_rot);

            // check whether the arm target reaches the object
            if (dummy_trans == object_trans)
            {
                std::cout << "The arm has reached the target object" << std::endl;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////
//                     G read utilities
/////////////////////////////////////////////////////////////////

RoboCompCameraRGBDSimple::TImage SpecificWorker::get_rgb_from_G()
{
    // get head camera node
    auto cam = G->get_node("viriato_head_camera_sensor");
    if (cam.has_value())
    {
        // read RGB data attributes from graph 
        RoboCompCameraRGBDSimple::TImage rgb;
        try
        {
            auto rgb_data = G->get_rgb_image(cam.value());
            const auto width = G->get_attrib_by_name<rgb_width>(cam.value());
            const auto height = G->get_attrib_by_name<rgb_height>(cam.value());
            const auto depth = G->get_attrib_by_name<rgb_depth>(cam.value());
            const auto cam_id = G->get_attrib_by_name<rgb_cameraID>(cam.value());
            const auto focalx = G->get_attrib_by_name<rgb_focalx>(cam.value());
            const auto focaly = G->get_attrib_by_name<rgb_focaly>(cam.value());
            const auto alivetime = G->get_attrib_by_name<rgb_alivetime>(cam.value());

            // assign attributes to RoboCompCameraRGBDSimple::TImage
            rgb.image = rgb_data.value().get();
            rgb.width = width.value();
            rgb.height = height.value();
            rgb.depth = depth.value();
            rgb.cameraID = cam_id.value();
            rgb.focalx = focalx.value();
            rgb.focaly = focaly.value();
            rgb.alivetime = alivetime.value();

            return rgb;
        }
        catch (const std::exception &e)
        {
            std::cout << __FILE__ << __FUNCTION__ << __LINE__ << " " << e.what() << std::endl;
            std::terminate();
        }
    }
    else
    {
        qFatal("Terminate in Compute. No node rgbd found");
    }
}

RoboCompCameraRGBDSimple::TDepth SpecificWorker::get_depth_from_G()
{
    // get head camera node
    auto cam = G->get_node("viriato_head_camera_sensor");
    if (cam.has_value())
    {
        // read depth data attributes from graph
        RoboCompCameraRGBDSimple::TDepth depth;
        try
        {
            auto depth_data = G->get_attrib_by_name<img_depth_att>(cam.value());
            const auto width = G->get_attrib_by_name<depth_width>(cam.value());
            const auto height = G->get_attrib_by_name<depth_height>(cam.value());
            const auto cam_id = G->get_attrib_by_name<depth_cameraID>(cam.value());
            const auto focalx = G->get_attrib_by_name<rgb_focalx>(cam.value());
            const auto focaly = G->get_attrib_by_name<rgb_focaly>(cam.value());
            const auto depth_factor = G->get_attrib_by_name<depthFactor>(cam.value());
            const auto alivetime = G->get_attrib_by_name<rgb_alivetime>(cam.value());

            // assign attributes to RoboCompCameraRGBDSimple::TDepth
            depth.depth = depth_data.value();
            depth.width = width.value();
            depth.height = height.value();
            depth.cameraID = cam_id.value();
            depth.focalx = focalx.value();
            depth.focaly = focaly.value();
            depth.depthFactor = depth_factor.value(); // set to 0.1 for viriato_head_camera_sensor
            depth.alivetime = alivetime.value();

            return depth;
        }
        catch(const std::exception& e)
        {
            std::cout << __FILE__ << __FUNCTION__ << __LINE__ << " " << e.what() << std::endl;
            std::terminate();
        }
    }
    else
    {
        qFatal("Terminate in Compute. No node rgbd found");
    }
}

/////////////////////////////////////////////////////////////////
//                     G injection utilities
/////////////////////////////////////////////////////////////////

void SpecificWorker::inject_estimated_poses(RoboCompObjectPoseEstimationRGBD::PoseType poses)
{
    // get innermodel sub-API
    auto innermodel = G->get_inner_api();
    // get a copy of world node
    auto world = G->get_node("world");
    // loop over each estimated object pose
    for (auto pose : poses)
    {
        if (pose.objectname.compare(grasp_object) == 0)
        {
            // convert quaternions into euler angles
            vector<float> quat{pose.qx, pose.qy, pose.qz, pose.qw};
            vector<float> angles = this->quat_to_euler(quat);

            // re-project estimated poses into world coordinates
            QVec orig_point = QVec(6);
            orig_point.setItem(0, pose.x);
            orig_point.setItem(1, pose.y);
            orig_point.setItem(2, pose.z);
            orig_point.setItem(3, angles.at(0));
            orig_point.setItem(4, angles.at(1));
            orig_point.setItem(5, angles.at(2));
            auto final_pose = innermodel->transform("world", orig_point, "camera_pose");

            // get object node id (if exists)
            auto id = G->get_id_from_name(pose.objectname);

            // check whether object node already exists
            auto object_node = G->get_node(pose.objectname);
            if (!object_node.has_value()) // if node doesn't exist
            {
                // define object node
                DSR::Node object = DSR::Node();
                object.type("mesh");
                object.agent_id(agent_id);
                object.name(pose.objectname);

                // inject object node into graph
                id = G->insert_node(object);

                // check whether node is inserted or not
                if (id.has_value())
                {
                    std::cout << "Node inserted successfully -> " << id.value() << ":" << G->get_name_from_id(id.value()).value() << std::endl;
                }
                else
                {
                    std::cout << "Failed to insert node!" << std::endl;
                }
            }

            // inject estimated object pose into graph
            vector<float> trans{static_cast<float>(final_pose->x()), static_cast<float>(final_pose->y()), static_cast<float>(final_pose->z())};
            vector<float> rot{static_cast<float>(final_pose->rx()), static_cast<float>(final_pose->ry()), static_cast<float>(final_pose->rz())};
            G->insert_or_assign_edge_RT(world.value(), id.value(), trans, rot);

            // ignore rest of objects
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////
//                     Geometry utilities
/////////////////////////////////////////////////////////////////

vector<float> SpecificWorker::quat_to_euler(vector<float> quat)
{
    // euler angles vector
    vector<float> angles;

    // roll (x-axis rotation)
    double sinr_cosp = 2 * (quat.at(3) * quat.at(0) + quat.at(1) * quat.at(2));
    double cosr_cosp = 1 - 2 * ( quat.at(0) *  quat.at(0) +  quat.at(1) *  quat.at(1));
    angles.push_back(std::atan2(sinr_cosp, cosr_cosp));

    // pitch (y-axis rotation)
    double sinp = 2 * (quat.at(3) * quat.at(1) - quat.at(2) * quat.at(0));
    if (std::abs(sinp) >= 1)
        angles.push_back(std::copysign(M_PI / 2, sinp)); // use 90 degrees if out of range
    else
        angles.push_back(std::asin(sinp));

    // yaw (z-axis rotation)
    double siny_cosp = 2 * (quat.at(3) * quat.at(2) + quat.at(0) * quat.at(1));
    double cosy_cosp = 1 - 2 * (quat.at(1) * quat.at(1) + quat.at(2) * quat.at(2));
    angles.push_back(std::atan2(siny_cosp, cosy_cosp));

    return angles;
}

vector<float> SpecificWorker::interpolate_trans(vector<float> src, vector<float> dest, float factor)
{
    // interpolate between the source and destination positions with the given factor
    float interp_x = src.at(0) + (dest.at(0)-src.at(0)) * factor;
    float interp_y = src.at(1) + (dest.at(1)-src.at(1)) * factor;
    float interp_z = src.at(2) + (dest.at(2)-src.at(2)) * factor;
    vector<float> interp_trans{interp_x, interp_y, interp_z};

    float final_pose_dist = sqrt(pow(dest.at(0)-interp_trans.at(0), 2.0) + 
                                pow(dest.at(1)-interp_trans.at(1), 2.0) + 
                                pow(dest.at(2)-interp_trans.at(2), 2.0));

    if (final_pose_dist <= 0.01)
    {
        return dest;
    }

    return interp_trans;
}

/////////////////////////////////////////////////////////////////
//                     IO utilities
/////////////////////////////////////////////////////////////////

std::map<std::string, vector<vector<float>>> SpecificWorker::read_pcl_from_file()
{
    std::vector<std::string> filenames;
    std::map<std::string, std::vector<std::vector<float>>> data;

    if(boost::filesystem::is_directory("objects-pcl"))
    {
        for(auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator("objects-pcl"), {}))
        {
            filenames.push_back(entry.path().string());
        }
    }

    for (auto filename : filenames)
    {
        std::ifstream file(filename);
        std::string line;
        std::vector<std::vector<float>> pcl;

        while (std::getline(file, line))
        {
            float value;
            std::stringstream ss(line);
            std::vector<float> point;

            while (ss >> value)
            {
                point.push_back(value);
            }
            
            pcl.push_back(point);
        }

        data.insert({filename.substr(12, filename.size()-16), pcl});
    }

    return data;
}

/////////////////////////////////////////////////////////////////
//                     Display utilities
/////////////////////////////////////////////////////////////////

void SpecificWorker::show_image(cv::Mat &img, RoboCompObjectPoseEstimationRGBD::PoseType poses)
{
    // TODO : draw the DNN-estimated poses on the displayed image
    auto pix = QPixmap::fromImage(QImage(img.data, img.cols, img.rows, QImage::Format_RGB888));
    custom_widget.rgb_image->setPixmap(pix);
}

int SpecificWorker::startup_check()
{
    std::cout << "Startup check" << std::endl;
    QTimer::singleShot(200, qApp, SLOT(quit()));
    return 0;
}

/**************************************/
// From the RoboCompDSRGetID you can call this methods:
// this->dsrgetid_proxy->getID(...)

/**************************************/
// From the RoboCompObjectPoseEstimationRGBD you can call this methods:
// this->objectposeestimationrgbd_proxy->getObjectPose(...)

/**************************************/
// From the RoboCompObjectPoseEstimationRGBD you can use this types:
// RoboCompObjectPoseEstimationRGBD::ObjectPose
